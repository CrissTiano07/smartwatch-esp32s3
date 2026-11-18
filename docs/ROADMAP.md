# ROADMAP

Regra: só avança com o critério de aceite cumprido **na placa**. Cada fase = commits pequenos.

| Fase | Entrega | Critério de aceite | Status |
|------|---------|--------------------|--------|
| 0 | Repositório + projeto IDF mínimo | `idf.py build` ok · log "boot ok" no monitor · 1º commit no GitHub | ✅ |
| 1 | I2C + AXP2101 | Scan lista os chips · log mostra tensão e % da bateria | ✅ |
| 2 | RTC PCF85063 | Grava hora · reset · hora continua certa no log, sem Wi-Fi | ✅ |
| 3 | Tela + LVGL mínimo | Relógio digital HH:MM na tela, vindo do RTC | ✅ |
| 4 | Estados de energia | Tela apaga por timeout · botão/toque acende | ✅ |
| 5 | **Dígitos grandes** | HH:MM em dígitos grandes ocupando boa parte da tela, vindos do RTC | ✅ |
| 5.1 | Sono leve | CPU dorme entre eventos (tela apagada); mede o consumo · **autonomia ≥ 1 dia** | ⬜ |
| — | Levantar o braço (IMU) | Gesto de erguer o pulso acende a tela (QMI8658) | ⬜ |
| 5.2 | Sono profundo | Deep sleep com tela apagada, acordando por **BOOT / alarme RTC / IMU** · **autonomia ≥ 3 dias** | ⬜ |
| 5.3 | Ajuste fino | Fecha a meta no perfil típico (~100 ativações/dia × 5 s, sem Wi-Fi) · **autonomia 5 dias** | ⬜ |
| 6 | Acerto da hora | NTP oportunista grava no RTC e desliga Wi-Fi · acerto manual | ⬜ |

Status: ⬜ pendente · 🟨 em andamento · ✅ concluída

> **Ordem da Fase 5 (energia):** primeiro os **dígitos grandes** (visual), depois a escada de bateria — sono leve (≥1 dia) → IMU/levantar o braço → sono profundo (≥3 dias) → ajuste fino (5 dias). Cada degrau mede o consumo real na placa e anota em `HARDWARE.md` antes de avançar. Levantar o braço entra entre 5.1 e 5.2 porque o IMU já precisa estar acordando a tela antes de a CPU entrar em deep sleep.
