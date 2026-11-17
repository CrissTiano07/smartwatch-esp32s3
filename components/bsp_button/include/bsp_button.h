/*
 * bsp_button.h — botão físico BOOT (GPIO0) da placa.
 *
 * Esta placa tem dois botões laterais: BOOT e PWR (ver docs/HARDWARE.md).
 * Usamos o BOOT porque é o único 100% confirmado como GPIO direto do ESP32
 * (GPIO0, nível baixo = pressionado). O PWR chega por um caminho ainda não
 * confirmado (ver HARDWARE.md) — fica para depois.
 *
 * Leitura por INTERRUPÇÃO (nada de polling — regra do projeto). A rotina de
 * interrupção (ISR) faz o mínimo: só avisa uma task por uma fila. Quem trata
 * o clique de verdade é a task, onde já é seguro logar e chamar código normal.
 */
#pragma once

#include "esp_err.h"

// Função chamada a cada clique do botão. IMPORTANTE: roda na TASK do botão
// (não na ISR), então aqui pode logar, mexer na tela, etc. — à vontade.
typedef void (*bsp_button_cb_t)(void);

// Configura o botão BOOT (GPIO0) como entrada com interrupção na borda de
// descida (o momento do aperto) e cria a task que trata o evento.
// 'cb' é chamado uma vez por clique, já com antirrebote. Pode ser NULL.
esp_err_t bsp_button_init(bsp_button_cb_t cb);
