# HANDOFF — estado atual

> Atualizar ao **fim de cada sessão**. Primeira coisa que a IA lê, junto com `CLAUDE.md`.

**Última atualização:** 2026-11-16
**Fase atual:** 1 — I2C + AXP2101

## Feito
- Documentação inicial: CLAUDE.md, SPEC, HARDWARE, ROADMAP, DECISIONS, HANDOFF
- Repositório no GitHub: CrissTiano07/smartwatch-esp32s3
- ESP-IDF v5.5.3 recuperado (`install.bat esp32s3`); `idf.py --version` → v5.5.3 ✅
- **Fase 0 concluída:** build + flash + monitor; log "boot ok" e batimento a cada 5 s ✅

## Como abrir o ambiente (todo terminal novo)
```
C:\esp\v5.5.3\esp-idf\export.bat
```
Depois: `idf.py build` · `idf.py -p COM9 flash` · `idf.py -p COM9 monitor`

## Próximo passo
1. Fase 1: scan do barramento I2C (GPIO14/15) e confirmar endereços em `HARDWARE.md`
2. Ler tensão e % da bateria no AXP2101

## Bloqueios / dúvidas abertas
- Capacidade real da bateria (400 mAh?)
- Pinos QSPI da tela (pegar do demo oficial na Fase 3)
- Extensão ESP-IDF do VS Code não reconhece a instalação (exige EIM). Instalador EIM falhou ao criar venv ("non-UTF8 output"). Não bloqueia: usar terminal.
- Aviso `bad config line 2` no submódulo micro-ecc do IDF: faz a versão do IDF aparecer como `-128-NOTFOUND` no log. Corrigir (cosmético)

## Limpeza pendente (não urgente)
- `C:\esp\v5.5.6` (tentativa EIM falha)
- `C:\Espressif\tools\python_old`

## Notas da sessão
- 2026-11-16: VS Code com extensão ESP-IDF instalada; Restricted Mode precisa de "Trust"
- 2026-11-16: placa é COM9 (COM8 era outro dispositivo)
