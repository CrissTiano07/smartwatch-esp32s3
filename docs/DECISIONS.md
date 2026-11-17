# DECISIONS — registro de decisões

Formato: data · decisão · motivo. Não reabrir sem fato novo.

| # | Data | Decisão | Motivo |
|---|------|---------|--------|
| 1 | 2026-11-15 | Recomeçar do zero; firmware v6.3 só como consulta | Base antiga com crashes acumulados; objetivo de entender cada linha |
| 2 | 2026-11-15 | Nome: `smartwatch-esp32s3` | Nome descreve o produto |
| 3 | 2026-11-15 | ESP-IDF v5.5.3 (não Arduino) | Controle fino de energia; já instalado |
| 4 | 2026-11-15 | VS Code + extensão ESP-IDF | Padrão da comunidade; Git visual |
| 5 | 2026-11-15 | PCF85063 é a fonte da verdade da hora; Wi-Fi só corrige | Relógio não pode depender de rede |
| 6 | 2026-11-15 | Sem OTA | Complexidade desnecessária; USB-C nativo basta |
| 7 | 2026-11-15 | Hora interna em UTC; fuso só na exibição | Evita bugs de fuso |
| 8 | 2026-11-16 | Semear o RTC com a hora de compilação (convertida p/ UTC) quando OS=1 | Dá uma hora inicial sem Wi-Fi; NTP corrige na Fase 6. Opção `RTC_FORCAR_SEMENTE` força mesmo com OS=0, p/ sobrescrever hora errada deixada por outro firmware |
| 9 | 2026-11-16 | Tela CO5300 controlada pelo driver `waveshare/esp_lcd_sh8601` (==2.0.0), não pela BSP completa nem pelo `esp_lcd_co5300` | É o driver que a BSP oficial da Waveshare usa nesta placa; o CO5300 compartilha o protocolo QSPI do SH8601. Usar a init validada maximiza acerto no hardware real. Driver só, não a BSP inteira, para manter o código enxuto e entendível (regra do projeto) |
| 10 | 2026-11-16 | LVGL via `espressif/esp_lvgl_port` (==2.9.0); buffers de desenho na RAM interna com DMA (parcial, 40 linhas, duplo) | Evita reimplementar à mão a task do LVGL, o tick e a thread-safety (fonte dos crashes do firmware antigo). Buffers na RAM interna seguindo a BSP oficial; PSRAM fica reservada p/ buffers maiores se for preciso |
| 11 | 2026-11-17 | **A tela apaga SÓ por timeout.** Com a tela apagada, o clique do botão acende e reinicia o timeout. Com a tela acesa, o clique **só reinicia o timeout** (não apaga) e fica reservado p/ funções futuras (navegação/menu) | Comportamento previsível: o usuário nunca apaga a tela "sem querer" no clique; economia garantida pelo timeout. Deixa o clique livre p/ virar entrada de UI nas próximas fases. Afeta R5/R6 na SPEC. Fase 4 usa o botão **BOOT (GPIO0)**; o PWR fica p/ depois (ver HARDWARE.md) |
