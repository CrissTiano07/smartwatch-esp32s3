# HANDOFF — estado atual

> Atualizar ao **fim de cada sessão**. Primeira coisa que a IA lê, junto com `CLAUDE.md`.

**Última atualização:** 2026-11-17
**Fase atual:** 4 concluída ✅ (estados de energia: timeout apaga, botão acende) → próxima: 5 (sono leve/profundo + medição)

## Feito
- Documentação inicial: CLAUDE.md, SPEC, HARDWARE, ROADMAP, DECISIONS, HANDOFF
- Repositório no GitHub: CrissTiano07/smartwatch-esp32s3
- ESP-IDF v5.5.3 recuperado (`install.bat esp32s3`); `idf.py --version` → v5.5.3 ✅
- **Fase 0 concluída:** build + flash + monitor; log "boot ok" e batimento a cada 5 s ✅
- **Fase 1 concluída:** componentes `bsp_i2c` e `axp2101`. Na placa: scan achou 0x18, 0x34, 0x40, 0x51, 0x6B; bateria 4110 mV, 100% (gauge), USB sim, parado/cheio ✅
- **Fase 2 concluída:** componente `pcf85063` (0x51). Lê/grava hora em UTC (burst, BCD), usa a flag OS como "hora válida?", semeia pela hora de build quando OS=1. `main.c` loga `RTC: ... UTC (válida/INVÁLIDA)` a cada 5 s. Na placa (2026-11-16): após desligar `RTC_FORCAR_SEMENTE` e regravar → `hora válida no RTC; não semeia` e `RTC: 2026-11-16 17:32:27 UTC (válida)` (compilado 14:31:57 local = 17:31:57 UTC; diferença = tempo de gravar/bootar). Sobrevive a reset ✅
- Config: `.claude/settings.json` libera build e gits de leitura sem prompt; CLAUDE.md ganhou seção "Branches".
- **Fase 3 concluída ✅ (tela AMOLED CO5300 + LVGL, relógio HH:MM):**
  - **Passo 1 (PSRAM):** `CONFIG_SPIRAM` em modo OCT a 80 MHz no `sdkconfig.defaults` (ESP32-S3R8). Na placa: `Found 8MB PSRAM device`, `SPI SRAM memory test OK`, `PSRAM: 8388608 bytes` ✅. `main` ganhou `REQUIRES` explícito.
  - **Passo 2 (tela):** componente `bsp_display` — barramento QSPI (SPI2), painel SH8601 com a init oficial da Waveshare, `set_gap(22,0)`, preenchimento em faixas sem malloc. Vermelho puro na placa ✅ (ordem de bytes RGB565 BE confirmada). Dep `waveshare/esp_lcd_sh8601 ==2.0.0`.
  - **Passo 3 (LVGL):** `esp_lvgl_port ==2.9.0` + `lvgl ==9.5.0`, task stack 8192, buffers RGB565 na RAM interna com DMA. Texto fixo "Smartwatch ok" ✅. Correções: `rounder_event_cb` (alinhamento 2 px — tirou o texto inclinado) e `sw_rotate=true` (tirou o erro `swap_xy`).
  - **Passo 4 (relógio):** `main` converte a hora UTC do RTC para Fortaleza (`TZ=<-03>3`, `epoch_de_utc()` supre a falta de `timegm()`) e mostra HH:MM, atualizando quando o minuto vira. Na placa (2026-11-17): HH:MM certo de Fortaleza, centralizado, minuto virando ✅.
  - **Medição (heap após subir o LVGL):** interno **175563 B** · PSRAM **8385028 B**.
- **Fase 4 concluída ✅ (estados de energia):**
  - **Botão BOOT (GPIO0):** componente `bsp_button`. ISR nas 2 bordas só enfileira (sem log na ISR); task faz **antirrebote por nível** (espera 30 ms, lê o GPIO, conta clique só na transição solto→pressionado). Corrige o repique da soltura (segurar 1 s e soltar = 1 clique). Botões documentados em HARDWARE.md; **PWR ainda em aberto** (caminho EXIO6/AXP2101 não confirmado — precisa do esquemático).
  - **Sleep/wake da tela:** `bsp_display_sleep()` = pausa LVGL (`lvgl_port_stop`) + display off (0x28) + sleep-in (0x10); `bsp_display_wake()` = sleep-out (0x11) + 120 ms + display on (0x29) + `lvgl_port_resume`. `bsp_display_redraw()` = `lv_obj_invalidate` da tela inteira.
  - **Gerenciador (main):** timeout de inatividade apaga; clique acorda (na hora, pela task do botão) e reinicia o timeout; transições protegidas por mutex. No wake, repinta a hora atual (`mostrar_hora(true)`) + `redraw`. Decisão de produto: apagar é **só por timeout** (DECISIONS #11, SPEC R5/R6).
  - Na placa (2026-11-17): apaga aos 15 s; botão acorda 1× sem repique; hora certa ao acordar mesmo depois de virar 1+ minutos dormindo ✅. **Timeout de teste = 15 s** (`TELA_TIMEOUT_MS` no `main.c`); ajustar o valor "de produto" depois.

## Como abrir o ambiente (todo terminal novo)
```
C:\esp\v5.5.3\esp-idf\export.bat
```
Depois: `idf.py build` · `idf.py -p COM9 flash` · `idf.py -p COM9 monitor`

## Acertar a hora manualmente (RTC_FORCAR_SEMENTE)
Quando o RTC guarda uma hora errada mas marcada como válida (OS=0, ex.: resquício de outro firmware), a semeadura automática não roda. Para forçar:
1. `idf.py menuconfig` → **"RTC (PCF85063)"** → **ligar** `RTC_FORCAR_SEMENTE`.
2. `idf.py -p COM9 flash` — grava a hora de compilação no RTC (log: `hora semeada da build (forçada)`).
3. `idf.py menuconfig` → **desligar** `RTC_FORCAR_SEMENTE`.
4. `idf.py -p COM9 flash` de novo — a partir daqui o RTC manda e a hora só anda (log: `hora válida no RTC; não semeia`).
> Deixe a opção SEMPRE desligada no dia a dia; ligada, o relógio volta à hora de compilação a cada boot.
> A semente nasce alguns segundos atrasada (tempo de compilar/gravar). O NTP da Fase 6 corrige o valor absoluto.

## Próximo passo
- **Fase 5 — sono leve/profundo + medição:** medir o consumo de cada estado (ATIVO / tela apagada / sono) e anotar em `HARDWARE.md`. Hoje a tela apaga mas a CPU segue no laço de 5 s — avaliar light/deep sleep.
- Oportunidades já mapeadas para a Fase 5 (ver "Notas da sessão"): interrupção de minuto do PCF85063 no GPIO39 (trocar o polling do relógio); desligar `CONFIG_SPIRAM_MEMTEST` p/ acelerar boot; GPIO0 é RTC-capable → serve p/ acordar do deep sleep.
- Ajustar `TELA_TIMEOUT_MS` (hoje 15 s de teste) para o valor de produto.

## Como a IA roda o build nesta máquina
`idf.py.exe build` pela ferramenta **PowerShell** (no Git Bash, `cmd /c` sofre path-mangling do MSYS e abre cmd interativo). Flash/monitor seguem com o Cristiano.

## Bloqueios / dúvidas abertas
- **Fonte com acentos (pt-BR):** as fontes Montserrat padrão do LVGL não têm caracteres acentuados. Textos atuais são sem acento ("Smartwatch ok"). Para textos pt-BR com acento, gerar uma fonte própria com os glifos necessários (ferramenta `lv_font_conv`) e habilitá-la. Pendência das fases de UI.
- Capacidade real da bateria (400 mAh?)
- Pinos QSPI da tela (pegar do demo oficial na Fase 3)
- Extensão ESP-IDF do VS Code não reconhece a instalação (exige EIM). Instalador EIM falhou ao criar venv ("non-UTF8 output"). Não bloqueia: usar terminal.
- FT3168 (touch, 0x38) não respondeu no scan. Provável sono/reset — investigar na fase do touch (não chutar)
- Bit3 de 0x00 (bateria presente) coerente, mas não testado sem bateria
- IntelliSense do VS Code mostra erros de include (sem includePath do IDF). Cosmético; o build real compila
- Aviso `bad config line 2` no submódulo micro-ecc do IDF: faz a versão do IDF aparecer como `-128-NOTFOUND` no log. Corrigir (cosmético)

## Limpeza pendente (não urgente)
- `C:\esp\v5.5.6` (tentativa EIM falha)
- `C:\Espressif\tools\python_old`

## Notas da sessão
- 2026-11-17: **(Fase 1 resolvida)** bateria: no boot apareceu `carregando` a ~4213 mV e depois `parado/cheio`. A carga sobe a ~4,2 V e assenta em ~4,1 V em repouso (LiPo normal). **Não há teto em 4,1 V** — os 4110 mV com bateria cheia são tensão de repouso. Registrado em HARDWARE.md (AXP2101). Segue aberto só a **capacidade real** (400 mAh?).
- 2026-11-17: **(bug do botão, corrigido)** antirrebote por "tempo desde o último clique" deixava o repique da **soltura** virar 2º clique em apertos longos. Trocado por antirrebote **por nível** (máquina de estado lê o GPIO após 30 ms). Fica de lição: debounce por nível é mais robusto que por janela de tempo.
- 2026-11-17: tela ~1 min atrasada vs. notebook. **Esperado:** a semente do RTC é a hora de compilação (atraso do compilar/gravar/bootar). Correção é a **Fase 6 (NTP)**, que acerta o valor absoluto e grava no RTC. Não é bug do relógio.
- 2026-11-17: **(para a Fase 5)** trocar o polling do relógio (hoje o laço lê o RTC a cada 5 s) pela **interrupção de minuto do PCF85063 no GPIO39** — acorda só quando o minuto vira. Casa com "nada de polling quando existe interrupção" (CLAUDE.md) e economiza bateria.
- 2026-11-16: **(para a Fase 5 — energia/boot)** o teste de memória da PSRAM (`CONFIG_SPIRAM_MEMTEST`, ligado por padrão) adiciona ~430 ms ao boot. Avaliar desligar para acelerar o boot quando a estabilidade da PSRAM já estiver confirmada.
- 2026-11-16: VS Code com extensão ESP-IDF instalada; Restricted Mode precisa de "Trust"
- 2026-11-16: placa é COM9 (COM8 era outro dispositivo)
- 2026-11-16: Fase 1 codificada; nota errada de VBAT (0x30/0x31 ×1,1 mV) corrigida em HARDWARE.md
- 2026-11-16: Fase 1 validada na placa; E-Gauge já calibrado (A4=100%); QMI8658 em 0x6B
