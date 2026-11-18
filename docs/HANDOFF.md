# HANDOFF — estado atual

> Atualizar ao **fim de cada sessão**. Primeira coisa que a IA lê, junto com `CLAUDE.md`.

**Última atualização:** 2026-11-18
**Fase atual:** 5 (dígitos grandes) concluída ✅ na placa, incluindo o refino do relógio → próxima: escada de bateria: **5.1 sono leve (≥1 dia)** → levantar o braço (IMU) → 5.2 sono profundo (≥3 dias) → 5.3 ajuste fino (5 dias). Ordem detalhada no ROADMAP.

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
  - **Timeout preciso e configurável:** vem do menuconfig **`CONFIG_SW_TELA_TIMEOUT_MS` (padrão 5000 ms**, menu "Smartwatch - Energia") e é contado por um **esp_timer one-shot** reiniciado a cada clique.
  - **Arquitetura:** uma **task de energia única** consome uma **fila de eventos** (`EV_CLIQUE`/`EV_TIMEOUT`); o callback do botão e o do esp_timer só enfileiram (não bloqueiam suas tasks). Sendo a única dona das transições, dispensa mutex e trava de corrida.
  - Na placa (2026-11-17): boot apaga em ~5 s; após o clique apaga em **Δ≈5 s** (±0,5 s) pelo log; cliques seguidos contam do último; botão acorda 1× sem repique; hora certa ao acordar mesmo depois de virar 1+ minutos dormindo ✅.
- **Fase 5 — dígitos grandes concluída ✅:**
  - **Fonte própria:** componente `ui_fonts` com `font_digits_180` (Montserrat Regular **180 px, 4 bpp, só `0-9`**), gerada por `npx lv_font_conv@1.5.3`. Reproduzível: `.ttf` + OFL em `tools/fonts/` e comando exato em `components/ui_fonts/README.md`. Decisão em DECISIONS #12.
  - **Relógio empilhado:** `bsp_display` cria **dois rótulos** com essa fonte — HH acima do centro, MM abaixo (`DIG_DY 70` px), sem `:`. Nova API `bsp_display_set_hhmm(hh, mm)` no lugar de `bsp_display_set_text`.
  - Na placa (2026-11-18): HH em cima e MM embaixo, grandes, centralizados, sem sobrepor; minuto virando; hora certa ao acordar; `DIG_DY` ok sem ajuste ✅.
  - **Aceito por ora (refino futuro):** dígitos proporcionais deslizam de lado ao trocar `1`↔outro dígito → largura fixa fica para depois (DECISIONS #12).

## Linha de base de bateria (medir contra isto)
- **2026-11-17 — teste de uso real:** **4h40** de autonomia (11:40 → 16:20), com a **CPU sempre ativa** (laço de 5 s, sem sono) e a tela apagando só por timeout. É o estado de hoje (fim da Fase 4) e o **ponto de partida** a superar na Fase 5.
- **AXP2101:** desligou **antes de a bateria zerar** (corte de proteção/limite de descarga do PMIC, não 0%). Ou seja, os 4h40 não esgotaram a capacidade real.
- **RTC (PCF85063):** **manteve a hora** mesmo após o AXP cortar — confirma a fonte da verdade da hora independente da alimentação principal (bateria de backup do RTC).
- **Meta (SPEC):** 5 dias no perfil típico (~100 ativações/dia × 5 s, sem Wi-Fi) → ≤ ~3,3 mA médios. A escada da Fase 5 mede cada degrau na placa.

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
- **Fase 5.1 — sono leve (≥1 dia):** fazer a CPU dormir entre eventos com a tela apagada; medir o consumo e anotar em `HARDWARE.md`. Ponto de partida a superar: linha de base 4h40 (abaixo).
- **Depois, o resto da escada de bateria** (cada degrau mede o consumo real e anota em `HARDWARE.md` antes de avançar):
  - **5.1 sono leve** (CPU dorme entre eventos, tela apagada) → autonomia ≥ 1 dia.
  - **Levantar o braço (IMU QMI8658)** → gesto acende a tela (entra antes do deep sleep).
  - **5.2 sono profundo** (deep sleep) acordando por **BOOT / alarme RTC / IMU** → autonomia ≥ 3 dias.
  - **5.3 ajuste fino** → fechar os 5 dias no perfil típico.
- Oportunidades já mapeadas para a escada (ver "Notas da sessão"): interrupção de minuto do PCF85063 no GPIO39 (trocar o polling do relógio); desligar `CONFIG_SPIRAM_MEMTEST` p/ acelerar boot; GPIO0 é RTC-capable → serve p/ acordar do deep sleep.
- Timeout da tela é configurável em `idf.py menuconfig` → "Smartwatch - Energia" (`CONFIG_SW_TELA_TIMEOUT_MS`).

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
- 2026-11-18: **(retorno de uso real + reorganização da Fase 5)** linha de base **4h40** com CPU sempre ativa; AXP cortou antes de zerar e o RTC segurou a hora (registrado acima). Nova ordem da Fase 5: **dígitos grandes → 5.1 sono leve (≥1 dia) → IMU (levantar o braço) → 5.2 sono profundo (≥3 dias) → 5.3 ajuste fino (5 dias)**. Pedidos do dono: dígitos grandes, acender ao levantar o braço, meta de 5 dias no perfil típico. SPEC e ROADMAP atualizados.
- 2026-11-17: **(Fase 1 resolvida)** bateria: no boot apareceu `carregando` a ~4213 mV e depois `parado/cheio`. A carga sobe a ~4,2 V e assenta em ~4,1 V em repouso (LiPo normal). **Não há teto em 4,1 V** — os 4110 mV com bateria cheia são tensão de repouso. Registrado em HARDWARE.md (AXP2101). Segue aberto só a **capacidade real** (400 mAh?).
- 2026-11-17: **(bug do botão, corrigido)** antirrebote por "tempo desde o último clique" deixava o repique da **soltura** virar 2º clique em apertos longos. Trocado por antirrebote **por nível** (máquina de estado lê o GPIO após 30 ms). Fica de lição: debounce por nível é mais robusto que por janela de tempo.
- 2026-11-17: tela ~1 min atrasada vs. notebook. **Esperado:** a semente do RTC é a hora de compilação (atraso do compilar/gravar/bootar). Correção é a **Fase 6 (NTP)**, que acerta o valor absoluto e grava no RTC. Não é bug do relógio.
- 2026-11-17: **(para a Fase 5)** trocar o polling do relógio (hoje o laço lê o RTC a cada 5 s) pela **interrupção de minuto do PCF85063 no GPIO39** — acorda só quando o minuto vira. Casa com "nada de polling quando existe interrupção" (CLAUDE.md) e economiza bateria.
- 2026-11-16: **(para a Fase 5 — energia/boot)** o teste de memória da PSRAM (`CONFIG_SPIRAM_MEMTEST`, ligado por padrão) adiciona ~430 ms ao boot. Avaliar desligar para acelerar o boot quando a estabilidade da PSRAM já estiver confirmada.
- 2026-11-16: VS Code com extensão ESP-IDF instalada; Restricted Mode precisa de "Trust"
- 2026-11-16: placa é COM9 (COM8 era outro dispositivo)
- 2026-11-16: Fase 1 codificada; nota errada de VBAT (0x30/0x31 ×1,1 mV) corrigida em HARDWARE.md
- 2026-11-16: Fase 1 validada na placa; E-Gauge já calibrado (A4=100%); QMI8658 em 0x6B
