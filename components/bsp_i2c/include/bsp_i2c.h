/*
 * bsp_i2c.h — barramento I2C compartilhado da placa (SCL GPIO14, SDA GPIO15).
 *
 * Por que um módulo só para isso? Seis chips dividem os mesmos dois fios.
 * Se cada driver criasse o próprio barramento, teríamos conflito de hardware.
 * Aqui existe UM handle do barramento, e os chips viram "dispositivos" nele.
 */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

// Inicializa o barramento. Pode ser chamada mais de uma vez (só faz na 1ª).
esp_err_t bsp_i2c_init(void);

// Varre os endereços 0x08..0x77 e loga quem respondeu, com o nome do chip
// esperado segundo docs/HARDWARE.md. Devolve quantos responderam.
int bsp_i2c_scan(void);

// Registra um chip no barramento. 'out' recebe o handle usado nas leituras.
esp_err_t bsp_i2c_add_device(uint8_t addr, i2c_master_dev_handle_t *out);

// Lê 'len' bytes a partir do registrador 'reg' (escreve reg, depois lê).
esp_err_t bsp_i2c_read_reg(i2c_master_dev_handle_t dev, uint8_t reg,
                           uint8_t *buf, size_t len);

// Escreve um byte 'val' no registrador 'reg'.
esp_err_t bsp_i2c_write_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t val);
