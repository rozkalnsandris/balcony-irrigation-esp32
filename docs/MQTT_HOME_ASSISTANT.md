# MQTT un Home Assistant

## Brokeris

- Host: `192.168.0.180`
- Port: `1883`
- ESP32 client ID bāze: `balkons_esp32`

Lietotājvārds un parole ir tikai `include/secrets.h` un netiek glabāti Git.

## Galvenie MQTT topic

| Topic | Virziens | Nozīme |
|---|---|---|
| `balkons/status` | ESP32 → MQTT | retained `online/offline` availability |
| `balkons/sukna/komanda` | HA → ESP32 | `ON` / `OFF` |
| `balkons/sukna/stends` | ESP32 → HA | retained `ON` / `OFF` |
| `balkons/log` | ESP32 → MQTT | notikumu logi |
| `balkons/cmd` | RPi5/bridge → ESP32 | teksta komandas |
| `balkons/telegram_out` | ESP32 → RPi5 | Telegram izejošās ziņas |
| `balkons/pukeN/mitrums` | ESP32 → HA | mitruma kategorija katram sensoram |

## Home Assistant discovery

ESP32 publicē 15 sensorus un vienu sūkņa switch zem `homeassistant/.../config`. Ierīces identitāte: `balkons_esp32`, nosaukums “Balkona Laistīšana”.

Discovery payloadi ir retained; availability tiek publicēta `balkons/status`, ieskaitot MQTT Last Will `offline`. Home Assistant dokumentācija atbalsta gan retained discovery, gan availability topic modeli.

Svarīga semantika: HA `ON` pašreizējā firmware sāk sūkni ar firmware maksimālo limitu (180 s), nevis ar 30 s noklusējumu. Lokālais hard-limit tik un tā izslēdz sūkni. Šī uzvedība bootstrap auditā nav klusām mainīta.

## Teksta komandas

- `laist` — sāk 30 s vai pagarina aktīvo sesiju par 30 s;
- `laist_X` — `X` minūtes, bet ne vairāk par firmware 3 min limitu;
- `stop` — manuāli OFF;
- `mitrums` — sensoru kategoriju pārskats;
- `raw` — RAW ADC pārskats;
- `statuss` — sūkņa/Wi-Fi/MQTT/heap/laika/uptime statuss;
- `statistika` — laistīšanas statistika kopš boot.
