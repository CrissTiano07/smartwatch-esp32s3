# HANDOFF — estado atual

> Atualizar ao **fim de cada sessão**. Primeira coisa que a IA lê, junto com `CLAUDE.md`.

**Última atualização:** 2026-11-16
**Fase atual:** 2 — RTC PCF85063

## Feito
- Documentação inicial: CLAUDE.md, SPEC, HARDWARE, ROADMAP, DECISIONS, HANDOFF
- Repositório no GitHub: CrissTiano07/smartwatch-esp32s3
- ESP-IDF v5.5.3 recuperado (`install.bat esp32s3`); `idf.py --version` → v5.5.3 ✅
- **Fase 0 concluída:** build + flash + monitor; log "boot ok" e batimento a cada 5 s ✅
- **Fase 1 concluída:** componentes `bsp_i2c` e `axp2101`. Na placa: scan achou 0x18, 0x34, 0x40, 0x51, 0x6B; bateria 4110 mV, 100% (gauge), USB sim, parado/cheio ✅
- **Fase 2 codificada (aguardando teste na placa):** componente `pcf85063` (0x51). Lê/grava hora em UTC (burst, BCD), usa a flag OS como "hora válida?", semeia pela hora de build quando OS=1. `main.c` loga `RTC: ... UTC (válida/INVÁLIDA)` a cada 5 s. Build ✅ (validado com `idf.py.exe build`).
- Config: `.claude/settings.json` libera build e gits de leitura sem prompt; CLAUDE.md ganhou seção "Branches".

## Como abrir o ambiente (todo terminal novo)
```
C:\esp\v5.5.3\esp-idf\export.bat
```
Depois: `idf.py build` · `idf.py -p COM9 flash` · `idf.py -p COM9 monitor`

## Próximo passo (testar Fase 2 na placa)
1. `idf.py -p COM9 flash monitor`. No 1º boot após falta de energia, o log deve mostrar `hora semeada da build (...)` e depois `RTC: ... UTC (válida)` a cada 5 s.
2. **Reset** (botão RST ou `Ctrl+T Ctrl+R`): a hora deve continuar andando e seguir `válida` — isso cumpre o critério de aceite.
3. Se a hora estiver errada (outro firmware deixou OS=0 com hora ruim): `idf.py menuconfig` → "RTC (PCF85063)" → ligar **RTC_FORCAR_SEMENTE** → flash (grava a hora de build) → desligar a opção → flash de novo. Daí em diante o RTC manda.
4. Com o critério cumprido na placa, marcar Fase 2 ✅ no ROADMAP e seguir para a Fase 3 (tela + LVGL).

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
