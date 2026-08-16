# Balkona laistīšana — ESP32

Balkona laistīšanas sistēmas source-of-truth repozitorijs. Sistēma izmanto ESP32, 15 augsnes mitruma sensorus caur CD74HC4067 multipleksoru, releja vadītu R385 sūkni, MQTT, Home Assistant auto-discovery, OTA un RPi5 integrācijas.

> **Drošība:** repozitorijā nav un nedrīkst būt Wi-Fi, MQTT, Home Assistant, Telegram vai OTA paroles/tokeni. `include/secrets.h` ir lokāls fails un ir iekļauts `.gitignore`.

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
├── src/main.cpp                      # ESP32 firmware
├── include/secrets.example.h         # tikai piemērs; īstais secrets.h netiek commitots
├── platformio.ini                    # build-only CI + atsevišķa OTA vide
├── .github/workflows/firmware-ci.yml # build-only GitHub Actions CI
├── docs/ARCHITECTURE.md
├── docs/HARDWARE.md
├── docs/MQTT_HOME_ASSISTANT.md
├── docs/SAFETY.md
├── docs/PROJECT_HISTORY.md
└── docs/AUDIT_2026-08-16.md
```

## Lokāla sagatavošana

1. Nokopē `include/secrets.example.h` uz `include/secrets.h`.
2. Ievadi lokāli Wi-Fi, MQTT un OTA datus.
3. Parasts build izmanto drošo `esp32_ci` vidi un neprasa OTA paroli:

```bash
pio run
```

4. Tikai pirms apzināta OTA upload iestati paroli vides mainīgajā:

```bash
export BALKONS_OTA_PASSWORD='...'
```

5. OTA upload (tikai tad, kad apzināti vēlies mainīt dzīvo ESP32):

```bash
pio run -e esp32_ota -t upload
```

OTA mērķis ir `balkons-esp32.local`, kas atbilst firmware hostname; nav jāuztur cieti iešūta DHCP IP adrese.

## Svarīga robeža

Šis repozitorijs dokumentē un glabā kodu. **CI vai parasts Git merge nedrīkst automātiski ieslēgt sūkni vai veikt OTA uz dzīvo ierīci.** Fiziskas darbības jāpalaiž atsevišķi un apzināti.

## Aktuālie TODO

- [ ] Fiziski pārbaudīt, vai releja sūkņa ķēde tiešām izmanto NO (normally-open) fail-safe kontaktu.
- [ ] Uzstādīt/pārbaudīt flyback diodi un barošanas/decoupling kondensatorus pret motora back-EMF/trokšņiem.
- [ ] Pēc elektriskās aizsardzības veikt kontrolētu stabilitātes testu.
- [ ] Pārbaudīt un pēc gala montāžas pārvietot MUX `S1` no GPIO12 uz GPIO25, ja fiziskais vads ir pārlikts.
- [ ] No RPi5 paņemt **aktuālo** Telegram/HA bridge un `laistisana.sh` versiju, sanitizēt un tikai tad pievienot repo.

## Vēsturiskie materiāli

Claude sarunu eksports netiek glabāts šajā repo: tas saturēja akreditācijas datus un arī novecojušus sistēmas stāvokļus. No vēstures repo ir pārcelti tikai droši, projekta darbībai noderīgi lēmumi un dokumentācija.
