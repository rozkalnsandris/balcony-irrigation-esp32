# Vēstures evidence provenance

Šis fails paskaidro, **no kāda tipa avota** nāk `HISTORICAL_KNOWLEDGE_BASE.md` fakti. Mērķis ir nepieļaut, ka vēlāk “vecā čata atmiņa” tiek kļūdaini uztverta kā svaigs production pierādījums.

## A. Raw Claude eksporta analīze

Saglabātā Claude eksporta analīze rāda:

- periods: 2026-04-18 .. 2026-07-16;
- 269 sarunas;
- 9 923 ziņas;
- 180 pielikumi ar izvilktu saturu;
- 17 sarunas kategorijā “Balkona laistīšana, ESP32 un augi”;
- lielākā relevantā saruna: 2026-06-27 “Balkona laistīšanas projekts turpinājums”, 319 ziņas, apmēram 496 000 rakstzīmes;
- 2026-05-01 pamata laistīšanas saruna indeksā: 603 ziņas.

Šis avots ir noderīgs arhitektūras lēmumiem un agrīnai projekta vēsturei, bet pats analīzes fails brīdina, ka kopsavilkumos ir arī novecojis stāvoklis un Claude pieņēmumi.

## B. Claude-era drafti / sanitizētie plāni

Vēsturiskie LinkedIn plāni tika izmantoti tikai kā papildus evidence par projekta stāstu.

Svarīga pretruna:

- vecāks drafts kļūdaini rakstīja `NC` kā fail-safe;
- vēlākā koriģētā versija skaidri norāda **NO** un pati atzīmē “VERIFY REAL HARDWARE”.

Tāpēc `NO` ir projekta koriģētais dizaina lēmums, bet exact fiziskās spailes joprojām ir atsevišķi pārbaudāms fakts.

Vecie publicēšanas drafti nedrīkst tikt izmantoti kā vienīgais pierādījums skaitļiem, montāžai vai “zero crashes” apgalvojumiem.

## C. Atgūtie testa/transkripta artefakti pēc raw Claude eksporta perioda

Daļa tehniski ļoti vērtīgu ierakstu tika atrasta vēlākos lietotāja saglabātos/pasted vēstures artefaktos. Tie satur agrāku laistīšanas darbu transkriptus, bet **nav tas pats, kas raw Claude ZIP analīzes fails**.

No šīs klases nāk, piemēram:

- `1N5408 + 100nF + 470µF` post-fix ~15 s testa rezultāts;
- vēsturiskais `laistisana.sh` 60 s cikls;
- periods ar 23:00 daily watering schedule;
- `DURATION` maiņa no 110 s uz 60 s;
- `ha_call_service` false-success novērojums;
- daļa sensoru unavailable incidentu.

Repo tos apzīmē kā **recovered historical transcript/evidence**, nevis kā pašreizējā production stāvokļa automātisku apliecinājumu.

## D. RPi5 audit/log evidence

Atsevišķi saglabātie RPi5 audit/log artefakti parāda:

- `balkons-bot.service` un `balkons-log.service` kā reāli eksistējušus systemd servisus vienā auditētā stāvoklī;
- 2026-07-17 logu periodu, kur `balkons-bot.service` atkārtoti krita ar `exit-code`.

Tas ir labs incidents/architecture evidence, bet ne current service health. Aktuālais stāvoklis jālasa no dzīva RPi5.

## E. Current repository source

Pašreizējais `src/main.cpp`, `platformio.ini` un GitHub `main` ir autoritatīvi attiecībā uz:

- pinout;
- 30 s default pump duration;
- 180 s hard limit;
- MQTT topic;
- HA discovery;
- watchdog/reconnect;
- OTA behavior;
- current time configuration.

Ja old history konfliktē ar current source, uzvar current source.

## F. Current runtime verification

2026-08-16 production OTA verifikācija ir jaunākais runtime evidence:

- exact `main` `599abfac74b0b30fdc03e3076fda7630353812c0`;
- exact-main CI SUCCESS;
- OTA mDNS/hostname reachable;
- MQTT connected;
- correct CET/CEST local time;
- fresh uptime after OTA;
- pump OFF.

Šis pierādījums ir augstākas prioritātes nekā jebkura vecā saruna.

## G. Ideju attēli un mockup

Elektronikas organizācijas/IP65/DIN kolāža ir **ideju/proposal materiāls**. Tā nedrīkst tikt izmantota kā pierādījums, ka konkrēta IP65 kaste, DIN sliede, atsevišķs 12 V PSU vai attēlā redzamā vadu topoloģija faktiski ir uzstādīta.

## H. Ko apzināti neglabājam GitHub

Pat ja tas atrodams vecajā vēsturē, publiskajā repo netiek pārnests:

- Wi-Fi SSID/paroles;
- MQTT paroles;
- OTA paroles;
- Home Assistant bearer tokeni;
- Telegram bot tokeni/chat sensitīvie ID;
- GitHub/Cloudflare/API tokeni;
- raw Claude ZIP/JSON/Markdown sarunas;
- `.env`, `include/secrets.h`;
- `.pio` vai firmware bin/ELF ar kompilētiem secrets;
- privāta personīgā informācija, kas nav vajadzīga laistīšanas projektam.

## Praktiskais noteikums nākamajam čatam

Ja jautājums ir “kā tas ir uzbūvēts / kāpēc tā izdarījām”, vispirms lasi `HISTORICAL_KNOWLEDGE_BASE.md`.

Ja jautājums ir “kā tas darbojas **tagad**”, vispirms lasi current `main` un, ja nepieciešams, veic svaigu read-only runtime pārbaudi.

Ja jautājums ir par fizisku vadu/polaritāti pēc iespējamas pārlikšanas, vecs čats nav pietiekams — vajag svaigu fizisku evidence.