/*
 * bsp_display.c — inicialização da tela AMOLED CO5300 (QSPI) e preenchimento.
 *
 * Fonte dos valores (pinos, init, offset): BSP oficial da Waveshare para a
 * ESP32-S3-Touch-AMOLED-2.06 (ver docs/HARDWARE.md). Nada aqui é chute.
 */
#include "bsp_display.h"
#include "driver/spi_master.h"     // spi_bus_initialize, SPI2_HOST
#include "esp_lcd_panel_io.h"      // esp_lcd_new_panel_io_spi
#include "esp_lcd_panel_ops.h"     // reset/init/set_gap/disp_on_off/draw_bitmap
#include "esp_lcd_sh8601.h"        // driver do painel + macros de config QSPI
#include "esp_check.h"             // ESP_RETURN_ON_ERROR
#include "esp_log.h"

static const char *TAG = "bsp_display";

// --- Pinos QSPI (docs/HARDWARE.md; fonte: BSP oficial Waveshare) ---
#define LCD_HOST   SPI2_HOST
#define PIN_SCLK   11   // clock
#define PIN_CS     12   // chip select
#define PIN_D0     4    // dados 0
#define PIN_D1     5    // dados 1
#define PIN_D2     6    // dados 2
#define PIN_D3     7    // dados 3
#define PIN_RST    8    // reset do painel

#define LCD_BITS_PER_PIXEL 16   // RGB565

// Pintamos a tela em FAIXAS horizontais para não precisar de um buffer do
// frame inteiro (410*502*2 = ~402 KB). 20 linhas por faixa -> ~16 KB.
#define STRIP_H 20

// Buffer estático (sem malloc em laço, regra do projeto) e na RAM interna,
// que é acessível pelo DMA do SPI. 2 bytes por pixel (RGB565).
static uint8_t s_strip[BSP_DISP_H_RES * STRIP_H * 2];

static esp_lcd_panel_handle_t s_panel = NULL;

// Sequência de init do painel — copiada VERBATIM da BSP oficial da Waveshare
// (esp32_s3_touch_amoled_2_06.c). Formato: {comando, dados, nº de bytes, atraso_ms}.
// O CO5300 usa o mesmo conjunto de comandos do SH8601.
static const sh8601_lcd_init_cmd_t lcd_init_cmds[] = {
    {0x11, (uint8_t[]){0x00}, 0, 120},                 // sleep-out (espera 120 ms)
    {0xC4, (uint8_t[]){0x80}, 1, 0},                   // seleciona modo QSPI
    {0x44, (uint8_t[]){0x01, 0xD1}, 2, 0},             // linha do tearing (0x01D1)
    {0x35, (uint8_t[]){0x00}, 1, 0},                   // tearing effect ligado
    {0x53, (uint8_t[]){0x20}, 1, 10},                  // controle de brilho on
    {0x63, (uint8_t[]){0xFF}, 1, 10},                  // brilho em modo HBM
    {0x51, (uint8_t[]){0x00}, 1, 10},                  // brilho = 0 durante o init
    {0x2A, (uint8_t[]){0x00, 0x16, 0x01, 0xAF}, 4, 0}, // colunas 22..431 (= 410)
    {0x2B, (uint8_t[]){0x00, 0x00, 0x01, 0xF5}, 4, 0}, // linhas 0..501 (= 502)
    {0x29, (uint8_t[]){0x00}, 0, 10},                  // display ON
    {0x51, (uint8_t[]){0xFF}, 1, 0},                   // brilho = 255 (máximo)
};

esp_err_t bsp_display_init(void)
{
    // 1) Barramento QSPI (4 linhas de dados). A macro preenche os campos a
    //    partir dos pinos; o último argumento é o tamanho máximo de uma
    //    transferência DMA — basta caber uma faixa.
    ESP_LOGI(TAG, "iniciando barramento QSPI (SPI2)");
    const spi_bus_config_t buscfg = SH8601_PANEL_BUS_QSPI_CONFIG(
        PIN_SCLK, PIN_D0, PIN_D1, PIN_D2, PIN_D3, sizeof(s_strip));
    ESP_RETURN_ON_ERROR(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO),
                        TAG, "spi_bus_initialize");

    // 2) Camada de I/O do painel sobre o SPI (CS, clock 40 MHz, modo QSPI —
    //    tudo vindo da macro do driver).
    ESP_LOGI(TAG, "criando IO do painel (CS=%d)", PIN_CS);
    esp_lcd_panel_io_handle_t io = NULL;
    const esp_lcd_panel_io_spi_config_t io_cfg = SH8601_PANEL_IO_QSPI_CONFIG(PIN_CS, NULL, NULL);
    ESP_RETURN_ON_ERROR(
        esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_cfg, &io),
        TAG, "panel_io_spi");

    // 3) Cria o painel SH8601 passando a sequência de init do fabricante.
    sh8601_vendor_config_t vendor = {
        .init_cmds = lcd_init_cmds,
        .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0]),
        .flags = { .use_qspi_interface = 1 },
    };
    const esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = LCD_BITS_PER_PIXEL,
        .vendor_config = &vendor,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_sh8601(io, &panel_cfg, &s_panel),
                        TAG, "new_panel_sh8601");

    // 4) Liga o painel. O offset de coluna (22) alinha a janela de desenho com
    //    a área física do AMOLED (ver docs/HARDWARE.md).
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(s_panel), TAG, "reset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_panel), TAG, "init");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_set_gap(s_panel, 22, 0), TAG, "set_gap");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(s_panel, true), TAG, "disp_on");
    ESP_LOGI(TAG, "painel pronto (%dx%d)", BSP_DISP_H_RES, BSP_DISP_V_RES);
    return ESP_OK;
}

esp_err_t bsp_display_fill_rgb565(uint16_t color)
{
    if (s_panel == NULL) {
        return ESP_ERR_INVALID_STATE;  // bsp_display_init() não rodou
    }

    // RGB565 big-endian: byte ALTO primeiro. Ex.: 0xF800 vira {0xF8, 0x00}.
    // Se a tela aparecer de outra cor, a ordem dos bytes está trocada.
    uint8_t hi = (uint8_t)(color >> 8);
    uint8_t lo = (uint8_t)(color & 0xFF);
    for (size_t i = 0; i < sizeof(s_strip); i += 2) {
        s_strip[i]     = hi;
        s_strip[i + 1] = lo;
    }

    // Desenha a tela toda, uma faixa de STRIP_H linhas por vez. Como a cor é
    // uniforme, o mesmo buffer serve para todas as faixas.
    for (int y = 0; y < BSP_DISP_V_RES; y += STRIP_H) {
        int h = (y + STRIP_H <= BSP_DISP_V_RES) ? STRIP_H : (BSP_DISP_V_RES - y);
        ESP_RETURN_ON_ERROR(
            esp_lcd_panel_draw_bitmap(s_panel, 0, y, BSP_DISP_H_RES, y + h, s_strip),
            TAG, "draw_bitmap");
    }
    return ESP_OK;
}
