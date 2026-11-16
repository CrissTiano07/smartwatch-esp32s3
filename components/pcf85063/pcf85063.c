/*
 * pcf85063.c — leitura e gravação da hora do RTC PCF85063.
 */
#include <stdio.h>          // sscanf: ler __DATE__/__TIME__
#include <string.h>         // strstr: achar o mês na tabela
#include <stdlib.h>         // setenv
#include <time.h>           // struct tm, mktime, gmtime_r, tzset
#include "pcf85063.h"
#include "bsp_i2c.h"
#include "sdkconfig.h"      // CONFIG_RTC_FORCAR_SEMENTE (vem do menuconfig)
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

// decimal -> BCD. Caminho inverso: dezena no nibble alto, unidade no baixo.
// Ex.: 59 -> (5 << 4) | 9 = 0x59. Para 0..99 o resultado cabe num byte.
static uint8_t dec2bcd(uint8_t dec)
{
    return (uint8_t)(((dec / 10) << 4) | (dec % 10));
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

esp_err_t pcf85063_gravar(const pcf85063_hora_t *in)
{
    if (s_dev == NULL || in == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    // Monta os 7 bytes na ordem dos registradores (0x04..0x0A), já em BCD.
    uint8_t buf[REG_TIME_LEN];
    buf[0] = dec2bcd(in->seg) & MASK_SECONDS;   // bit OS fica 0 -> hora válida
    buf[1] = dec2bcd(in->min);
    buf[2] = dec2bcd(in->hora);                 // modo 24h
    buf[3] = dec2bcd(in->dia);
    buf[4] = in->dia_semana & MASK_WEEKDAYS;    // 0..6, não é BCD
    buf[5] = dec2bcd(in->mes);
    buf[6] = dec2bcd((uint8_t)(in->ano % 100)); // só os dois últimos dígitos

    // Burst: os 7 de uma vez. (O datasheet sugere ainda parar o oscilador —
    // bit STOP de 0x00 — durante a escrita; para uma semente de boot a chance
    // de o contador virar no meio é desprezível, então mantemos simples.)
    return bsp_i2c_write_regs(s_dev, REG_SECONDS, buf, REG_TIME_LEN);
}

// Converte a hora de COMPILAÇÃO (__DATE__/__TIME__, horário local de
// Fortaleza) para UTC, preenchendo 'out'. Faz a conversão do jeito certo —
// via struct tm + TZ + mktime + gmtime_r — para a virada de dia/mês/ano
// sair correta, em vez de somar 3 horas na mão.
static bool hora_de_build_utc(pcf85063_hora_t *out)
{
    // __DATE__ vem como "Mmm dd yyyy" (ex.: "Nov 16 2026"); __TIME__ como
    // "hh:mm:ss". O mês é texto, então procuramos numa tabela.
    static const char meses[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
    char mes_txt[4] = { 0 };
    int dia = 0, ano = 0, h = 0, m = 0, s = 0;

    if (sscanf(__DATE__, "%3s %d %d", mes_txt, &dia, &ano) != 3) return false;
    if (sscanf(__TIME__, "%d:%d:%d", &h, &m, &s) != 3) return false;

    const char *p = strstr(meses, mes_txt);
    if (p == NULL) return false;
    int mes = (int)((p - meses) / 3) + 1;       // posição/3 -> 0..11, +1 -> 1..12

    // Monta a data/hora como horário LOCAL.
    struct tm local = { 0 };
    local.tm_year  = ano - 1900;                // tm conta anos desde 1900
    local.tm_mon   = mes - 1;                   // 0..11
    local.tm_mday  = dia;
    local.tm_hour  = h;
    local.tm_min   = m;
    local.tm_sec   = s;
    local.tm_isdst = 0;                         // Fortaleza não tem horário de verão

    // Diz ao runtime que o horário local é o de Fortaleza (UTC-3) e converte
    // para "epoch" (segundos desde 1970 em UTC). mktime usa o TZ atual.
    setenv("TZ", "<-03>3", 1);
    tzset();
    time_t epoch = mktime(&local);
    if (epoch == (time_t)-1) return false;

    // De epoch de volta para campos de calendário, agora em UTC.
    struct tm utc;
    gmtime_r(&epoch, &utc);
    out->ano        = (uint16_t)(utc.tm_year + 1900);
    out->mes        = (uint8_t)(utc.tm_mon + 1);
    out->dia        = (uint8_t)utc.tm_mday;
    out->hora       = (uint8_t)utc.tm_hour;
    out->min        = (uint8_t)utc.tm_min;
    out->seg        = (uint8_t)utc.tm_sec;
    out->dia_semana = (uint8_t)utc.tm_wday;     // 0=domingo, como o RTC espera
    return true;
}

esp_err_t pcf85063_semear_se_preciso(bool *semeou)
{
    if (semeou == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    *semeou = false;

    bool valida = false;
    esp_err_t err = pcf85063_hora_valida(&valida);
    if (err != ESP_OK) {
        return err;
    }

#if CONFIG_RTC_FORCAR_SEMENTE
    const bool forcar = true;
#else
    const bool forcar = false;
#endif

    // Só grava se a hora é inválida, ou se o usuário pediu para forçar.
    if (valida && !forcar) {
        ESP_LOGI(TAG, "hora válida no RTC; não semeia");
        return ESP_OK;
    }

    pcf85063_hora_t h;
    if (!hora_de_build_utc(&h)) {
        ESP_LOGE(TAG, "não consegui interpretar a hora de build");
        return ESP_FAIL;
    }

    err = pcf85063_gravar(&h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "falha ao gravar hora: %s", esp_err_to_name(err));
        return err;
    }

    *semeou = true;
    ESP_LOGW(TAG, "hora semeada da build (%s): %04u-%02u-%02u %02u:%02u:%02u UTC",
             forcar ? "forçada" : "RTC inválido",
             h.ano, h.mes, h.dia, h.hora, h.min, h.seg);
    return ESP_OK;
}
