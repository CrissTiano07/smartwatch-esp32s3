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
// esp_lvgl_port) e cria DOIS rótulos empilhados (HH em cima, MM embaixo) com a
// fonte de dígitos grandes. Chamar depois de bsp_display_init(). A partir daqui
// o desenho é feito pelo LVGL, não mais por bsp_display_fill_rgb565().
esp_err_t bsp_display_lvgl_init(void);

// Atualiza o relógio: 'hh' e 'mm' são strings de 2 dígitos ("09", "47"), que
// vão nos dois rótulos empilhados. Seguro entre tarefas. Sem ":" — o layout é
// empilhado e a fonte grande (font_digits_180) tem só os dígitos 0-9
// (ver docs/DECISIONS.md #12). Passar "" nos dois deixa a tela em branco.
esp_err_t bsp_display_set_hhmm(const char *hh, const char *mm);

// Põe a tela para dormir (economia de energia, Fase 4): PAUSA o LVGL, desliga
// a saída do painel (display off, 0x28) e manda o painel entrar em sleep (0x10).
// A imagem na memória do painel é preservada — bsp_display_wake() a traz de volta.
esp_err_t bsp_display_sleep(void);

// Acorda a tela: tira o painel do sleep (0x11), espera estabilizar, religa a
// saída (display on, 0x29) e RETOMA o LVGL. É o inverso de bsp_display_sleep().
// Depois do wake, chame bsp_display_redraw() para repintar a tela.
esp_err_t bsp_display_wake(void);

// Marca a tela inteira como "suja" para o LVGL repintar tudo (fundo + rótulo).
// Use depois do wake: enquanto dormia, o painel ignorou o desenho do LVGL.
esp_err_t bsp_display_redraw(void);
