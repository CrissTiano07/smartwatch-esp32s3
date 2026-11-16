/*
 * main.c — Fase 0: prova de vida do firmware.
 *
 * Objetivo: confirmar que compilar → gravar → ver log funciona.
 * Critério de aceite: aparecer "boot ok" no monitor serial.
 */

#include <inttypes.h>            // PRIu32/PRIu64: imprimir inteiros de tamanho fixo no printf
#include "freertos/FreeRTOS.h"   // sistema operacional de tempo real do ESP32 (sempre antes de task.h)
#include "freertos/task.h"       // vTaskDelay: pausar sem travar o processador
#include "esp_log.h"             // ESP_LOGI/ESP_LOGW/ESP_LOGE: log com nível e etiqueta
#include "esp_chip_info.h"       // esp_chip_info: modelo e núcleos do chip
#include "esp_flash.h"           // esp_flash_get_size: tamanho da memória flash
#include "esp_system.h"          // esp_get_free_heap_size, esp_get_idf_version
#include "esp_timer.h"           // esp_timer_get_time: microssegundos desde o boot

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

    // --- Batimento: prova que o programa continua vivo ---
    while (1) {
        // esp_timer_get_time() devolve microssegundos (int64). /1.000.000 = segundos.
        uint64_t segundos = esp_timer_get_time() / 1000000ULL;
        ESP_LOGI(TAG, "vivo há %" PRIu64 " s", segundos);

        // Pausa 5 s. pdMS_TO_TICKS converte milissegundos em "ticks" do FreeRTOS.
        // Diferente de um loop vazio, vTaskDelay libera o processador (economiza energia).
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
