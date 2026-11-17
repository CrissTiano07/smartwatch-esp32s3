/*
 * bsp_display.h — tela AMOLED 2.06" (controlador CO5300, interface QSPI).
 *
 * O painel é controlado pelo driver SH8601 (o CO5300 compartilha o mesmo
 * conjunto de comandos QSPI). Pinos, offset de coluna e a sequência de
 * inicialização vêm da BSP oficial da Waveshare — ver docs/HARDWARE.md.
 *
 * A tela NÃO depende de nenhum regulador do AXP2101 ligado por software.
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"

// Resolução do painel (área visível, já descontado o offset de coluna).
#define BSP_DISP_H_RES 410
#define BSP_DISP_V_RES 502

// Inicializa o barramento QSPI, cria o painel, roda a sequência de init do
// fabricante e liga a tela. Chamar uma vez, depois do boot.
esp_err_t bsp_display_init(void);

// Pinta a tela inteira com uma cor RGB565 (ex.: 0xF800 = vermelho puro).
// A cor vai para o painel em big-endian (byte alto primeiro), que é a ordem
// que este painel espera. Desenha em faixas, sem alocar memória.
esp_err_t bsp_display_fill_rgb565(uint16_t color);

// Sobe o LVGL sobre o painel (task do LVGL, tick e thread-safety via
// esp_lvgl_port) e cria um rótulo (label) central. Chamar depois de
// bsp_display_init(). A partir daqui o desenho é feito pelo LVGL, não mais
// por bsp_display_fill_rgb565().
esp_err_t bsp_display_lvgl_init(void);

// Troca o texto do rótulo central, de forma segura entre tarefas.
// IMPORTANTE: as fontes Montserrat padrão do LVGL NÃO têm acentos — use texto
// sem acento (ver pendência no HANDOFF sobre fonte própria pt-BR).
esp_err_t bsp_display_set_text(const char *txt);
