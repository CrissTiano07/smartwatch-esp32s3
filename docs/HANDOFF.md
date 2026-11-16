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

## Como abrir o ambiente (todo terminal novo)
```
C:\esp\v5.5.3\esp-idf\export.bat
```
Depois: `idf.py build` · `idf.py -p COM9 flash` · `idf.py -p COM9 monitor`

## Próximo passo
1. (Opcional, rápido) Rodar só na bateria, sem USB, e conferir no log: bit5 de 0x00 = 0 e STATUS2 = "descarregando"
2. Fase 2: módulo `pcf85063` (0x51) — ler/gravar hora em UTC, sobreviver a reset

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
