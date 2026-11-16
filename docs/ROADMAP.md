# ROADMAP

Regra: só avança com o critério de aceite cumprido **na placa**. Cada fase = commits pequenos.

| Fase | Entrega | Critério de aceite | Status |
|------|---------|--------------------|--------|
| 0 | Repositório + projeto IDF mínimo | `idf.py build` ok · log "boot ok" no monitor · 1º commit no GitHub | ✅ |
| 1 | I2C + AXP2101 | Scan lista os chips · log mostra tensão e % da bateria | ✅ |
| 2 | RTC PCF85063 | Grava hora · reset · hora continua certa no log, sem Wi-Fi | ⬜ |
| 3 | Tela + LVGL mínimo | Relógio digital HH:MM na tela, vindo do RTC | ⬜ |
| 4 | Estados de energia | Tela apaga por timeout · botão/toque acende | ⬜ |
| 5 | Sono leve/profundo + medição | Consumo medido de cada estado anotado em `HARDWARE.md` | ⬜ |
| 6 | Acerto da hora | NTP oportunista grava no RTC e desliga Wi-Fi · acerto manual | ⬜ |
| 7 | Levantar o pulso | IMU acorda a tela | ⬜ |

Status: ⬜ pendente · 🟨 em andamento · ✅ concluída
