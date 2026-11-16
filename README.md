# smartwatch-esp32s3

Firmware de smartwatch para a Waveshare ESP32-S3-Touch-AMOLED-2.06 (ESP-IDF v5.5.3).
Hora mantida pelo RTC, sem depender de Wi-Fi. Foco em bateria.

- Produto: [docs/SPEC.md](docs/SPEC.md)
- Hardware: [docs/HARDWARE.md](docs/HARDWARE.md)
- Fases: [docs/ROADMAP.md](docs/ROADMAP.md)
- Estado atual: [docs/HANDOFF.md](docs/HANDOFF.md)

## Build
```
idf.py build
idf.py -p COM9 flash
idf.py -p COM9 monitor
```
