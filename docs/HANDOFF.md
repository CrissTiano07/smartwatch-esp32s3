# HANDOFF — estado atual

> Atualizar ao **fim de cada sessão**. Primeira coisa que a IA lê, junto com `CLAUDE.md`.

**Última atualização:** 2026-11-16
**Fase atual:** 1 — I2C + AXP2101

## Feito
- Documentação inicial: CLAUDE.md, SPEC, HARDWARE, ROADMAP, DECISIONS, HANDOFF
- Repositório no GitHub: CrissTiano07/smartwatch-esp32s3
- ESP-IDF v5.5.3 recuperado (`install.bat esp32s3`); `idf.py --version` → v5.5.3 ✅
- **Fase 0 concluída:** build + flash + monitor; log "boot ok" e batimento a cada 5 s ✅
- **Fase 1 (código pronto, falta validar na placa):** componentes `bsp_i2c` e `axp2101`; `main.c` faz scan e loga bateria + bytes brutos a cada 5 s. `idf.py build` ok, sem warnings.

## Como abrir o ambiente (todo terminal novo)
```
C:\esp\v5.5.3\esp-idf\export.bat
```
Depois: `idf.py build` · `idf.py -p COM9 flash` · `idf.py -p COM9 monitor`

## Próximo passo
1. Cristiano: `idf.py -p COM9 flash monitor` e colar o log (scan + 2–3 linhas `bat:`/`raw:`)
2. Com o log: atualizar a tabela de endereços em `HARDWARE.md` (tirar o "a confirmar")
3. Confirmar bit3 de 0x00 = bateria presente (comparar com/sem bateria ou com/sem USB)
4. Conferir mV do log contra multímetro, se possível
5. Critério cumprido → marcar Fase 1 ✅ no ROADMAP e seguir para Fase 2 (PCF85063)

## Bloqueios / dúvidas abertas
- Capacidade real da bateria (400 mAh?)
- Pinos QSPI da tela (pegar do demo oficial na Fase 3)
- Extensão ESP-IDF do VS Code não reconhece a instalação (exige EIM). Instalador EIM falhou ao criar venv ("non-UTF8 output"). Não bloqueia: usar terminal.
- Se 0xA4 ficar sempre 0 mesmo após carga completa: o E-Gauge pode estar desligado (XPowersLib liga via reg 0x18 bit3 — **não verificado**, não implementado)
- IntelliSense do VS Code mostra erros de include (sem includePath do IDF). Cosmético; o build real compila
- Aviso `bad config line 2` no submódulo micro-ecc do IDF: faz a versão do IDF aparecer como `-128-NOTFOUND` no log. Corrigir (cosmético)

## Limpeza pendente (não urgente)
- `C:\esp\v5.5.6` (tentativa EIM falha)
- `C:\Espressif\tools\python_old`

## Notas da sessão
- 2026-11-16: VS Code com extensão ESP-IDF instalada; Restricted Mode precisa de "Trust"
- 2026-11-16: placa é COM9 (COM8 era outro dispositivo)
- 2026-11-16: Fase 1 codificada; nota errada de VBAT (0x30/0x31 ×1,1 mV) corrigida em HARDWARE.md
