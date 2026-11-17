/*
 * bsp_button.c — botão BOOT (GPIO0) por interrupção.
 *
 * Fluxo: aperto -> borda de descida no GPIO0 -> ISR -> manda 1 item na fila
 *        -> task acorda -> antirrebote -> chama o callback do app.
 *
 * Por que uma fila entre a ISR e a task? A ISR roda em contexto de interrupção:
 * tem que ser curtíssima e NÃO pode logar, alocar memória nem falar I2C. Então
 * ela só "deposita um aviso" na fila e sai. O trabalho de verdade (logar, etc.)
 * acontece na task, em contexto normal.
 */
#include "bsp_button.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "bsp_button";

#define BOTAO_GPIO     GPIO_NUM_0        // BOOT (ver docs/HARDWARE.md)
#define DEBOUNCE_MS    30                // tempo p/ o contato "assentar" antes de ler o nível
#define FILA_TAM       8                 // guarda as bordas do repique até a task processá-las

static QueueHandle_t   s_fila = NULL;    // ponte ISR -> task
static bsp_button_cb_t s_cb   = NULL;    // o que fazer a cada clique

// ISR: roda em contexto de interrupção. Regra de ouro: o MÍNIMO possível.
// Nada de ESP_LOG, malloc ou I2C aqui. Só avisa a task pela fila e sai.
// Dispara nas DUAS bordas (aperto E soltura) — a task decide o que é o quê
// lendo o nível real do pino.
// IRAM_ATTR: o código da ISR fica na RAM interna (rápida e sempre acessível),
// não na flash — exigência para rotinas de interrupção.
static void IRAM_ATTR isr_botao(void *arg)
{
    uint32_t ev = 1;                     // conteúdo não importa; vale o "chegou algo"
    BaseType_t acordou = pdFALSE;
    // Versão "...FromISR" da função de fila: a única que pode ser usada na ISR.
    xQueueSendFromISR(s_fila, &ev, &acordou);
    // Se a task do botão estava bloqueada esperando, troca para ela já ao sair
    // da interrupção (em vez de esperar o próximo tick do escalonador).
    if (acordou) {
        portYIELD_FROM_ISR();
    }
}

// Task do botão: fica bloqueada na fila até a ISR avisar. Aqui já é contexto
// normal, então pode logar e chamar o callback do aplicativo.
//
// Antirrebote por NÍVEL (máquina de estado), não por tempo-desde-o-último:
// ao soltar o botão o contato também "treme" e gera bordas espúrias; com o
// método antigo (janela de 200 ms desde o aperto), soltar depois de ~1 s caía
// FORA da janela e virava um 2º clique. Aqui, a cada borda esperamos o contato
// assentar (DEBOUNCE_MS) e lemos o nível real do pino: só contamos clique na
// transição SOLTO->PRESSIONADO. Soltar nunca gera clique, não importa o tempo.
static void task_botao(void *arg)
{
    uint32_t ev;
    bool pressionado = false;            // estado JÁ confirmado (false = solto)

    while (1) {
        // Dorme (sem gastar CPU) até a ISR colocar algo na fila.
        if (xQueueReceive(s_fila, &ev, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        // Espera o repique passar antes de decidir pelo nível do pino. As
        // bordas que chegarem durante esse tempo ficam na fila e serão
        // processadas em seguida — mas aí o pino já estará estável, então
        // lerão o mesmo nível e não mudarão o estado (idempotente).
        vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_MS));

        // BOOT: nível baixo = pressionado (pull-up em repouso leva a alto).
        bool nivel_baixo = (gpio_get_level(BOTAO_GPIO) == 0);

        if (nivel_baixo && !pressionado) {
            pressionado = true;          // transição solto -> pressionado = 1 clique
            if (s_cb) {
                s_cb();
            }
        } else if (!nivel_baixo && pressionado) {
            pressionado = false;         // soltou: só atualiza o estado, sem clique
        }
        // nível igual ao estado conhecido -> repique, ignora
    }
}

esp_err_t bsp_button_init(bsp_button_cb_t cb)
{
    s_cb = cb;

    // Fila com poucos itens: só nos interessa saber que houve aperto.
    s_fila = xQueueCreate(FILA_TAM, sizeof(uint32_t));
    if (s_fila == NULL) {
        return ESP_ERR_NO_MEM;
    }

    // GPIO0 como entrada. Pull-up interno: em repouso o pino fica em alto;
    // apertar o BOOT liga ao GND e leva a baixo. Interrupção nas DUAS bordas
    // (aperto e soltura): a task lê o nível real para decidir o que foi e para
    // tratar o repique da soltura (ver task_botao).
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << BOTAO_GPIO),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_ANYEDGE,
    };
    esp_err_t err = gpio_config(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio_config falhou: %s", esp_err_to_name(err));
        return err;
    }

    // Serviço de ISR de GPIO: instala uma vez para o app todo. Se outro módulo
    // já instalou, o IDF devolve ESP_ERR_INVALID_STATE — aqui isso é esperado
    // e não é erro (seguimos adiante).
    err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "install_isr_service falhou: %s", esp_err_to_name(err));
        return err;
    }

    // Liga a nossa ISR especificamente ao GPIO0.
    err = gpio_isr_handler_add(BOTAO_GPIO, isr_botao, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "isr_handler_add falhou: %s", esp_err_to_name(err));
        return err;
    }

    // Task pequena: só espera a fila e chama o callback. Stack 3072 é folgado
    // para logar; prioridade 5 = acima do laço ocioso, abaixo de tarefas críticas.
    if (xTaskCreate(task_botao, "btn", 3072, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "xTaskCreate falhou");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "botão BOOT (GPIO%d) pronto (interrupção nas 2 bordas + antirrebote %d ms)",
             BOTAO_GPIO, DEBOUNCE_MS);
    return ESP_OK;
}
