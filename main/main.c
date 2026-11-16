/*
 * main.c — Fase 2: barramento I2C + AXP2101 + RTC PCF85063.
 *
 * Critério de aceite da Fase 2: grava a hora, reinicia e a hora continua
 * certa no log (e marcada como válida), sem Wi-Fi. A bateria segue sendo
 * logada da Fase 1.
 */

#include <inttypes.h>            // PRIu32/PRIu64: imprimir inteiros de tamanho fixo no printf
#include "freertos/FreeRTOS.h"   // sistema operacional de tempo real do ESP32 (sempre antes de task.h)
#include "freertos/task.h"       // vTaskDelay: pausar sem travar o processador
#include "esp_log.h"             // ESP_LOGI/ESP_LOGW/ESP_LOGE: log com nível e etiqueta
#include "esp_chip_info.h"       // esp_chip_info: modelo e núcleos do chip
#include "esp_flash.h"           // esp_flash_get_size: tamanho da memória flash
#include "esp_system.h"          // esp_get_free_heap_size, esp_get_idf_version
#include "esp_timer.h"           // esp_timer_get_time: microssegundos desde o boot
#include "bsp_i2c.h"             // nosso barramento I2C compartilhado
#include "axp2101.h"             // nosso driver do chip de energia
#include "pcf85063.h"            // nosso driver do RTC (hora)

// Etiqueta que aparece no começo de cada linha de log: "I (123) main: ..."
// Ajuda a saber de qual módulo veio a mensagem quando o projeto crescer.
static const char *TAG = "main";

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

    // --- I2C: barramento primeiro, depois quem está nele ---
    // Se algo falhar, NÃO reiniciamos: seguimos no laço para o erro
    // ficar visível no monitor (um reboot em loop esconderia a causa).
    bool axp_ok = false;
    bool rtc_ok = false;
    if (bsp_i2c_init() == ESP_OK) {
        bsp_i2c_scan();
        axp_ok = (axp2101_init() == ESP_OK);

        // RTC: registra o chip e, se a hora estiver inválida (ou se a opção
        // RTC_FORCAR_SEMENTE estiver ligada), semeia com a hora de build.
        rtc_ok = (pcf85063_init() == ESP_OK);
        if (rtc_ok) {
            bool semeou = false;
            pcf85063_semear_se_preciso(&semeou);
        }
    }

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

        if (rtc_ok) {
            pcf85063_hora_t h;   // na stack: sem malloc no laço
            bool valida = false;
            esp_err_t e1 = pcf85063_ler(&h);
            esp_err_t e2 = pcf85063_hora_valida(&valida);
            if (e1 == ESP_OK && e2 == ESP_OK) {
                // Formato ISO, sempre UTC (o fuso entra só na tela, na Fase 3).
                ESP_LOGI(TAG, "RTC: %04u-%02u-%02u %02u:%02u:%02u UTC (%s)",
                         h.ano, h.mes, h.dia, h.hora, h.min, h.seg,
                         valida ? "válida" : "INVÁLIDA");
            } else {
                ESP_LOGW(TAG, "leitura do RTC falhou");
            }
        }

        // Pausa 5 s. pdMS_TO_TICKS converte milissegundos em "ticks" do FreeRTOS.
        // Diferente de um loop vazio, vTaskDelay libera o processador (economiza energia).
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
