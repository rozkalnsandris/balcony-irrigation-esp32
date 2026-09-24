# Balkona laistīšana — ESP32

Balkona laistīšanas sistēmas source-of-truth repozitorijs. Sistēma izmanto ESP32, 15 augsnes mitruma sensorus caur CD74HC4067 multipleksoru, releja vadītu R385 sūkni, MQTT, Home Assistant auto-discovery, OTA un RPi5 integrācijas.

> **Drošība:** repozitorijā nav un nedrīkst būt Wi-Fi, MQTT, Home Assistant, Telegram vai OTA paroles/tokeni. `include/secrets.h` ir lokāls fails un ir iekļauts `.gitignore`.

## Pašreizējais production baseline

- exact production `main`: `e34a3e02a290d5022db6d41452f4d81a6575aac6`;
- exact-main `firmware-ci` #19 / run `31972773249`: SUCCESS;
- 2026-08-16 authenticated ArduinoOTA deploy #6: SUCCESS / `OTA_RC=0`;
- post-OTA `/statuss`: exact `Firmware: e34a3e02a290d5022db6d41452f4d81a6575aac6`, Wi-Fi ONLINE, MQTT ONLINE, Europe/Berlin laiks korekts, sūknis OFF, svaigs uptime;
- OTA hostname: `balkons-esp32.local`.

Iepriekšējais `599abfac74b0b30fdc03e3076fda7630353812c0` bija pirmais veiksmīgi verificētais bootstrap OTA baseline un paliek projekta vēsturē, bet vairs nav pašreizējais production SHA.

## Pašreizējais firmware

- ESP32 Dev Module / Arduino framework / PlatformIO.
- 15 mitruma kanāli caur CD74HC4067 MUX.
- Sensori tiek klasificēti kā `sauss`, `videjs`, `mitrs`.
- MQTT brokeris un syslog atrodas RPi5 lokālajā tīklā.
- Home Assistant sensoru un sūkņa switch auto-discovery.
- Telegram ziņas tiek izvadītas caur MQTT uz RPi5 pusi.
- ArduinoOTA ar paroles aizsardzību.
- PlatformIO build iebūvē pilno Git revīziju; `/statuss` un startup logs rāda exact runtime firmware identitāti.
- Watchdog un Wi-Fi/MQTT reconnect loģika.
- Sūknim ir lokāls, no ārējiem servisiem neatkarīgs **180 sekunžu hard limit**.
- Boot un OTA sākumā relejs tiek piespiests OFF.

## Ātra struktūra

```text
.
├── src/main.cpp                         # ESP32 firmware
├── include/secrets.example.h            # tikai piemērs; īstais secrets.h netiek commitots
├── platformio.ini                       # build-only CI + atsevišķa OTA vide
├── scripts/git_rev_macro.py             # build-time exact Git revision
├── scripts/ota_upload_existing.py       # exact-artifact ArduinoOTA uploader
├── .github/workflows/firmware-ci.yml    # build-only GitHub Actions CI
├── docs/ARCHITECTURE.md
├── docs/HARDWARE.md
├── docs/MQTT_HOME_ASSISTANT.md
├── docs/SAFETY.md
├── docs/FIRMWARE_IDENTITY.md
├── docs/PHYSICAL_WIRING_VERIFICATION.md # pump-OFF fiziskās montāžas evidence checklist
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

4. OTA build ir atsevišķs un pats par sevi neko neaugšupielādē:

```bash
pio run -e esp32_ota
```

5. Pirms deploy nofiksē exact source SHA un `.pio/build/esp32_ota/firmware.bin` SHA-256. Kontrolētam deploy izmanto jau uzbūvēto artefaktu; `scripts/ota_upload_existing.py` pirms tīkla darbības pārbauda abus identifikatorus un tracked worktree tīrību.

```bash
python scripts/ota_upload_existing.py \
  --expected-source-sha <AUTHORIZED_GIT_SHA> \
  --expected-firmware-sha256 <AUTHORIZED_FIRMWARE_SHA256>
```

Helperis nolasa tikai lokālo `OTA_PASSWORD` no ignored `include/secrets.h`, neizdrukā tā vērtību un izsauc pinned Arduino `espota.py` tajā pašā Python procesā, lai parole nebūtu jāliek shell komandā. Tas **neveido firmware no jauna**, neveic firewall izmaiņas un vienmēr izmanto reverse-TCP host portu `3233`.

PlatformIO tiešam interaktīvam upload arī ir piesprausts tas pats ports. Ja apzināti izmanto `pio run -e esp32_ota -t upload`, auth dod ar atsevišķu mainīgo, nevis ar `PLATFORMIO_UPLOAD_FLAGS`:

```bash
export BALCONY_OTA_AUTH='YOUR_OTA_PASSWORD'
pio run -e esp32_ota -t upload
unset BALCONY_OTA_AUTH
```

`BALCONY_OTA_AUTH` pieeja saglabā repo definēto `--host_port=3233`; `PLATFORMIO_UPLOAD_FLAGS` šim projektam neizmanto auth nodošanai, jo tas var aizstāt projekta upload flags.

### OTA tīkla/firewall priekšnosacījums

Arduino `espota.py` vispirms veic UDP invitation/auth uz ESP32 portu `3232`, pēc tam ESP32 atver **TCP savienojumu atpakaļ uz uploader hostu**. Tāpēc uploader hostam jāpieņem `3233/tcp` no konkrētās ESP32 adreses. Firewall noteikumam jābūt šauram (ESP32 source IP → uploader host `3233/tcp`), nevis globāli atvērtam portam.

Host firewall konfigurācija ir production/LIVE darbība un nav firmware build/merge blakusefekts. To konfigurē atsevišķi ar explicit owner autorizāciju. Helperis firewall nemaina.

OTA mērķis ir `balkons-esp32.local`, kas atbilst firmware hostname; nav jāuztur cieti iešūta DHCP IP adrese OTA konfigurācijā.

Pēc jebkura explicit owner autorizēta OTA `/statuss` laukam `Firmware:` precīzi jāsakrīt ar autorizēto `git rev-parse HEAD`; pilnais verifikācijas kontrakts ir [`docs/FIRMWARE_IDENTITY.md`](docs/FIRMWARE_IDENTITY.md).

## Svarīga robeža

Šis repozitorijs dokumentē un glabā kodu. **CI vai parasts Git merge nedrīkst automātiski ieslēgt sūkni vai veikt OTA uz dzīvo ierīci.** Fiziskas darbības jāpalaiž atsevišķi un apzināti.

## Aktuālie TODO

- [ ] Vienreiz nofotografēt/nostiprināt faktiskās releja `COM/NO/NC` spailes pēc [`docs/PHYSICAL_WIRING_VERIFICATION.md`](docs/PHYSICAL_WIRING_VERIFICATION.md). Vēstures koriģētais fail-safe dizains ir **NO**, bet precīzo pašreizējo termināļu stāvokli glabājam kā fiziski pārbaudāmu faktu.
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
