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
#include "esp_timer.h"
#include "esp_log.h"

static const char *TAG = "bsp_button";

#define BOTAO_GPIO     GPIO_NUM_0        // BOOT (ver docs/HARDWARE.md)
#define DEBOUNCE_US    (200 * 1000)      // 200 ms: janela que ignora repiques do contato
#define FILA_TAM       4                 // poucos itens bastam: só precisamos de "houve aperto"

static QueueHandle_t   s_fila = NULL;    // ponte ISR -> task
static bsp_button_cb_t s_cb   = NULL;    // o que fazer a cada clique

// ISR: roda em contexto de interrupção. Regra de ouro: o MÍNIMO possível.
// Nada de ESP_LOG, malloc ou I2C aqui. Só avisa a task pela fila e sai.
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
static void task_botao(void *arg)
{
    uint32_t ev;
    int64_t ultimo_us = 0;               // quando aceitamos o último clique

    while (1) {
        // Dorme (sem gastar CPU) até a ISR colocar algo na fila.
        if (xQueueReceive(s_fila, &ev, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        // Antirrebote por tempo: o contato "treme" e gera várias bordas em
        // poucos ms. Só aceitamos um clique se já passou a janela desde o
        // anterior; os repiques dentro da janela são descartados.
        int64_t agora_us = esp_timer_get_time();
        if (agora_us - ultimo_us < DEBOUNCE_US) {
            continue;
        }
        ultimo_us = agora_us;

        if (s_cb) {
            s_cb();
        }
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
    // apertar o BOOT liga ao GND e leva a baixo. Interrupção na borda de
    // descida = exatamente o instante do aperto (não trata a soltura).
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << BOTAO_GPIO),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_NEGEDGE,
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

    ESP_LOGI(TAG, "botão BOOT (GPIO%d) pronto (interrupção na borda de descida)",
             BOTAO_GPIO);
    return ESP_OK;
}
