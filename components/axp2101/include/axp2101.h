/*
 * axp2101.h — chip de energia (PMU): bateria, carga e USB.
 * Registradores e fontes: docs/HARDWARE.md.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

// Estado de carga, direto dos bits 6:5 do STATUS2 (0x01).
typedef enum {
    AXP2101_CHG_PARADO       = 0,  // parado ou bateria cheia
    AXP2101_CHG_CARREGANDO   = 1,
    AXP2101_CHG_DESCARREGANDO = 2,
    AXP2101_CHG_INDEFINIDO   = 3,  // valor não documentado
} axp2101_chg_t;

typedef struct {
    uint16_t vbat_mv;       // tensão da bateria em mV
    uint8_t  pct;           // % final que o firmware deve usar
    bool     pct_por_tensao; // true = % veio da curva de tensão (E-Gauge deu 0)
    bool     vbus_ok;       // USB conectado e com tensão boa
    bool     bat_presente;  // bateria detectada (bit a confirmar pelo log)
    axp2101_chg_t carga;

    // Bytes crus, para conferir no log se a interpretação está certa.
    uint8_t raw_status1;    // 0x00
    uint8_t raw_status2;    // 0x01
    uint8_t raw_vbat_h;     // 0x34
    uint8_t raw_vbat_l;     // 0x35
    uint8_t raw_gauge;      // 0xA4
} axp2101_info_t;

// Registra o chip no barramento e liga a medição de VBAT.
// Exige bsp_i2c_init() antes.
esp_err_t axp2101_init(void);

// Lê tudo de uma vez e preenche 'out'.
esp_err_t axp2101_read(axp2101_info_t *out);

// Texto curto para o estado de carga (para log).
const char *axp2101_chg_str(axp2101_chg_t c);
