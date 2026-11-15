# SPEC — smartwatch-esp32s3

## Objetivo
Relógio de pulso que **mostra a hora certa sem depender de Wi-Fi** e dura o máximo possível na bateria.

## Requisitos (MVP)
| ID | Requisito | Critério de aceite |
|----|-----------|--------------------|
| R1 | Hora mantida pelo RTC PCF85063 | Desligar Wi-Fi/roteador por 24 h → hora continua certa (desvio < 5 s) |
| R2 | Hora sobrevive a reset e sono profundo | Após reset/deep sleep, hora volta correta sem rede |
| R3 | Acerto da hora | NTP quando houver Wi-Fi conhecido **ou** acerto manual pelo toque |
| R4 | Fuso Fortaleza | Exibe UTC-3, sem horário de verão |
| R5 | Tela apaga sozinha | Sem interação por N s → tela apaga (N configurável) |
| R6 | Acordar | Botão ou toque acende a tela em < 300 ms |
| R7 | Bateria visível | % estimada (E-Gauge do AXP2101, fallback por tensão) |
| R8 | Estabilidade | 72 h ligado sem travar/reiniciar |

## Requisitos futuros (fora do MVP)
- Levantar o pulso acende a tela (IMU QMI8658)
- Sincronização via celular por BLE
- Sons (ES8311)
- Tema visual sci-fi / animação de boot
- Velocímetro para moto (**precisa de GPS externo ou celular via BLE — placa não tem GPS**)

## Meta de energia
- Bateria: 400 mAh (confirmar)
- Meta: 5 dias → consumo médio ≤ ~3,3 mA
- Implica: tela apagada na maior parte do tempo + sono leve/profundo

## Estados de energia
| Estado | Tela | CPU | Sai por |
|--------|------|-----|---------|
| ATIVO | acesa | normal | timeout |
| DIM | brilho baixo | normal | toque/botão → ATIVO · timeout → OFF |
| OFF | apagada | sono leve | toque/botão/alarme RTC |
| SONO PROFUNDO | apagada | deep sleep | botão/alarme RTC |

## Fora de escopo
- OTA (gravação só por USB-C)
- Notificações do celular
- Arduino framework
