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
