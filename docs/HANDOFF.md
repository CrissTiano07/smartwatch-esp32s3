# HANDOFF — estado atual

> Atualizar ao **fim de cada sessão**. Primeira coisa que a IA lê, junto com `CLAUDE.md`.

**Última atualização:** 2026-11-16
**Fase atual:** 2 concluída ✅ → próxima: 3 (tela + LVGL)

## Feito
- Documentação inicial: CLAUDE.md, SPEC, HARDWARE, ROADMAP, DECISIONS, HANDOFF
- Repositório no GitHub: CrissTiano07/smartwatch-esp32s3
- ESP-IDF v5.5.3 recuperado (`install.bat esp32s3`); `idf.py --version` → v5.5.3 ✅
- **Fase 0 concluída:** build + flash + monitor; log "boot ok" e batimento a cada 5 s ✅
- **Fase 1 concluída:** componentes `bsp_i2c` e `axp2101`. Na placa: scan achou 0x18, 0x34, 0x40, 0x51, 0x6B; bateria 4110 mV, 100% (gauge), USB sim, parado/cheio ✅
- **Fase 2 concluída:** componente `pcf85063` (0x51). Lê/grava hora em UTC (burst, BCD), usa a flag OS como "hora válida?", semeia pela hora de build quando OS=1. `main.c` loga `RTC: ... UTC (válida/INVÁLIDA)` a cada 5 s. Na placa (2026-11-16): após desligar `RTC_FORCAR_SEMENTE` e regravar → `hora válida no RTC; não semeia` e `RTC: 2026-11-16 17:32:27 UTC (válida)` (compilado 14:31:57 local = 17:31:57 UTC; diferença = tempo de gravar/bootar). Sobrevive a reset ✅
- Config: `.claude/settings.json` libera build e gits de leitura sem prompt; CLAUDE.md ganhou seção "Branches".

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
- **Fase 3:** tela AMOLED (CO5300, QSPI) + LVGL mínimo → relógio digital HH:MM vindo do RTC.
- Antes: habilitar PSRAM no sdkconfig (necessária para LVGL — ver nota na seção "Confirmado no 1º boot" do HARDWARE.md) e pegar os pinos QSPI da tela no esquemático/demo oficial da Waveshare.

## Como a IA roda o build nesta máquina
`idf.py.exe build` pela ferramenta **PowerShell** (no Git Bash, `cmd /c` sofre path-mangling do MSYS e abre cmd interativo). Flash/monitor seguem com o Cristiano.

## Bloqueios / dúvidas abertas
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
- 2026-11-16: VS Code com extensão ESP-IDF instalada; Restricted Mode precisa de "Trust"
- 2026-11-16: placa é COM9 (COM8 era outro dispositivo)
- 2026-11-16: Fase 1 codificada; nota errada de VBAT (0x30/0x31 ×1,1 mV) corrigida em HARDWARE.md
- 2026-11-16: Fase 1 validada na placa; E-Gauge já calibrado (A4=100%); QMI8658 em 0x6B
