/*
 * pcf85063.h — RTC PCF85063 (endereço I2C 0x51).
 *
 * É a FONTE DA VERDADE da hora (ver docs/DECISIONS.md). O relógio tem
 * alimentação própria, então mantém a hora mesmo quando o ESP32 reinicia.
 *
 * A hora guardada aqui é sempre UTC (regra do projeto). O fuso de Fortaleza
 * (UTC-3) só é aplicado na hora de exibir, lá na frente.
 *
 * Registradores de hora: 0x04..0x0A, todos em BCD. O bit7 do registrador de
 * segundos (0x04) é a flag OS (Oscillator Stop): 1 = o oscilador parou em
 * algum momento (ex.: faltou energia) e a hora NÃO é confiável; 0 = hora
 * válida. Fonte: datasheet NXP PCF85063A, seção dos registradores de tempo.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

// Um instante de tempo, já em números decimais normais (não BCD) e em UTC.
typedef struct {
    uint16_t ano;        // ano com 4 dígitos, ex.: 2026
    uint8_t  mes;        // 1..12
    uint8_t  dia;        // 1..31
    uint8_t  hora;       // 0..23 (sempre 24h)
    uint8_t  min;        // 0..59
    uint8_t  seg;        // 0..59
    uint8_t  dia_semana; // 0..6 (0 = domingo); como gmtime_r preenche tm_wday
} pcf85063_hora_t;

// Registra o chip (0x51) no barramento. Exige bsp_i2c_init() antes.
esp_err_t pcf85063_init(void);

// Lê a flag OS e devolve em '*valida': true = hora confiável, false = não.
esp_err_t pcf85063_hora_valida(bool *valida);

// Lê a hora atual (0x04..0x0A) numa única transação I2C e converte de BCD.
// Burst de leitura: se o relógio "virar" o segundo no meio, ainda assim
// pegamos um instante coerente, não metade de antes e metade de depois.
esp_err_t pcf85063_ler(pcf85063_hora_t *out);

// Grava a hora (converte dec->BCD) nos 7 registradores numa transação só.
// Como escreve o registrador de segundos com o bit OS = 0, a gravação já
// marca a hora como válida. 'in' deve estar em UTC e no modo 24h.
esp_err_t pcf85063_gravar(const pcf85063_hora_t *in);

// Semeia o RTC com a hora de COMPILAÇÃO (convertida para UTC) quando ele
// está inválido (flag OS = 1) — ou sempre, se CONFIG_RTC_FORCAR_SEMENTE
// estiver ligado no menuconfig. É o jeito de ter uma hora inicial sem Wi-Fi;
// o NTP da Fase 6 corrige o valor absoluto depois.
// '*semeou' recebe true se chegou a gravar, false se confiou no RTC.
esp_err_t pcf85063_semear_se_preciso(bool *semeou);
