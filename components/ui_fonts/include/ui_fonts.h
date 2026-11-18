// ui_fonts.h — fontes próprias da UI, geradas com lv_font_conv.
//
// Por que fonte própria: as Montserrat embutidas do LVGL vão só até 48 px, e o
// relógio precisa de dígitos GRANDES (Fase 5). Gerando só os glifos 0-9 num
// tamanho grande, a fonte fica enxuta (10 glifos) e enche a tela.
//
// Como regenerar: ver README.md deste componente (comando exato + .ttf em
// tools/fonts/). Decisão registrada em docs/DECISIONS.md (#12).
#pragma once

#include "lvgl.h"

// Dígitos 0-9 do Montserrat Regular, 180 px nominais (~128 px de altura real),
// 4 bpp (anti-aliasing). SEM ":" — o layout empilha HH sobre MM, sem dois-pontos.
// Definida em font_digits_180.c (gerado).
extern const lv_font_t font_digits_180;
