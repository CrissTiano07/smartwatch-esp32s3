/*
 * main.c — I2C + AXP2101 (bateria) + RTC PCF85063 + tela (LVGL).
 *
 * Fase 3 (Passo 4): mostra HH:MM na tela, vindo do RTC, no horário de
 * Fortaleza (UTC-3), atualizando quando o minuto vira. A hora interna
 * continua em UTC; o fuso entra só na exibição. Bateria e RTC seguem no log.
 */

#include <inttypes.h>            // PRIu32/PRIu64: imprimir inteiros de tamanho fixo no printf
#include <stdlib.h>              // setenv: define o fuso (TZ) para a exibição
#include <time.h>                // struct tm, timegm, localtime_r, strftime
#include "freertos/FreeRTOS.h"   // sistema operacional de tempo real do ESP32 (sempre antes de task.h)
#include "freertos/task.h"       // vTaskDelay: pausar sem travar o processador
#include "freertos/semphr.h"     // mutex que serializa as transições de energia da tela
#include "esp_log.h"             // ESP_LOGI/ESP_LOGW/ESP_LOGE: log com nível e etiqueta
#include "esp_chip_info.h"       // esp_chip_info: modelo e núcleos do chip
#include "esp_flash.h"           // esp_flash_get_size: tamanho da memória flash
#include "esp_system.h"          // esp_get_free_heap_size, esp_get_idf_version
#include "esp_timer.h"           // esp_timer_get_time: microssegundos desde o boot
#include "esp_psram.h"           // esp_psram_get_size: tamanho da PSRAM detectada
#include "esp_heap_caps.h"       // heap_caps_get_free_size: heap livre por tipo (interno/PSRAM)
#include "bsp_i2c.h"             // nosso barramento I2C compartilhado
#include "axp2101.h"             // nosso driver do chip de energia
#include "pcf85063.h"            // nosso driver do RTC (hora)
#include "bsp_display.h"         // nossa tela AMOLED (CO5300/QSPI)
#include "bsp_button.h"          // nosso botão físico BOOT (GPIO0)

// Etiqueta que aparece no começo de cada linha de log: "I (123) main: ..."
// Ajuda a saber de qual módulo veio a mensagem quando o projeto crescer.
static const char *TAG = "main";

// --- Estado de energia da tela (Fase 4) ---
// O tempo sem interação até apagar vem do menuconfig
// (CONFIG_SW_TELA_TIMEOUT_MS, padrão 5000 ms — ver main/Kconfig.projbuild).

// Flags preenchidas no boot (se cada subsistema subiu). Ficam em escopo de
// arquivo porque o laço principal E o callback do botão as usam.
static bool s_tela_ok = false;
static bool s_rtc_ok  = false;
static int  s_ultimo_min = -1;             // último minuto mostrado (-1 = nenhum)

// A tela está dormindo? E quando foi a última atividade (microssegundos desde
// o boot)? 'volatile' porque são lidas/escritas por tarefas diferentes (o laço
// principal, a task do botão e a task do esp_timer).
static volatile bool    s_tela_dormindo = false;
static volatile int64_t s_ultima_atividade_us = 0;

// Serializa as transições de energia da tela (apagar/acender), que podem ser
// pedidas por tarefas diferentes ao mesmo tempo (timer = apagar, botão = acordar).
static SemaphoreHandle_t s_power_mux = NULL;

// Temporizador de DISPARO ÚNICO (one-shot) que apaga a tela. É reiniciado a
// cada atividade, então a tela apaga EXATAMENTE no timeout após o último
// clique (preciso, ao contrário de checar no laço de 5 s).
static esp_timer_handle_t s_timer_tela = NULL;

// Marca "houve atividade agora": guarda o instante (para a trava anti-corrida
// no callback do timer) e REINICIA o temporizador one-shot.
static void registrar_atividade(void)
{
    s_ultima_atividade_us = esp_timer_get_time();
    if (s_timer_tela == NULL) {
        return;
    }
    (void)esp_timer_stop(s_timer_tela);  // se não estava rodando, devolve erro — ok ignorar
    esp_timer_start_once(s_timer_tela, (uint64_t)CONFIG_SW_TELA_TIMEOUT_MS * 1000);
}

// Converte uma data do calendário (UTC) em "epoch" = segundos desde
// 1970-01-01. É o inverso do gmtime. A newlib do ESP-IDF não traz timegm(),
// então calculamos aqui. A conta dos DIAS usa um algoritmo conhecido (civil
// days, de Howard Hinnant) que já lida com anos bissextos corretamente.
static time_t epoch_de_utc(int ano, int mes, int dia,
                           int hora, int min, int seg)
{
    // Trata jan/fev como meses 13/14 do ano anterior (simplifica o bissexto).
    ano -= (mes <= 2);
    long era = (ano >= 0 ? ano : ano - 399) / 400;
    unsigned yoe = (unsigned)(ano - era * 400);                 // ano dentro da "era" (0..399)
    unsigned doy = (153 * (mes + (mes > 2 ? -3 : 9)) + 2) / 5 + dia - 1; // dia do ano
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;       // dia dentro da era
    long dias = era * 146097 + (long)doe - 719468;             // dias desde 1970-01-01
    return (time_t)(((dias * 24 + hora) * 60 + min) * 60 + seg);
}

// Converte a hora do RTC (que é SEMPRE UTC) para o horário de Fortaleza e
// escreve "HH:MM" em 'buf'. Devolve o minuto local (0..59) para sabermos
// quando o minuto virou. A regra do projeto é: hora interna em UTC, fuso só
// na exibição — por isso a conversão acontece aqui, na hora de mostrar.
//
// epoch_de_utc(): monta o "epoch" a partir dos campos UTC do RTC.
// localtime_r(): converte o epoch de volta para hora local, aplicando o TZ
// que configuramos no app_main ("<-03>3" = UTC-3, Fortaleza, sem verão).
static int hhmm_fortaleza(const pcf85063_hora_t *h, char *buf, size_t n)
{
    time_t epoch = epoch_de_utc(h->ano, h->mes, h->dia, h->hora, h->min, h->seg);
    struct tm local;
    localtime_r(&epoch, &local);
    strftime(buf, n, "%H:%M", &local);
    return local.tm_min;
}

// Atualiza o relógio HH:MM na tela a partir do RTC. 'forcar' ignora a checagem
// de "o minuto mudou" — usamos no wake, para repintar a hora imediatamente, já
// que o painel ignorou o que o LVGL desenhou enquanto dormia.
static void mostrar_hora(bool forcar)
{
    if (!s_rtc_ok || !s_tela_ok) {
        return;
    }
    pcf85063_hora_t h;
    if (pcf85063_ler(&h) != ESP_OK) {
        return;
    }
    char hhmm[6];  // "HH:MM" + terminador
    int min_local = hhmm_fortaleza(&h, hhmm, sizeof(hhmm));
    if (forcar || min_local != s_ultimo_min) {
        bsp_display_set_text(hhmm);
        s_ultimo_min = min_local;
    }
}

// Apaga a tela por inatividade. Idempotente e protegida por mutex (pode ser
// chamada de tarefas diferentes). bsp_display_sleep() já pausa o LVGL.
static void tela_dormir(void)
{
    if (s_power_mux == NULL) {
        return;
    }
    xSemaphoreTake(s_power_mux, portMAX_DELAY);
    if (!s_tela_dormindo && s_tela_ok) {
        bsp_display_sleep();
        s_tela_dormindo = true;
        ESP_LOGI(TAG, "tela: apagada por timeout");
    }
    xSemaphoreGive(s_power_mux);
}

// Acende a tela. Depois de acordar o painel e retomar o LVGL (dentro de
// bsp_display_wake), atualiza a hora e força repintar a tela inteira — o painel
// ignorou o desenho feito enquanto dormia.
static void tela_acordar(const char *motivo)
{
    if (s_power_mux == NULL) {
        return;
    }
    xSemaphoreTake(s_power_mux, portMAX_DELAY);
    if (s_tela_dormindo && s_tela_ok) {
        bsp_display_wake();
        s_tela_dormindo = false;
        mostrar_hora(true);        // repinta a hora atual já
        bsp_display_redraw();      // invalida a tela inteira (fundo + rótulo)
        ESP_LOGI(TAG, "tela: acesa (%s)", motivo);
    }
    xSemaphoreGive(s_power_mux);
}

// Callback do temporizador one-shot: dispara uma vez, CONFIG_SW_TELA_TIMEOUT_MS
// após a última atividade. Roda na task do esp_timer (contexto normal — pode
// mexer na tela). Trava anti-corrida: se um clique chegou quase junto com o
// disparo, ele já reiniciou o timer; conferimos pelo instante da última
// atividade e, se foi recente, não apagamos (deixamos o novo timer agir).
static void timer_apagar_cb(void *arg)
{
    int64_t inativo_us = esp_timer_get_time() - s_ultima_atividade_us;
    if (inativo_us < (int64_t)CONFIG_SW_TELA_TIMEOUT_MS * 1000 - 50000) {  // margem 50 ms
        return;
    }
    tela_dormir();
}

// Callback do botão BOOT. Roda na task do bsp_button (não na ISR), então é
// seguro logar e mexer na tela (inclusive os 120 ms de espera do wake). Cada
// clique conta como atividade (reinicia o timeout) e, se a tela estiver
// dormindo, acende. Com a tela acesa, basta reiniciar o timeout.
static void ao_clicar_botao(void)
{
    registrar_atividade();
    if (s_tela_dormindo) {
        tela_acordar("botão");
    }
    ESP_LOGI(TAG, "botão: clique");
}

// app_main é o "main()" do ESP-IDF: o sistema chama esta função depois do boot.
void app_main(void)
{
    ESP_LOGI(TAG, "boot ok");

    // --- Informações do chip: confirma que estamos falando com a placa certa ---
    esp_chip_info_t chip;
    esp_chip_info(&chip);  // preenche a struct 'chip' (passamos o endereço com &)
    ESP_LOGI(TAG, "chip: ESP32-S3, %d núcleos, revisão %d", chip.cores, chip.revision);

    // Tamanho da flash. A placa deve ter 32 MB (ver docs/HARDWARE.md).
    uint32_t flash_bytes = 0;
    // NULL = chip de flash principal. A função devolve ESP_OK (0) se deu certo.
    if (esp_flash_get_size(NULL, &flash_bytes) == ESP_OK) {
        ESP_LOGI(TAG, "flash: %" PRIu32 " MB", flash_bytes / (1024 * 1024));
    }

    ESP_LOGI(TAG, "ESP-IDF: %s", esp_get_idf_version());
    ESP_LOGI(TAG, "heap livre: %" PRIu32 " bytes", esp_get_free_heap_size());

    // PSRAM: confirma que os 8 MB do ESP32-S3R8 foram detectados (precisamos
    // dela para os buffers da tela na Fase 3). esp_psram_get_size() devolve 0
    // se a PSRAM não foi habilitada/encontrada no sdkconfig.
    ESP_LOGI(TAG, "PSRAM: %u bytes (%u MB)",
             (unsigned)esp_psram_get_size(),
             (unsigned)(esp_psram_get_size() / (1024 * 1024)));

    // --- I2C: barramento primeiro, depois quem está nele ---
    // Se algo falhar, NÃO reiniciamos: seguimos no laço para o erro
    // ficar visível no monitor (um reboot em loop esconderia a causa).
    bool axp_ok = false;
    if (bsp_i2c_init() == ESP_OK) {
        bsp_i2c_scan();
        axp_ok = (axp2101_init() == ESP_OK);

        // RTC: registra o chip e, se a hora estiver inválida (ou se a opção
        // RTC_FORCAR_SEMENTE estiver ligada), semeia com a hora de build.
        s_rtc_ok = (pcf85063_init() == ESP_OK);
        if (s_rtc_ok) {
            bool semeou = false;
            pcf85063_semear_se_preciso(&semeou);
        }
    }

    // --- Tela (Fase 3, Passo 4): relógio HH:MM do RTC ---
    // O painel sobe primeiro; depois o LVGL assume o desenho. O relógio em si
    // é atualizado no laço abaixo, quando o minuto vira. Não depende do AXP2101.
    s_tela_ok = (bsp_display_init() == ESP_OK && bsp_display_lvgl_init() == ESP_OK);
    if (s_tela_ok) {
        // Heap livre logo após subir o LVGL, separado por tipo de memória, para
        // medirmos quanto o LVGL consumiu (buffers de desenho vão na RAM interna).
        ESP_LOGI(TAG, "heap após LVGL: interno %u B | PSRAM %u B",
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
        bsp_display_set_text("--:--");  // placeholder até a 1ª leitura do RTC
    } else {
        ESP_LOGE(TAG, "falha ao iniciar a tela");
    }

    // --- Botão BOOT + gerenciador de energia da tela (Fase 4) ---
    // Mutex das transições de energia + temporizador one-shot que apaga a tela.
    s_power_mux = xSemaphoreCreateMutex();
    if (s_power_mux == NULL) {
        ESP_LOGE(TAG, "sem memória para o mutex de energia");
    }
    const esp_timer_create_args_t timer_args = {
        .callback = timer_apagar_cb,
        .name = "tela_off",
    };
    if (esp_timer_create(&timer_args, &s_timer_tela) != ESP_OK) {
        ESP_LOGE(TAG, "falha ao criar o temporizador da tela");
    }
    // Arma a 1ª contagem: o boot conta como atividade (a tela apaga sozinha
    // CONFIG_SW_TELA_TIMEOUT_MS depois, se ninguém clicar). Depois sobe o botão:
    // cada clique reinicia essa contagem e acorda a tela, se dormindo.
    registrar_atividade();
    if (bsp_button_init(ao_clicar_botao) != ESP_OK) {
        ESP_LOGW(TAG, "falha ao iniciar o botão");
    }

    // Fuso de Fortaleza (UTC-3, sem horário de verão). Só afeta a EXIBIÇÃO;
    // a hora interna (RTC) continua em UTC. Formato POSIX: "<-03>3".
    setenv("TZ", "<-03>3", 1);
    tzset();

    // --- Batimento: prova que o programa continua vivo ---
    while (1) {
        // esp_timer_get_time() devolve microssegundos (int64). /1.000.000 = segundos.
        uint64_t segundos = esp_timer_get_time() / 1000000ULL;
        ESP_LOGI(TAG, "vivo há %" PRIu64 " s", segundos);

        if (axp_ok) {
            axp2101_info_t bat;  // na stack da task: sem malloc no laço
            esp_err_t err = axp2101_read(&bat);
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "bat: %u mV, %u%% (%s) | usb %s | bat %s | %s",
                         bat.vbat_mv, bat.pct,
                         bat.pct_por_tensao ? "tensão" : "gauge",
                         bat.vbus_ok ? "sim" : "não",
                         bat.bat_presente ? "sim" : "não",
                         axp2101_chg_str(bat.carga));
                // Bytes crus: conferir bits contra docs/HARDWARE.md.
                ESP_LOGI(TAG, "raw: 00=%02X 01=%02X 34=%02X 35=%02X A4=%02X",
                         bat.raw_status1, bat.raw_status2,
                         bat.raw_vbat_h, bat.raw_vbat_l, bat.raw_gauge);
            } else {
                ESP_LOGW(TAG, "axp2101_read falhou: %s", esp_err_to_name(err));
            }
        }

        if (s_rtc_ok) {
            pcf85063_hora_t h;   // na stack: sem malloc no laço
            bool valida = false;
            esp_err_t e1 = pcf85063_ler(&h);
            esp_err_t e2 = pcf85063_hora_valida(&valida);
            if (e1 == ESP_OK && e2 == ESP_OK) {
                // Formato ISO, sempre UTC (o fuso entra só na tela).
                ESP_LOGI(TAG, "RTC: %04u-%02u-%02u %02u:%02u:%02u UTC (%s)",
                         h.ano, h.mes, h.dia, h.hora, h.min, h.seg,
                         valida ? "válida" : "INVÁLIDA");

                // Relógio na tela: só atualiza com a tela ACESA (dormindo, o
                // LVGL está pausado e o painel ignoraria o desenho). Ao acordar,
                // tela_acordar() repinta a hora na hora. Se o minuto mudou,
                // atualiza o label (evita redesenhar à toa a cada 5 s).
                if (s_tela_ok && !s_tela_dormindo) {
                    char hhmm[6];  // "HH:MM" + terminador
                    int min_local = hhmm_fortaleza(&h, hhmm, sizeof(hhmm));
                    if (min_local != s_ultimo_min) {
                        bsp_display_set_text(hhmm);
                        s_ultimo_min = min_local;
                    }
                }
            } else {
                ESP_LOGW(TAG, "leitura do RTC falhou");
            }
        }

        // (A tela apaga pelo temporizador one-shot, não aqui no laço — ver
        // registrar_atividade() e timer_apagar_cb().)

        // Pausa 5 s. pdMS_TO_TICKS converte milissegundos em "ticks" do FreeRTOS.
        // Diferente de um loop vazio, vTaskDelay libera o processador (economiza energia).
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
