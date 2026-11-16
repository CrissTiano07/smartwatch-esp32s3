/*
 * pcf85063.c — leitura da hora do RTC PCF85063.
 *
 * Nesta primeira parte só LEMOS: init, flag OS e a hora. A gravação
 * (acertar o relógio) entra no próximo commit.
 */
#include "pcf85063.h"
#include "bsp_i2c.h"
#include "esp_log.h"

static const char *TAG = "pcf85063";

#define PCF85063_ADDR   0x51

// Registradores de hora (datasheet NXP PCF85063A). Começam em 0x04 e são
// sete, em sequência, até 0x0A — por isso dá para ler tudo de uma vez.
#define REG_SECONDS     0x04   // bit7 = OS; bits 6:0 = segundos em BCD
#define REG_TIME_LEN    7      // 0x04..0x0A: seg, min, hora, dia, dsem, mês, ano

// Máscaras: cada registrador usa só alguns bits; o resto é flag ou não existe.
#define MASK_OS         0x80   // bit7 de 0x04: oscilador parou?
#define MASK_SECONDS    0x7F
#define MASK_MINUTES    0x7F
#define MASK_HOURS_24H  0x3F   // modo 24h: horas ocupam bits 5:0
#define MASK_DAYS       0x3F
#define MASK_WEEKDAYS   0x07
#define MASK_MONTHS     0x1F
// Ano usa o byte inteiro (00..99), então não precisa de máscara.

static i2c_master_dev_handle_t s_dev = NULL;

// BCD -> decimal. Em BCD cada nibble (4 bits) guarda um dígito decimal:
// o nibble alto é a dezena, o baixo é a unidade. Ex.: 0x59 -> 5*10 + 9 = 59.
static uint8_t bcd2dec(uint8_t bcd)
{
    return (uint8_t)(((bcd >> 4) * 10) + (bcd & 0x0F));
}

esp_err_t pcf85063_init(void)
{
    esp_err_t err = bsp_i2c_add_device(PCF85063_ADDR, &s_dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "add_device falhou: %s", esp_err_to_name(err));
        return err;
    }

    // Confirma que o chip responde lendo o registrador de segundos.
    uint8_t seg = 0;
    err = bsp_i2c_read_reg(s_dev, REG_SECONDS, &seg, 1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "chip não responde em 0x%02X: %s",
                 PCF85063_ADDR, esp_err_to_name(err));
        return err;
    }

    // Já aproveita e avisa no log se a hora está marcada como válida ou não.
    bool os_parou = (seg & MASK_OS) != 0;
    ESP_LOGI(TAG, "init ok (OS=%d -> hora %s)",
             os_parou, os_parou ? "INVÁLIDA" : "válida");
    return ESP_OK;
}

esp_err_t pcf85063_hora_valida(bool *valida)
{
    if (s_dev == NULL || valida == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    uint8_t seg = 0;
    esp_err_t err = bsp_i2c_read_reg(s_dev, REG_SECONDS, &seg, 1);
    if (err != ESP_OK) {
        return err;
    }
    // OS=0 significa integridade garantida; por isso a hora é válida quando
    // o bit está zerado.
    *valida = (seg & MASK_OS) == 0;
    return ESP_OK;
}

esp_err_t pcf85063_ler(pcf85063_hora_t *out)
{
    if (s_dev == NULL || out == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    // Burst: os 7 registradores numa transação só (escreve 0x04, lê 7 bytes).
    uint8_t buf[REG_TIME_LEN] = { 0 };
    esp_err_t err = bsp_i2c_read_reg(s_dev, REG_SECONDS, buf, REG_TIME_LEN);
    if (err != ESP_OK) {
        return err;
    }

    // buf[0]=seg(+OS) buf[1]=min buf[2]=hora buf[3]=dia buf[4]=dsem buf[5]=mês buf[6]=ano
    out->seg        = bcd2dec(buf[0] & MASK_SECONDS);   // tira a flag OS antes de converter
    out->min        = bcd2dec(buf[1] & MASK_MINUTES);
    out->hora       = bcd2dec(buf[2] & MASK_HOURS_24H);
    out->dia        = bcd2dec(buf[3] & MASK_DAYS);
    out->dia_semana = (uint8_t)(buf[4] & MASK_WEEKDAYS); // já é 0..6, não é BCD
    out->mes        = bcd2dec(buf[5] & MASK_MONTHS);
    out->ano        = (uint16_t)(2000 + bcd2dec(buf[6])); // o chip guarda só 00..99
    return ESP_OK;
}
