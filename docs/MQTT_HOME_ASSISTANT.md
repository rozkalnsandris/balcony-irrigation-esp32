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

Discovery payloadi ir retained; availability tiek publicēta `balkons/status`, ieskaitot MQTT Last Will `offline`.

Svarīga semantika: HA `ON` pašreizējā firmware sāk sūkni ar firmware maksimālo limitu (180 s), nevis ar 30 s noklusējumu. Lokālais hard-limit tik un tā izslēdz sūkni. Šī uzvedība bootstrap auditā nav klusām mainīta.

## Teksta komandas

- `laist` — sāk 30 s vai pagarina aktīvo sesiju par 30 s;
- `laist_X` — `X` minūtes, bet ne vairāk par firmware 3 min limitu;
- `stop` — manuāli OFF;
- `mitrums` — sensoru kategoriju pārskats;
- `raw` — RAW ADC pārskats;
- `statuss` — sūkņa/Wi-Fi/MQTT/heap/laika/uptime statuss;
- `statistika` — laistīšanas statistika kopš boot.

## Telegram bridge vēsturiskais kontrakts

Telegram TLS tika pārvietots no ESP32 uz RPi5. ESP32 publicē izejošās ziņas uz `balkons/telegram_out` un saņem teksta komandas caur `balkons/cmd`.

Claude-era RPi5 auditā bija redzami `balkons-bot.service` un `balkons-log.service`; citā incidenta logā `balkons-bot.service` atkārtoti krita. Tas ir iemesls, kāpēc sūkņa lokālais taimeris nedrīkst būt atkarīgs no Telegram bridge veselības.

Aktuālais RPi5 source vēl jāpaņem no dzīva hosta un jāsanitizē pirms commit.

## Vēsturiskais `laistisana.sh` un HA vadības incidents

Atgūtajā vēsturē `laistisana.sh` izmantoja Home Assistant REST API ar retry, kontrolētu gaidīšanas laiku un shell `trap` OFF drošības tīklu. Ir atgūts reāls 60 s ON → OFF izpildījums ar Telegram paziņojumu.

Tajā pašā vēstures periodā Hermes `ha_call_service` dažkārt varēja atgriezt `success`, lai gan komanda līdz ESP32 nebija nonākusi. Tāpēc toreiz priekšroka tika dota tiešajam skriptam ar REST/retry.

Tas ir **historical behavior**, nevis current RPi5 source guarantee. Pašreizējo skriptu nedrīkst rekonstruēt tikai no čata; tas jānolasa no RPi5.

## Vēsturiskais 30 s auto-OFF incidents

Vecākā firmware/automatizācijas stadijā tika fiksēti testi, kuros pēc 40–45 s pumpis joprojām bija ON. Šis incidents ir superseded ar current firmware lokālo pump session timer un 180 s hard-limit. Vecajos materiālos sastopamais 600 s limits arī ir superseded.

Plašāka recovered vēsture un conflict tabula: [`HISTORICAL_KNOWLEDGE_BASE.md`](HISTORICAL_KNOWLEDGE_BASE.md).