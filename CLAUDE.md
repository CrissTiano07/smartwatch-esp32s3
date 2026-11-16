# CLAUDE.md — instruções para o agente de IA

Leia este arquivo e `docs/HANDOFF.md` **antes de qualquer ação**.

## Projeto
Firmware de **smartwatch** (relógio de pulso) para a placa Waveshare ESP32-S3-Touch-AMOLED-2.06.
Prioridades, nesta ordem: **1) hora certa sem Wi-Fi · 2) bateria · 3) estabilidade · 4) visual**.

- Spec do produto: `docs/SPEC.md`
- Hardware e pinos: `docs/HARDWARE.md`
- Fases e critérios de aceite: `docs/ROADMAP.md`
- Decisões já tomadas (não reabrir sem motivo novo): `docs/DECISIONS.md`
- Estado atual / próximo passo: `docs/HANDOFF.md`

## Ambiente
- ESP-IDF **v5.5.3** em `C:\esp\v5.5.3\esp-idf`, Windows, porta **COM9** (USB nativa, VID_303A)
- Todo terminal novo: `C:\esp\v5.5.3\esp-idf\export.bat` (a extensão ESP-IDF do VS Code não reconhece esta instalação)
- Build/flash/monitor: `idf.py build` · `idf.py -p COM9 flash` · `idf.py -p COM9 monitor`
- Sair do monitor (teclado ABNT2): **Ctrl+T** e depois **Ctrl+X** (o `Ctrl+]` padrão não funciona no ABNT2)
- Cristiano faz flash e monitor no terminal dele; a IA só valida com `idf.py.exe build`

## Modo de trabalho (obrigatório)
O dono do projeto (Cristiano) está **aprendendo firmware**. Ele precisa entender cada linha.
1. Antes de codar: diga em 2–4 linhas o que vai fazer e por quê.
2. Uma mudança por vez. Pequena. Compilável.
3. Código comentado em pt-BR explicando o *porquê*, não o óbvio.
4. Depois de cada mudança: entregue **título + descrição curta do commit** (Conventional Commits).
5. Nunca pule fase do `ROADMAP.md`. Só avance com o critério de aceite cumprido no hardware real.
6. Na dúvida sobre hardware, consulte `HARDWARE.md`; se não estiver lá, pergunte ou peça um teste — não chute.
7. Ao fim da sessão: atualize `docs/HANDOFF.md`. Decisão nova → registre em `docs/DECISIONS.md`.

## Regras técnicas
- Hora interna sempre em **UTC**; fuso só na exibição (`TZ="<-03>3"`, Fortaleza, sem horário de verão).
- **PCF85063 é a fonte da verdade da hora.** Wi-Fi/NTP só corrige, depois desliga.
- Barramento I2C compartilhado: um único handle (`i2c_master`), acesso só pelo módulo `bsp_i2c`.
- Nada de polling quando existe interrupção (touch, IMU, RTC, botão).
- Evitar `malloc` em loop. Preferir alocação estática. Monitorar stack (`uxTaskGetStackHighWaterMark`).
- Wi-Fi desligado por padrão.
- Textos na tela: curtos.

## Commits
Formato: `tipo(escopo): resumo` — tipos: `feat`, `fix`, `docs`, `refactor`, `chore`, `test`.
Exemplo: `feat(rtc): lê e grava hora no PCF85063`
