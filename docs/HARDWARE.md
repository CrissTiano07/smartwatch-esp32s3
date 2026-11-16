# HARDWARE — Waveshare ESP32-S3-Touch-AMOLED-2.06

Fonte oficial: https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-2.06

## Placa
- SoC: ESP32-S3R8 · 8 MB PSRAM · 32 MB flash · microSD
- Tela: AMOLED 2.06" 410×502, controlador CO5300 (QSPI) · touch FT3168
- USB-C nativo (GPIO19/20) — gravação e log · aparece como **COM9** (VID_303A)

## Confirmado no 1º boot (2026-11-16)
- ESP32-S3 (QFN56) rev v0.2 · 2 núcleos · cristal 40 MHz
- Flash 32 MB (chip GD) · PSRAM 8 MB detectada pelo esptool
- PSRAM **ainda não habilitada** no sdkconfig → necessária para LVGL (Fase 3)
- Gravar com placa travada: segurar BOOT e conectar o cabo

## Barramento I2C compartilhado — SCL GPIO14 · SDA GPIO15
| Chip | Função | Endereço* | Interrupção |
|------|--------|-----------|-------------|
| AXP2101 | energia, carga, bateria | 0x34 | — |
| PCF85063 | RTC (hora) | 0x51 | GPIO39 |
| QMI8658 | IMU 6 eixos | 0x6A/0x6B | GPIO21 |
| FT3168 | touch | 0x38 | GPIO38 |
| ES8311 | codec de áudio (saída) | 0x18 | — |
| ES7210 | ADC de microfones | 0x40 | — |

\* **Endereços a confirmar no scan I2C da Fase 1.** Atualizar esta tabela com o resultado real.

## Outros pinos
| GPIO | Uso |
|------|-----|
| 13 | LCD_TE (sincronismo de tela) |
| 16 | I2S_MCLK (ES8311/ES7210) |
| 17 | SD_CS |
| 0 | BOOT |

Pinos QSPI da tela: **pendente** — copiar do esquemático/demo oficial na Fase 3.

## Bibliotecas de referência
- XPowersLib (AXP2101) — lewisxhe
- SensorLib (PCF85063, QMI8658) — lewisxhe
- Demo oficial Waveshare (ESP-IDF)

## Lições do firmware antigo (v6.3)
- E-Gauge do AXP2101 (reg 0xA4) retorna 0% até um ciclo completo de carga/descarga → usar fallback por tensão (ver registradores abaixo)
- Curva LiPo usada: 4200 mV=100% · 3700=50% · 3200=10% · 3000=0%
- ~~A nota antiga "regs 0x30/0x31, ×1,1 mV" estava errada~~ (0x30 é controle do ADC, não leitura).

## AXP2101 — registradores usados (fonte: driver Rockbox + XPowersLib)
| Reg | Nome | Uso |
|-----|------|-----|
| 0x00 | STATUS1 | bit5 = VBUS ok (USB presente) · bit3 = bateria presente (*confirmar pelo log*) |
| 0x01 | STATUS2 | bits 6:5 → 0 parado/cheio · 1 carregando · 2 descarregando |
| 0x30 | ADC enable | bit0 = medição de VBAT. Sempre leitura-modificação-escrita |
| 0x34/0x35 | VBAT H/L | 14 bits, **1 mV/unidade**: `((H & 0x3F) << 8) \| L` |
| 0xA4 | Bateria % | E-Gauge; 0 até calibrar → fallback por tensão |
- Crashes antigos: stack pequeno (4096→8192), `lv_malloc` em loop, fonte LVGL não habilitada, loop de retry do Wi-Fi
