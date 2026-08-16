# Projekta vēstures kopsavilkums

Šis nav pilns Claude čatu eksports. Tas ir sanitizēts projekta lēmumu un incidentu kopsavilkums. Pilnā, ilgtermiņa atgūtā zināšanu bāze ir [`HISTORICAL_KNOWLEDGE_BASE.md`](HISTORICAL_KNOWLEDGE_BASE.md).

## 2026-05 — projekta sākums

Projekts attīstījās no vienkāršas balkona automātiskās laistīšanas idejas līdz ESP32 sistēmai ar daudziem augsnes sensoriem, multipleksoru, sūkni, releju un Home Assistant/MQTT integrāciju.

Claude eksporta indeksā 2026-05-01 pamata sarunai bija 603 ziņas. Galvenais aparatūras virziens, kas saglabājās līdz pašreizējam firmware:

- 15 mitruma sensori;
- CD74HC4067 16→1 analogais MUX;
- ESP32;
- R385 sūknis;
- releja vadība;
- Home Assistant/MQTT.

## 2026-06 — sistēma kļūst par pilnu IoT kontrolieri

Lielā 2026-06-27 saruna “Balkona laistīšanas projekts turpinājums” Claude eksporta analīzē bija apmēram 496 000 rakstzīmju gara.

Šajā projekta posmā nostiprinājās:

- MUX pinout;
- sensoru kalibrācija;
- MQTT topic līgums;
- Home Assistant discovery;
- sūkņa komandas/statistika;
- watchdog;
- Wi-Fi/MQTT reconnect;
- OTA;
- logi un Telegram integrācija.

## Telegram arhitektūras maiņa

Agrīnā firmware Telegram TLS klients atradās uz ESP32 kopā ar sensoru lasīšanu un sūkņa vadību. Vēsturē tas izrādījās pārāk trausls — bija heap/TLS/savienojumu nestabilitāte.

Arhitektūras lēmums:

**ESP32 = hardware + safety + MQTT. RPi5 = Telegram + augstāka līmeņa integrācija.**

Vēsturiskā RPi5 auditā bija redzami `balkons-bot.service` un `balkons-log.service`. 2026-07-17 incidenta logā `balkons-bot.service` atkārtoti krita ar `exit-code`, vēlreiz apstiprinot, ka Telegram slānis nedrīkst būt vienīgais sūkņa OFF mehānisms.

## Sūkņa taimera incidents

Vecākā stadijā reālos testos 30 s auto-OFF neizpildījās: pēc 40–45 s sūknis joprojām varēja būt ON, līdz ārējam OFF vai restartam.

Tas noveda pie drošāka modeļa ar:

- lokālu ESP32 pump session timer;
- absolūtu hard-limit;
- ārējo RPi5/HA OFF tikai kā papildu slāni.

Vienā vēsturiskā variantā `MAX_PUMP_SECONDS` bija 600 s. Tas ir superseded. Current firmware limit = 180 s.

## Vēsturiska RPi5 `laistisana.sh`

Vēsturē eksistēja `laistisana.sh`, kas caur Home Assistant REST API:

- ieslēdza sūkni;
- izmantoja līdz 3 retry ar apmēram 3 s pauzi;
- vienā variantā gaidīja 60 s;
- izslēdza sūkni;
- izmantoja `trap` kā kļūdas OFF drošības tīklu;
- sūtīja Telegram paziņojumus.

Ir atgūts 60 s izpildījuma rezultāts ar veiksmīgu ON → wait → OFF → Telegram ķēdi.

Tajā periodā tika arī atzīmēts, ka Hermes `ha_call_service` reizēm ziņoja `success`, lai gan komanda līdz ESP32 nebija nonākusi, tāpēc `laistisana.sh` ar REST/retry tika uzskatīts par uzticamāku ceļu.

Aktuālais RPi5 skripts vēl jānolasa no dzīva RPi5 pirms importēšanas repo.

## Back-EMF incidents

Vēsturiski ESP32 restartējās tieši tad, kad sūknis tika **izslēgts**. Debugging secinājums bija R385/12 V DC motora induktīvais kickback un barošanas troksnis, nevis tikai firmware watchdog kļūda.

Dokumentētais labojums:

- `1N5408` flyback diode pāri sūkņa termināļiem;
- `100 nF` keramiskais kondensators;
- `470 µF` elektrolītiskais/bulk kondensators.

Atgūtā post-fix testā sūknis darbojās apmēram 15 s, pēc tam tika izslēgts, un ESP32 nepazuda. Tas ir historical short-test PASS.

## NO/NC dokumentācijas kļūda

Vecā LinkedIn draftā bija kļūdains apgalvojums, ka fail-safe OFF izmanto `NC`. Vēlāk tas tika izlabots: projekta pareizais dizains ir **NO (Normally Open)**, lai bez releja aktivizācijas sūknis paliktu OFF.

Tā kā koriģētajā materiālā bija arī norāde “VERIFY REAL HARDWARE”, precīzais pašreizējais `COM/NO/NC` spaiļu vadojums vēl jānofotografē kā fizisks pierādījums.

## MUX GPIO12 lēmums

MUX `S1` vēsturiski un pašlaik ir GPIO12. Tika identificēts, ka GPIO12/MTDI ir ESP32 strapping pin, tāpēc gala montāžai paredzēta pārcelšana uz GPIO25.

Robeža: kodā paliek `12`, līdz fiziskais vads tiešām ir pārlikts.

## Sensoru vēsturiskie incidenti

Atsevišķos vēsturiskos testos:

- `Puķe 8` bija offline/unavailable;
- citā testā `puke_5` jau pirms pumpja testa bija unavailable;
- `puke_2` vienā testā atjaunojās no unavailable uz `sauss`.

Tie nav pašreizējā sensora stāvokļa pierādījumi.

## 2026-08-16 — GitHub bootstrap un production OTA

Vecais Lenovo PlatformIO source tika salīdzināts ar importēto baseline; `src/main.cpp` SHA-256 sakrita 1:1 ar dokumentēto sākuma avotu.

Publiskais repo tika uzbūvēts ar:

- sanitizētu `secrets.example.h`;
- `.gitignore` un tracked-file safety scanneri;
- build-only GitHub Actions;
- pinotu/drošības/MQTT/arhitektūras dokumentāciju;
- current pioarduino/Arduino-ESP32 toolchain;
- OTA atsevišķā env, bez auto-deploy.

Bootstrap auditā tika salabota timezone inicializācija uz `configTzTime(...)`.

PR #1 tika squash-merged. Exact production `main`:

`599abfac74b0b30fdc03e3076fda7630353812c0`

Exact-main CI bija SUCCESS. Pēc explicit owner OTA autorizācijas firmware tika uzlikts pa Wi-Fi. Post-OTA `/statuss` apstiprināja svaigu uptime, pareizu Europe/Berlin laiku, Wi-Fi/MQTT ONLINE un sūkni OFF.

## Pašreizējie neatrisinātie punkti

- nofotografēt exact releja `COM/NO/NC` fizisko vadojumu;
- pēc jebkādas montāžas pārlikšanas nofotografēt `1N5408 + 100nF + 470µF` faktisko izvietojumu/polaritāti;
- svaigi pārbaudīt sensoru veselību;
- gala montāžā pabeigt GPIO12 → GPIO25 MUX S1 pārcelšanu;
- importēt tikai sanitizētas, **aktuālas** RPi5 Telegram/HA bridge, `balkons-log` un `laistisana.sh` versijas.

## Kāpēc raw Claude eksports nav repo

Eksportā bija paroles, tokeni, sensitīvi dati, veci bināri/artefakti, novecojuši stāvokļi un draftu kļūdas. Raw arhīvs paliek ārpus publiskā Git.

No tā atgūtie tehniskie fakti un to pretrunas tagad ir centralizēti [`HISTORICAL_KNOWLEDGE_BASE.md`](HISTORICAL_KNOWLEDGE_BASE.md), lai turpmāk raw ZIP ikdienas darbam vairs nebūtu vajadzīgs.