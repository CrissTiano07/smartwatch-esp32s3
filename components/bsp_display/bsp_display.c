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
#include "esp_lvgl_port.h"         // port oficial: liga o LVGL ao painel esp_lcd
#include "lvgl.h"                  // API gráfica (label, cores, fontes)
#include "ui_fonts.h"              // fonte própria de dígitos grandes (font_digits_180)
#include "freertos/FreeRTOS.h"     // base do RTOS (antes de task.h)
#include "freertos/task.h"         // vTaskDelay: esperar o painel estabilizar no wake

// Comandos de sleep do painel (padrão MIPI, iguais no SH8601/CO5300). O driver
// não os expõe pela API esp_lcd, então os enviamos direto pelo IO do painel —
// do mesmo jeito que a sequência de init manda o 0x11 (sleep-out).
#define LCD_CMD_SLPIN  0x10        // sleep in:  painel entra em repouso
#define LCD_CMD_SLPOUT 0x11        // sleep out: painel sai do repouso

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

static esp_lcd_panel_handle_t    s_panel = NULL;
static esp_lcd_panel_io_handle_t s_io = NULL;     // guardado p/ o esp_lvgl_port
// Relógio em DOIS rótulos empilhados: HH em cima, MM embaixo. Sem ":" porque a
// fonte própria tem só os dígitos 0-9 (ver docs/DECISIONS.md #12).
static lv_obj_t                 *s_label_hh = NULL;  // horas (metade de cima)
static lv_obj_t                 *s_label_mm = NULL;  // minutos (metade de baixo)

// Deslocamento vertical de cada rótulo a partir do centro. A fonte tem ~129 px
// de altura; ~70 px para cada lado empilha HH/MM com uma folga, mantendo o
// bloco centralizado na tela de 502 px.
#define DIG_DY 70

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
    const esp_lcd_panel_io_spi_config_t io_cfg = SH8601_PANEL_IO_QSPI_CONFIG(PIN_CS, NULL, NULL);
    ESP_RETURN_ON_ERROR(
        esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_cfg, &s_io),
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
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_sh8601(s_io, &panel_cfg, &s_panel),
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

// Quantas linhas de tela cabem em cada buffer de desenho do LVGL. Buffer
// PARCIAL (não o frame inteiro): o LVGL desenha por partes. 40 linhas ->
// 410*40*2 = ~32 KB por buffer; com buffer duplo, ~64 KB de RAM interna.
#define LVGL_BUF_LINES 40

// Arredondamento da área a redesenhar.
// PORQUÊ: o SH8601/CO5300 só aceita janelas de pixels alinhadas em 2 — a
// coluna/linha inicial tem de ser PAR e a largura/altura, PAR. Quando o LVGL
// manda uma área de largura ÍMPAR, o painel interpreta o fluxo de pixels com
// uma linha a mais/menos e cada linha "escorrega" um pixel para o lado — o
// texto aparece inclinado (efeito itálico) e deslocado. Este callback, ligado
// ao evento LV_EVENT_INVALIDATE_AREA, expande a área: início para baixo (par)
// e fim para cima (ímpar), garantindo largura/altura pares. Mesma lógica da
// BSP oficial da Waveshare para o LVGL 9.
static void rounder_event_cb(lv_event_t *e)
{
    lv_area_t *area = (lv_area_t *)lv_event_get_param(e);
    area->x1 = (area->x1 >> 1) << 1;         // x1 -> par (arredonda p/ baixo)
    area->y1 = (area->y1 >> 1) << 1;         // y1 -> par
    area->x2 = ((area->x2 >> 1) << 1) + 1;   // x2 -> ímpar (arredonda p/ cima)
    area->y2 = ((area->y2 >> 1) << 1) + 1;   // y2 -> ímpar
}

esp_err_t bsp_display_lvgl_init(void)
{
    if (s_panel == NULL) {
        return ESP_ERR_INVALID_STATE;  // bsp_display_init() não rodou
    }

    // 1) Sobe o serviço do LVGL (uma task dedicada + timer do tick). A regra do
    //    projeto pede stack >= 8192 (crash antigo foi stack curto no LVGL).
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_stack = 8192;
    port_cfg.task_priority = 4;
    ESP_RETURN_ON_ERROR(lvgl_port_init(&port_cfg), TAG, "lvgl_port_init");

    // 2) Registra o painel como um display do LVGL. Buffers na RAM INTERNA com
    //    DMA (buff_dma=true, buff_spiram=false), como a BSP oficial faz.
    //    swap_bytes=true: o LVGL desenha RGB565 em little-endian, mas o painel
    //    espera big-endian (foi o que o teste do vermelho confirmou no Passo 2).
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = s_io,
        .panel_handle = s_panel,
        .buffer_size = BSP_DISP_H_RES * LVGL_BUF_LINES,
        .double_buffer = true,
        .hres = BSP_DISP_H_RES,
        .vres = BSP_DISP_V_RES,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags = {
            .buff_dma = true,
            .buff_spiram = false,
            .swap_bytes = true,
            // sw_rotate=true faz o port NÃO chamar esp_lcd_panel_swap_xy (que o
            // SH8601 não suporta -> erro "swap_xy is not supported"). Sem
            // rotação (0°), não há custo; é só para evitar essa chamada.
            .sw_rotate = true,
        },
    };
    lv_display_t *disp = lvgl_port_add_disp(&disp_cfg);
    if (disp == NULL) {
        ESP_LOGE(TAG, "lvgl_port_add_disp falhou");
        return ESP_FAIL;
    }

    // 3) Cria o rótulo central. Toda mexida em objetos do LVGL tem de estar
    //    entre lock/unlock, porque a task do LVGL roda em paralelo.
    if (!lvgl_port_lock(0)) {
        return ESP_ERR_TIMEOUT;
    }
    // Alinhamento de 2 px exigido pelo painel (ver rounder_event_cb acima).
    lv_display_add_event_cb(disp, rounder_event_cb, LV_EVENT_INVALIDATE_AREA, NULL);

    lv_obj_t *scr = lv_display_get_screen_active(disp);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);  // fundo preto (AMOLED)

    // Dois rótulos com a fonte grande (font_digits_180), brancos, empilhados e
    // centralizados: HH acima do centro, MM abaixo. Começam vazios; o relógio
    // os preenche na 1ª leitura do RTC.
    s_label_hh = lv_label_create(scr);
    lv_obj_set_style_text_color(s_label_hh, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_label_hh, &font_digits_180, 0);
    lv_label_set_text(s_label_hh, "");
    lv_obj_align(s_label_hh, LV_ALIGN_CENTER, 0, -DIG_DY);

    s_label_mm = lv_label_create(scr);
    lv_obj_set_style_text_color(s_label_mm, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_label_mm, &font_digits_180, 0);
    lv_label_set_text(s_label_mm, "");
    lv_obj_align(s_label_mm, LV_ALIGN_CENTER, 0, DIG_DY);
    lvgl_port_unlock();

    ESP_LOGI(TAG, "LVGL pronto");
    return ESP_OK;
}

esp_err_t bsp_display_set_hhmm(const char *hh, const char *mm)
{
    if (s_label_hh == NULL || s_label_mm == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!lvgl_port_lock(0)) {
        return ESP_ERR_TIMEOUT;
    }
    lv_label_set_text(s_label_hh, hh);   // o LVGL copia a string internamente
    lv_label_set_text(s_label_mm, mm);
    // Recentraliza cada linha: a largura muda conforme os dígitos (o '1' é mais
    // estreito). O deslize horizontal é aceito por ora (DECISIONS #12); aqui só
    // garantimos que HH e MM ficam centrados no seu eixo e empilhados.
    lv_obj_align(s_label_hh, LV_ALIGN_CENTER, 0, -DIG_DY);
    lv_obj_align(s_label_mm, LV_ALIGN_CENTER, 0, DIG_DY);
    lvgl_port_unlock();
    return ESP_OK;
}

esp_err_t bsp_display_sleep(void)
{
    if (s_panel == NULL || s_io == NULL) {
        return ESP_ERR_INVALID_STATE;  // a tela não subiu
    }
    // 1) PAUSA o LVGL: desliga o tick e os timers, então a task do LVGL deixa
    //    de desenhar enquanto a tela dorme (economiza CPU e nada é enviado a um
    //    painel adormecido).
    lvgl_port_stop();
    // 2) Trava o mutex do LVGL para garantir que um flush em andamento terminou
    //    antes de mandarmos o painel dormir. Se não conseguir a trava, desfaz a
    //    pausa para não deixar o LVGL parado por engano.
    if (!lvgl_port_lock(0)) {
        lvgl_port_resume();
        return ESP_ERR_TIMEOUT;
    }
    // 3) Desliga a saída (pixels apagam) e manda o painel dormir.
    esp_err_t err = esp_lcd_panel_disp_on_off(s_panel, false);          // DISPOFF (0x28)
    if (err == ESP_OK) {
        err = esp_lcd_panel_io_tx_param(s_io, LCD_CMD_SLPIN, NULL, 0);  // SLPIN (0x10)
    }
    lvgl_port_unlock();
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "tela dormindo (LVGL pausado + display off + sleep)");
    }
    return err;
}

esp_err_t bsp_display_wake(void)
{
    if (s_panel == NULL || s_io == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!lvgl_port_lock(0)) {
        return ESP_ERR_TIMEOUT;
    }
    // 1) Acorda o painel. O oscilador/booster precisam de ~120 ms para
    //    estabilizar antes de religar a saída — mesmo atraso que o init usa
    //    depois do 0x11. 2) Religa a saída.
    esp_err_t err = esp_lcd_panel_io_tx_param(s_io, LCD_CMD_SLPOUT, NULL, 0);  // SLPOUT (0x11)
    if (err == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(120));
        err = esp_lcd_panel_disp_on_off(s_panel, true);                       // DISPON (0x29)
    }
    lvgl_port_unlock();
    // 3) RETOMA o LVGL (tick + timers) para voltar a desenhar. O repinte da
    //    tela é pedido por quem chamou (bsp_display_redraw).
    lvgl_port_resume();
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "tela acordada (sleep out + display on + LVGL retomado)");
    }
    return err;
}

esp_err_t bsp_display_redraw(void)
{
    if (s_label_hh == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!lvgl_port_lock(0)) {
        return ESP_ERR_TIMEOUT;
    }
    // Marca a tela INTEIRA como "suja" para o LVGL repintar tudo (fundo +
    // rótulos). Necessário depois do wake: enquanto o painel dormia, ele
    // ignorou o que o LVGL desenhou, então a memória do painel pode estar
    // desatualizada. lv_obj_get_screen(s_label_hh) devolve a tela que contém o
    // rótulo — sem depender de qual é o display "padrão".
    lv_obj_invalidate(lv_obj_get_screen(s_label_hh));
    lvgl_port_unlock();
    return ESP_OK;
}
