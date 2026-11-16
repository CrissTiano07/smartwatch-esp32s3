/*
 * axp2101.c — leitura de bateria e status do AXP2101.
 *
 * Só leitura + ligar o ADC de VBAT. Não mexemos em tensões de saída
 * (DCDC/LDO): elas alimentam o próprio ESP32 e a tela; errar ali pode
 * desligar a placa. Isso fica para quando houver motivo e fonte confiável.
 */
#include "axp2101.h"
#include "bsp_i2c.h"
#include "esp_log.h"

static const char *TAG = "axp2101";

#define AXP2101_ADDR        0x34

// Registradores (docs/HARDWARE.md)
#define REG_STATUS1         0x00
#define REG_STATUS2         0x01
#define REG_ADC_ENABLE      0x30
#define REG_VBAT_H          0x34
#define REG_VBAT_L          0x35
#define REG_BAT_PERCENT     0xA4

// Bits
#define STATUS1_VBUS_OK     (1 << 5)
#define STATUS1_BAT_PRESENT (1 << 3)
#define ADC_EN_VBAT         (1 << 0)

static i2c_master_dev_handle_t s_dev = NULL;

// Lê um único registrador. Atalho para não repetir o tamanho 1 toda hora.
static esp_err_t ler(uint8_t reg, uint8_t *val)
{
    return bsp_i2c_read_reg(s_dev, reg, val, 1);
}

/*
 * Curva de descarga LiPo (docs/HARDWARE.md):
 * 4200 mV=100% · 3700=50% · 3200=10% · 3000=0%.
 * Entre dois pontos, interpolação linear (regra de três).
 * É aproximado: a tensão cai sob carga e sobe quando o USB está ligado.
 */
static uint8_t pct_por_tensao(uint16_t mv)
{
    static const struct { uint16_t mv; uint8_t pct; } curva[] = {
        { 3000,   0 },
        { 3200,  10 },
        { 3700,  50 },
        { 4200, 100 },
    };
    const int n = sizeof(curva) / sizeof(curva[0]);

    if (mv <= curva[0].mv)     return 0;
    if (mv >= curva[n - 1].mv) return 100;

    for (int i = 1; i < n; i++) {
        if (mv <= curva[i].mv) {
            // Quanto 'mv' andou dentro do trecho [i-1, i], proporcionalmente.
            uint32_t dv   = mv - curva[i - 1].mv;
            uint32_t faixa_mv  = curva[i].mv - curva[i - 1].mv;
            uint32_t faixa_pct = curva[i].pct - curva[i - 1].pct;
            return curva[i - 1].pct + (dv * faixa_pct) / faixa_mv;
        }
    }
    return 100;  // não chega aqui; satisfaz o compilador
}

esp_err_t axp2101_init(void)
{
    esp_err_t err = bsp_i2c_add_device(AXP2101_ADDR, &s_dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "add_device falhou: %s", esp_err_to_name(err));
        return err;
    }

    // Leitura-modificação-escrita: os outros bits de 0x30 ligam outros
    // ADCs (temperatura, VBUS...). Sobrescrever com 0x01 desligaria todos.
    uint8_t adc = 0;
    err = ler(REG_ADC_ENABLE, &adc);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "chip não responde em 0x%02X: %s",
                 AXP2101_ADDR, esp_err_to_name(err));
        return err;
    }
    if ((adc & ADC_EN_VBAT) == 0) {
        err = bsp_i2c_write_reg(s_dev, REG_ADC_ENABLE, adc | ADC_EN_VBAT);
        if (err != ESP_OK) {
            return err;
        }
    }
    ESP_LOGI(TAG, "init ok (0x30: 0x%02X -> 0x%02X)", adc, adc | ADC_EN_VBAT);
    return ESP_OK;
}

esp_err_t axp2101_read(axp2101_info_t *out)
{
    if (s_dev == NULL || out == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    // Lemos registrador por registrador (como a XPowersLib faz).
    // Se qualquer leitura falhar, paramos e devolvemos o erro.
    esp_err_t err;
    if ((err = ler(REG_STATUS1,     &out->raw_status1)) != ESP_OK) return err;
    if ((err = ler(REG_STATUS2,     &out->raw_status2)) != ESP_OK) return err;
    if ((err = ler(REG_VBAT_H,      &out->raw_vbat_h))  != ESP_OK) return err;
    if ((err = ler(REG_VBAT_L,      &out->raw_vbat_l))  != ESP_OK) return err;
    if ((err = ler(REG_BAT_PERCENT, &out->raw_gauge))   != ESP_OK) return err;

    // VBAT: 14 bits. Os 6 bits baixos do byte H são a parte alta do número;
    // '& 0x3F' descarta os 2 bits de cima, que não fazem parte da medida.
    out->vbat_mv = ((uint16_t)(out->raw_vbat_h & 0x3F) << 8) | out->raw_vbat_l;

    out->vbus_ok      = (out->raw_status1 & STATUS1_VBUS_OK) != 0;
    out->bat_presente = (out->raw_status1 & STATUS1_BAT_PRESENT) != 0;

    // Bits 6:5 → desloca 5 para a direita e fica com os 2 bits de baixo.
    out->carga = (axp2101_chg_t)((out->raw_status2 >> 5) & 0x03);

    // E-Gauge só é útil depois de calibrado. 0 (ou lixo >100) = não confiar.
    if (out->raw_gauge == 0 || out->raw_gauge > 100) {
        out->pct = pct_por_tensao(out->vbat_mv);
        out->pct_por_tensao = true;
    } else {
        out->pct = out->raw_gauge;
        out->pct_por_tensao = false;
    }
    return ESP_OK;
}

const char *axp2101_chg_str(axp2101_chg_t c)
{
    switch (c) {
    case AXP2101_CHG_PARADO:        return "parado/cheio";
    case AXP2101_CHG_CARREGANDO:    return "carregando";
    case AXP2101_CHG_DESCARREGANDO: return "descarregando";
    default:                        return "indefinido";
    }
}
