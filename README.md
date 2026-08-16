# Balkona laistīšana — ESP32

Balkona laistīšanas sistēmas source-of-truth repozitorijs. Sistēma izmanto ESP32, 15 augsnes mitruma sensorus caur CD74HC4067 multipleksoru, releja vadītu R385 sūkni, MQTT, Home Assistant auto-discovery, OTA un RPi5 integrācijas.

> **Drošība:** repozitorijā nav un nedrīkst būt Wi-Fi, MQTT, Home Assistant, Telegram vai OTA paroles/tokeni. `include/secrets.h` ir lokāls fails un ir iekļauts `.gitignore`.

## Pašreizējais production baseline

- exact production `main`: `599abfac74b0b30fdc03e3076fda7630353812c0`;
- exact-main `firmware-ci`: SUCCESS;
- 2026-08-16 authenticated ArduinoOTA deploy: SUCCESS;
- post-OTA ESP32: Wi-Fi ONLINE, MQTT ONLINE, CET/CEST laiks korekts, sūknis OFF;
- OTA hostname: `balkons-esp32.local`.

## Pašreizējais firmware

- ESP32 Dev Module / Arduino framework / PlatformIO.
- 15 mitruma kanāli caur CD74HC4067 MUX.
- Sensori tiek klasificēti kā `sauss`, `videjs`, `mitrs`.
- MQTT brokeris un syslog atrodas RPi5 lokālajā tīklā.
- Home Assistant sensoru un sūkņa switch auto-discovery.
- Telegram ziņas tiek izvadītas caur MQTT uz RPi5 pusi.
- ArduinoOTA ar paroles aizsardzību.
- Watchdog un Wi-Fi/MQTT reconnect loģika.
- Sūknim ir lokāls, no ārējiem servisiem neatkarīgs **180 sekunžu hard limit**.
- Boot un OTA sākumā relejs tiek piespiests OFF.

## Ātra struktūra

```text
.
├── src/main.cpp                         # ESP32 firmware
├── include/secrets.example.h            # tikai piemērs; īstais secrets.h netiek commitots
├── platformio.ini                       # build-only CI + atsevišķa OTA vide
├── .github/workflows/firmware-ci.yml    # build-only GitHub Actions CI
├── docs/ARCHITECTURE.md
├── docs/HARDWARE.md
├── docs/MQTT_HOME_ASSISTANT.md
├── docs/SAFETY.md
├── docs/PROJECT_HISTORY.md
├── docs/HISTORICAL_KNOWLEDGE_BASE.md    # sanitizētā Claude-era projekta atmiņa
├── docs/SOURCE_BASELINE.md
└── docs/AUDIT_2026-08-16.md
```

## Lokāla sagatavošana

1. Nokopē `include/secrets.example.h` uz `include/secrets.h`.
2. Ievadi lokāli Wi-Fi, MQTT un OTA datus.
3. Parasts build izmanto drošo `esp32_ci` vidi un neprasa OTA paroli:

```bash
pio run
```

4. Tikai pirms apzināta OTA upload nodod uploaderim paroli caur PlatformIO oficiālo vides mainīgo:

```bash
export PLATFORMIO_UPLOAD_FLAGS='--auth=YOUR_OTA_PASSWORD'
```

5. OTA upload (tikai tad, kad apzināti vēlies mainīt dzīvo ESP32):

```bash
pio run -e esp32_ota -t upload
```

Pēc upload noņem slepeno mainīgo no shell sesijas:

```bash
unset PLATFORMIO_UPLOAD_FLAGS
```

OTA mērķis ir `balkons-esp32.local`, kas atbilst firmware hostname; nav jāuztur cieti iešūta DHCP IP adrese OTA konfigurācijā.

## Svarīga robeža

Šis repozitorijs dokumentē un glabā kodu. **CI vai parasts Git merge nedrīkst automātiski ieslēgt sūkni vai veikt OTA uz dzīvo ierīci.** Fiziskas darbības jāpalaiž atsevišķi un apzināti.

## Aktuālie TODO

- [ ] Vienreiz nofotografēt/nostiprināt faktiskās releja `COM/NO/NC` spailes. Vēstures koriģētais fail-safe dizains ir **NO**, bet precīzo pašreizējo termināļu stāvokli glabājam kā fiziski pārbaudāmu faktu.
- [x] Claude-era vēsturē atgūts pierādījums, ka tika uzlikts `1N5408 + 100nF + 470µF` back-EMF/decoupling komplekts.
- [x] Claude-era vēsturē atgūts vismaz viens ~15 s post-fix sūkņa tests, kur ESP32 nepazuda un sūknis korekti atgriezās OFF.
- [ ] Pēc jebkādas jaunākas aparatūras pārlikšanas nofotografēt pašreizējo diodes polaritāti/kondensatoru izvietojumu; atkārtot īsu sūkņa testu tikai ar owner autorizāciju.
- [ ] Pārbaudīt pašreizējo 15 sensoru veselību; vecie `Puķe 8`/`puke_5` unavailable gadījumi ir vēsturiski, nevis current status.
- [ ] Pēc gala montāžas pārvietot MUX `S1` no GPIO12 uz GPIO25 un **tikai pēc fiziskā vada pārlikšanas** mainīt firmware.
- [ ] No dzīva RPi5 paņemt aktuālo Telegram/HA bridge, `balkons-log` un `laistisana.sh`, sanitizēt un tikai tad pievienot repo.

## Vēsturiskie materiāli

Claude raw sarunu eksports netiek glabāts šajā repo: tas saturēja akreditācijas datus, draftus un novecojušus sistēmas stāvokļus.

Visa atgūtā, sanitizētā projekta zināšanu bāze tagad ir: **[`docs/HISTORICAL_KNOWLEDGE_BASE.md`](docs/HISTORICAL_KNOWLEDGE_BASE.md)**.

Tur ir arī īpaša sadaļa ar superseded/pretrunīgiem faktiem (`NC` → `NO`, vecais 600 s limits → current 180 s, vecais Telegram-on-ESP32 modelis → RPi5 bridge u.c.), lai nākamajos čatos nebūtu jāmeklē ZIP arhīvā.