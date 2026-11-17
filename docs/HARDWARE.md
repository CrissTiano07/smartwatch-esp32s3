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
| Chip | Função | Endereço | Scan 2026-11-16 | Interrupção |
|------|--------|----------|-----------------|-------------|
| AXP2101 | energia, carga, bateria | 0x34 | ✅ | — |
| PCF85063 | RTC (hora) | 0x51 | ✅ | GPIO39 |
| QMI8658 | IMU 6 eixos | **0x6B** | ✅ (0x6A não responde) | GPIO21 |
| FT3168 | touch | 0x38 | ❌ não respondeu | GPIO38 |
| ES8311 | codec de áudio (saída) | 0x18 | ✅ | — |
| ES7210 | ADC de microfones | 0x40 | ✅ | — |

FT3168 ausente no scan: provável sono/reset do controlador de touch (**não verificado**).
Investigar na fase do touch — ver pino de reset no esquemático/demo oficial.

## Botões físicos (Fase 4)
A placa tem **dois botões laterais: BOOT e PWR** (wiki oficial, FAQ "Can both buttons be programmed?").

| Botão | Como chega ao ESP32 | Pressionado = | Confiança |
|-------|---------------------|---------------|-----------|
| **BOOT** | **GPIO0 direto** | nível **baixo** (0) | ✅ confirmado |
| **PWR** | wiki diz "via **EXIO6** de um IO estendido"; **não confirmado** | nível **alto** (1) | 🟨 em aberto |

- **BOOT (GPIO0):** a wiki diz "the GPIO0 can detect the high and low level of the button... low level is pressed". É o que usamos na Fase 4 (componente `bsp_button`, interrupção na borda de descida). GPIO0 é pino de *strapping* (BOOT): ao **ligar** segurando-o, entra em modo download; em **runtime** é entrada normal.
- **PWR — como chega ao ESP32 (em aberto):**
  - A wiki fala em "**EXIO6** detection buttons of the extended IO" — ou seja, um **expansor de I/O** por I2C. **Mas o nosso scan I2C (0x18, 0x34, 0x40, 0x51, 0x6B) não achou expansor nenhum** (endereços típicos 0x20–0x27 vazios). Então ou a frase foi copiada de outra placa, ou o PWR passa por outro caminho.
  - **Não há linha de IRQ do PWR mapeada a um GPIO conhecido.** A BSP oficial não ajuda: tem `BSP_CAPS_BUTTONS 0`, nenhuma macro de botão, e o comentário "ESP-SparkBot-BSP pinout" (header copiado de outra placa).
  - **Via AXP2101?** Não confirmado. O comportamento "**segurar ~6 s desliga a placa**" e "**clique liga quando desligada**" é **hardware do PMU** (típico do pino PWRON do AXP2101) e **independe do nosso firmware**. Se o PWR for lido por software, seria pelos registradores de IRQ de power-key do AXP2101 — **não verificado** (não mexemos nesses regs ainda).
  - **Para fechar:** ler o esquemático oficial (`ESP32-S3-Touch-AMOLED-2.06.pdf` no site da Waveshare) ou o código de demo oficial. Enquanto isso, a Fase 4 usa só o BOOT.

## Outros pinos
| GPIO | Uso |
|------|-----|
| 13 | LCD_TE (sincronismo de tela) |
| 16 | I2S_MCLK (ES8311/ES7210) |
| 17 | SD_CS |
| 0 | BOOT (botão; runtime = entrada por interrupção — Fase 4) |

## Tela AMOLED CO5300 — QSPI (confirmado na Fase 3)
Fonte: BSP oficial da Waveshare, repo `waveshareteam/Waveshare-ESP32-components`,
commit `9daffb7`, arquivos `bsp/esp32_s3_touch_amoled_2_06/include/bsp/`
(`esp32_s3_touch_amoled_2_06.h`, `display.h`) e o `.c` da BSP.

| Sinal | GPIO | Observação |
|-------|------|------------|
| CS | 12 | chip select |
| SCLK (PCLK) | 11 | clock QSPI |
| D0 (SIO0) | 4 | linha de dados 0 |
| D1 | 5 | linha de dados 1 |
| D2 | 6 | linha de dados 2 |
| D3 | 7 | linha de dados 3 |
| RST | 8 | reset do painel |
| Backlight | — | não há; brilho via comando do painel `0x51` |

- Host SPI: **SPI2_HOST** · clock **40 MHz** · modo QSPI (4 linhas de dados).
- Painel: **410 × 502**, RGB565 **big-endian**.
- **Offset de coluna:** `esp_lcd_panel_set_gap(panel, 22, 0)` (x=22 / 0x16, y=0). Sem isso a imagem sai deslocada.
- Driver usado pela Waveshare nesta placa: `esp_lcd_sh8601` (o CO5300 compartilha o conjunto de comandos QSPI do SH8601).
- **A tela NÃO depende do AXP2101 por software** — a BSP não liga nenhum regulador (LDO) antes de inicializar o painel.
- TE (GPIO13) existe no hardware, mas a BSP oficial **não** o utiliza (sem sincronismo por TE).

## Bibliotecas de referência
- XPowersLib (AXP2101) — lewisxhe
- SensorLib (PCF85063, QMI8658) — lewisxhe
- Demo oficial Waveshare (ESP-IDF)

## Lições do firmware antigo (v6.3)
- E-Gauge do AXP2101 (reg 0xA4) retorna 0% até um ciclo completo de carga/descarga → usar fallback por tensão (ver registradores abaixo). Nesta placa, em 2026-11-16, já retornava 100% (calibrado).
- Curva LiPo usada: 4200 mV=100% · 3700=50% · 3200=10% · 3000=0%
- ~~A nota antiga "regs 0x30/0x31, ×1,1 mV" estava errada~~ (0x30 é controle do ADC, não leitura).
- Crashes antigos: stack pequeno (4096→8192), `lv_malloc` em loop, fonte LVGL não habilitada, loop de retry do Wi-Fi

## AXP2101 — registradores usados (fonte: driver Rockbox + XPowersLib)
| Reg | Nome | Uso |
|-----|------|-----|
| 0x00 | STATUS1 | bit5 = VBUS ok (USB presente) · bit3 = bateria presente (coerente no log; não testado sem bateria) |
| 0x01 | STATUS2 | bits 6:5 → 0 parado/cheio · 1 carregando · 2 descarregando |
| 0x30 | ADC enable | bit0 = medição de VBAT. Sempre leitura-modificação-escrita. Padrão no boot: 0x03 |
| 0x34/0x35 | VBAT H/L | 14 bits, **1 mV/unidade**: `((H & 0x3F) << 8) \| L` |
| 0xA4 | Bateria % | E-Gauge; 0 até calibrar → fallback por tensão |

Leitura real (2026-11-16, USB + bateria, cheia): `00=28 01=14 34=10 35=0E A4=64`
→ 4110 mV (±2 mV entre leituras) · 100% · VBUS sim · bateria sim · parado/cheio.

## PCF85063 — registradores de hora (fonte: datasheet NXP PCF85063A)
Endereço I2C **0x51** · INT no **GPIO39** (não usado ainda; reservado para alarme/wake nas fases de energia).
Todos os registradores de hora estão em **BCD** (cada dígito decimal num nibble; ex.: 59 = `0x59`).

| Reg | Campo | Bits úteis | Observação |
|-----|-------|-----------|------------|
| 0x04 | Segundos | 6:0 (BCD 00–59) | **bit7 = OS** (Oscillator Stop) |
| 0x05 | Minutos | 6:0 (BCD 00–59) | |
| 0x06 | Horas | 5:0 (BCD 00–23) | modo 24h (padrão do chip) |
| 0x07 | Dias | 5:0 (BCD 01–31) | |
| 0x08 | Dia da semana | 2:0 (0–6) | **não é BCD**; 0 = domingo (casa com `tm_wday`) |
| 0x09 | Mês | 4:0 (BCD 01–12) | |
| 0x0A | Ano | 7:0 (BCD 00–99) | offset 2000 no firmware |

- 0x04–0x0A são contíguos → lidos/gravados em **uma transação I2C (burst)**, para não pegar/gravar um instante partido na virada do segundo.
- **Flag OS (bit7 de 0x04):** `1` = oscilador parou (ex.: faltou energia) → hora **não confiável**; `0` = hora válida. Fica em 1 até o software zerar. Gravar os segundos com bit7=0 já zera. Reset do ESP32 **não** mexe no RTC (alimentação própria), então após reset o OS continua 0 e a hora segue.
- Modo 24h é o padrão do chip (bit 12/24 de Control_1 = 0). Se outro firmware tiver deixado em 12h, a leitura das horas sairia errada — não observado nesta placa; investigar só se aparecer.
