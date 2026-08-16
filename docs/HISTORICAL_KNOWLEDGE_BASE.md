# Sanitizētā vēsturiskā zināšanu bāze

Šis fails ir paredzēts kā ilgtermiņa projekta atmiņa, lai turpmāk nebūtu jāmeklē pa veco Claude sarunu ZIP eksportu.

Tas **nav** raw sarunu eksports. No vēstures šeit saglabājam tikai tehniski noderīgus faktus, lēmumus, incidentus, testu rezultātus un zināmas pretrunas. Paroles, tokeni, privātās atslēgas, Wi-Fi akreditācijas dati un citi sensitīvi dati apzināti nav iekļauti.

## 1. Avotu prioritāte

Ja dažādi avoti konfliktē, izmanto šo secību:

1. **Pašreizējais `main` firmware un repo dokumentācija.**
2. **Svaigi verificēts production runtime** uz faktiskā ESP32/RPi5.
3. **Reāli testa/logu ieraksti** no vēsturiskajām sarunām.
4. **Vēsturisku sarunu tehniskie lēmumi un kopsavilkumi.**
5. **Drafti, LinkedIn tekstu melnraksti un Claude pieņēmumi** — tikai kā pavedieni, nevis source-of-truth.

Claude eksporta analīze aptvēra 269 sarunas, 9 923 ziņas un periodu 2026-04-18..2026-07-16; kategorijā “Balkona laistīšana, ESP32 un augi” bija 17 sarunas. Lielākā bija 2026-06-27 “Balkona laistīšanas projekts turpinājums” ar 319 ziņām un apmēram 496 000 rakstzīmēm. Tāpēc šis fails glabā gala secinājumus, nevis mēģina pārnest visu dialogu.

## 2. Pašreizējā verificētā bāze — 2026-08-16

Production firmware tika uzlikts ar ArduinoOTA no exact `main`:

`599abfac74b0b30fdc03e3076fda7630353812c0`

Exact-main GitHub Actions `firmware-ci` bija SUCCESS. Pēc OTA ESP32 atgriezās ar:

- hostname: `balkons-esp32.local`;
- lokālo IP: `192.168.0.53`;
- ArduinoOTA servisu uz porta 3232 ar autentifikāciju;
- MQTT savienojumu ONLINE;
- pareizu Europe/Berlin CET/CEST laiku;
- svaigu uptime pēc restarta;
- sūkni OFF.

Šis runtime ir jaunāks un autoritatīvāks par Claude-era firmware detaļām.

## 3. Sistēmas mērķis un arhitektūra

Sistēma laista balkona augus un mēra augsnes mitrumu. Galvenās daļas:

- ESP32 Dev Module;
- 15 augsnes mitruma sensori;
- CD74HC4067 16-kanālu analogais multipleksors;
- R385 DC ūdens sūknis;
- active-low releja modulis;
- MQTT brokeris uz RPi5;
- Home Assistant MQTT discovery un vadība;
- RPi5 Telegram bridge;
- syslog uz RPi5;
- NTP/CET/CEST laiks;
- ArduinoOTA firmware atjauninājumi.

Svarīgs arhitektūras lēmums bija **nodalīt safety-critical ESP32 loku no Telegram/TLS**. ESP32 lasa sensorus, vada sūkni, uztur lokālo drošības taimeri un runā MQTT. Telegram un augstāka līmeņa loģika tika pārvietota uz RPi5, lai ārējas TLS/chat problēmas nevarētu destabilizēt pašu sūkņa vadību.

## 4. Kāpēc tika izmantots CD74HC4067

15 analogiem mitruma sensoriem nepietiek ar praktiski pieejamajām ESP32 ADC ieejām. Tāpēc viens CD74HC4067 multipleksors pārslēdz līdz 16 analogiem kanāliem uz vienu ESP32 ADC ieeju.

Pašreizējais pinout:

| Funkcija | GPIO | Statuss |
|---|---:|---|
| relejs | 26 | current |
| MUX S0 | 13 | current |
| MUX S1 | 12 | current, fiziski vēl nav pārvietots |
| MUX S2 | 14 | current |
| MUX S3 | 27 | current |
| MUX SIG | 34 | current ADC input |
| sensoru skaits | 15 | current |

### GPIO12 → GPIO25 lēmums

GPIO12/MTDI ir ESP32 strapping pin. Vēsturē tika nolemts gala montāžā MUX `S1` fiziski pārvietot no GPIO12 uz GPIO25, lai samazinātu boot/flashing risku.

**Svarīgi:** firmware nedrīkst mainīt `MUX_S1` uz 25 pirms fiziskais vads tiešām ir pārlikts. Pašreizējais kods pareizi joprojām izmanto GPIO12.

## 5. Mitruma sensoru kalibrācija

Saglabātie atskaites punkti:

- galīgi sauss: ap `2217` RAW ADC;
- vajag laistīt: ap `1850`;
- mitrs: ap `1175`.

Pašreizējie firmware sliekšņi:

- RAW `> 2000` → `sauss`;
- RAW `< 1400` → `mitrs`;
- pa vidu → `videjs`.

Firmware `/raw` pārskatā katram MUX kanālam tiek ņemts vairāku ADC lasījumu vidējais; `/mitrums` dod kategoriju pārskatu.

### Vēsturiskas sensoru anomālijas

Vecajos testos atsevišķos brīžos tika novērots:

- `Puķe 8` bez derīgiem datiem/offline;
- citā vēlākā testā `puke_5` bija unavailable jau **pirms** sūkņa testa;
- tajā pašā testā `puke_2` atjaunojās no unavailable uz `sauss`.

Tie ir **vēsturiski incidenti**, nevis apgalvojums par pašreizējo sensoru veselību. Aktuālais stāvoklis jāpārbauda ar `/mitrums` un `/raw`.

## 6. Sūknis un relejs

### Firmware puse

- releja GPIO: 26;
- relejs: active-low;
- `LOW` = ON;
- `HIGH` = OFF;
- firmware `setup()` sākumā piespiež OFF pirms pārējās inicializācijas;
- OTA sākumā relejs tiek piespiests OFF;
- ja ESP32 restartējas laistīšanas laikā un RTC flags saglabājas, pēc restarta var tikt nosūtīts brīdinājums;
- lokālais sūkņa taimeris nav atkarīgs tikai no HA, Telegram vai tīkla.

Pašreizējais noklusējuma `laist` ilgums ir 30 s, bet absolūtais viena seansa hard-limit ir 180 s.

### Releja NO/NC pretruna vecajos materiālos

Vecā LinkedIn draftā bija tehniski kļūdaina frāze, ka fail-safe OFF esot jāizmanto `NC`. Tas ir **superseded/WRONG**.

Vēlākajā koriģētajā vēstures materiālā skaidri ierakstīts:

- sūkņa fail-safe OFF topoloģijai jāizmanto releja **NO (Normally Open)** kontakts;
- ja ESP32/releja vadības barošana pazūd vai programmatūra nepalaižas, sūknim jāpaliek OFF.

Tomēr koriģētajā vēsturiskajā materiālā bija arī atzīme “VERIFY REAL HARDWARE”. Tāpēc pašreizējā dokumentācijas politika ir:

- **NO ir pareizais un vēsturiski koriģētais dizains**;
- precīzo faktisko `COM/NO/NC` spaiļu pieslēgumu pēc jebkādas pārvadīšanas/pārlikšanas joprojām ir vērts vienreiz nofotografēt un nostiprināt repo.

## 7. Back-EMF incidents un elektriskā aizsardzība

### Vēsturiskais simptoms

Agrīnā sistēmā ESP32 mēdza restartēties tieši **sūkņa izslēgšanas brīdī** — nevis startā un nevis kamēr sūknis darbojās. Vēsturiskajā debugging secinājumā vaininieks bija 12 V DC motora induktīvais kickback/back-EMF un barošanas traucējumi.

Tas sākumā izskatījās pēc firmware/watchdog problēmas, bet sakritība ar motora atslēgšanas brīdi noveda pie aparatūras cēloņa.

### Vēsturē dokumentētais labojums

Tika dokumentēta šāda aizsardzība:

- `1N5408` flyback diode pāri sūkņa termināļiem;
- `100 nF` keramiskais kondensators;
- `470 µF` elektrolītiskais/bulk kondensators barošanas trokšņu slāpēšanai.

Šīs vērtības ir **šīs konkrētās vēsturiskās iekārtas** dati, nevis universāls ieteikums jebkuram motoram.

### Atgūtais post-fix testa pierādījums

Vēsturiskajā transkriptā pēc aizsardzības uzstādīšanas ir reāls tests:

- sūknis: OFF → ON apmēram 15 s → OFF;
- ESP32 testa laikā nepazuda no tīkla;
- pumpja darbības laikā jauni sensoru unavailable stāvokļi neparādījās;
- `puke_5` bija unavailable jau pirms testa;
- secinājums transkriptā: `1N5408 + 100nF + 470µF` stabilizēja ESP32 šajā īsajā pumpja ciklā.

Tātad vecais TODO “flyback diode/kondensatori nav uzlikti” ir novecojis. Pareizais pašreizējais formulējums ir:

**Vēsturē ir pierādījums, ka aizsardzība tika uzlikta un vismaz viens ~15 s post-fix tests bija PASS.**

Kas vēl nav tikpat stingri nostiprināts:

- pašreizējās fiziskās polaritātes/faktiskā montāžas stāvokļa foto;
- liels atkārtotu ciklu skaits pēc pēdējās pārvietošanas/pārkārtošanas;
- osciloskopa vai sprieguma sliedes mērījums.

## 8. Vēsturiskais sūkņa taimera incidents

Vecākā sistēmas stadijā tika testēts 30 s auto-OFF, un vairākos mēģinājumos sūknis pēc 40–45 s joprojām bija ON, līdz tika nosūtīts OFF vai notika ESP32 restarts. Šis incidents ir svarīgs, jo tas izskaidro, kāpēc vēlāk tika nostiprināts lokālais firmware hard-limit un ārējais `laistisana.sh` drošības tīkls.

Vecajos materiālos ir arī periods, kur ESP32 `MAX_PUMP_SECONDS` bija minēts kā **600 s**. Tas ir **superseded**. Pašreizējais firmware source-of-truth ir **180 s**.

Pašreizējā firmware:

- `laist` = 30 s;
- `laist_X` = minūšu komanda, bet nekad virs 180 s;
- HA `ON` pašreiz sāk seansu ar 180 s limitu;
- `stop` izslēdz sūkni;
- `servicePump()` uztur lokālo OFF neatkarīgi no HA/Telegram.

## 9. Home Assistant un MQTT līgums

Pašreizējais brokeris atrodas RPi5 lokālajā tīklā uz `192.168.0.180:1883`.

Galvenie topic:

| Topic | Nozīme |
|---|---|
| `balkons/status` | retained online/offline availability |
| `balkons/sukna/komanda` | HA → ESP32 `ON` / `OFF` |
| `balkons/sukna/stends` | ESP32 → HA retained pump state |
| `balkons/log` | ESP32 notikumu logi |
| `balkons/cmd` | RPi5/bridge → ESP32 teksta komandas |
| `balkons/telegram_out` | ESP32 → RPi5 Telegram izejošās ziņas |
| `balkons/pukeN/mitrums` | sensoru kategorijas |

ESP32 publicē Home Assistant MQTT discovery 15 sensoriem un vienam sūkņa switch. Ierīces identitāte ir `balkons_esp32`, nosaukums “Balkona Laistīšana”. Discovery un pump/status dati izmanto retained semantiku; availability izmanto `balkons/status` un MQTT Last Will.

## 10. Telegram migrācija ESP32 → RPi5

Agrīnā versijā ESP32 vienlaicīgi:

- lasīja 15 sensorus;
- vadīja sūkni;
- turēja Wi-Fi/MQTT;
- uzturēja ārēju TLS savienojumu Telegram API.

Vēsturē tika novērota nestabilitāte/heap fragmentācija un pazūdošs Telegram savienojums. Arhitektūras risinājums bija izņemt Telegram TLS no ESP32.

Pēc migrācijas:

- ESP32 paliek hardware controller + MQTT endpoint;
- RPi5 serviss apstrādā Telegram;
- ESP32 nosūta Telegram paredzēto tekstu pa MQTT;
- RPi5 bridge var restartēties, neizmainot sūkņa lokālo drošības loģiku.

Vēsturiskā RPi5 audita logā `balkons-bot.service` bija redzams kā aktīvs serviss kopā ar `balkons-log.service`. Citā 2026-07-17 incidenta logā `balkons-bot.service` atkārtoti krita ar `Failed with result 'exit-code'` aptuveni ik pēc 10 s. Tas ir vēsturisks outage pierādījums un papildu pamatojums tam, kāpēc Telegram bridge nedrīkst būt safety-critical sūkņa OFF mehānisms.

Pašreizējais RPi5 bridge source vēl **nav** importēts šajā repo; pirms importēšanas jāņem svaigā versija no RPi5 un jāizņem secrets.

## 11. Vēsturiskais `laistisana.sh`

Claude-era vēsturē eksistēja RPi5 `laistisana.sh` kā augstāka līmeņa sūkņa vadības drošības skripts.

Atgūtā vēsturiskā funkcionalitāte:

- Home Assistant REST API;
- sūkņa ON;
- retry loģika — līdz 3 mēģinājumiem ar apmēram 3 s pauzi;
- kontrolēts gaidīšanas intervāls, vienā vēsturiskā variantā 60 s;
- OFF pēc intervāla;
- shell `trap` kā papildu OFF drošības tīkls kļūdas gadījumā;
- Telegram paziņojums par sākumu/beigām/kļūdu;
- akreditācijas dati tika pārvietoti uz privātu env failu, nevis glabāti skripta source.

Ir dokumentēts reāls 60 s laistīšanas izpildījums, kur ON, 60 s gaidīšana, OFF un Telegram paziņojums bija SUCCESS.

Vēsturē arī konstatēts, ka Hermes `ha_call_service` dažkārt varēja atgriezt `success`, lai gan komanda līdz ESP32 faktiski nebija nonākusi. Tāpēc konkrētajā periodā `laistisana.sh` ar HA REST API/retry tika uzskatīts par uzticamāku ceļu.

Šis ir **vēsturisks kontrakts**, nevis garantija par šodienas RPi5 faila saturu. Pirms repo importēšanas jāsalīdzina ar dzīvo RPi5.

## 12. Komandas un statusa diagnostika

Pašreizējais ESP32 teksta komandu komplekts:

- `laist`;
- `laist_X`;
- `stop`;
- `mitrums`;
- `raw`;
- `statuss`;
- `statistika`.

`statuss` rāda:

- sūkņa ON/OFF;
- atlikušās sekundes, ja sūknis darbojas;
- Wi-Fi IP;
- RSSI;
- MQTT state;
- brīvo heap;
- vietējo laiku;
- uptime.

Šis bija arī galvenais post-OTA runtime verifikācijas ceļš 2026-08-16.

## 13. Laiks / NTP

Vēsturiskajā firmware bija smalka timezone kļūda: pēc TZ uzstādīšanas tika izmantots `configTime(0, 0, ...)`, kas aktuālajā Arduino-ESP32 var pārrakstīt laika konfigurāciju uz UTC.

Bootstrap auditā tas tika labots uz `configTzTime(...)` ar Europe/Berlin CET/CEST TZ noteikumu. Post-OTA `/statuss` 2026-08-16 parādīja pareizu lokālo Vācijas vasaras laiku, tāpēc šis labojums ir production-verificēts.

## 14. Watchdog, reconnect un restartu drošība

Pašreizējā firmware drošības/robustness modelis ietver:

- ESP Task Watchdog;
- Wi-Fi reconnect;
- MQTT reconnect;
- MQTT command queue;
- Telegram outgoing queue, lai īslaicīgs MQTT zudums nezaudētu paziņojumu;
- retained pump state;
- RTC `pumpWasRunning` indikāciju;
- lokālu pump timer servicing arī garākās sensoru/komandu darbībās;
- syslog uz RPi5 UDP/514.

Mērķis: tīkla, HA vai Telegram atteice nedrīkst atstāt sūkni bez lokāla laika limita.

## 15. OTA vēsture un pašreizējais deploy modelis

OTA hostname: `balkons-esp32` / `balkons-esp32.local`.

2026-08-16 tika verificēts mDNS `_arduino._tcp` ieraksts ar portu 3232 un `auth_upload=yes`. OTA faktiski izmanto UDP kontroles/invitation daļu un pēc tam datu pārraides savienojumu; vienkāršs TCP `nc -vz ... 3232` nav derīgs OTA readiness tests.

Repo politika:

- CI ir build-only;
- merge pats nekad neveic OTA;
- production OTA tiek palaists tikai ar atsevišķu owner autorizāciju;
- uploader parole netiek glabāta Git;
- `include/secrets.h` ir lokāls un ignorēts;
- pilns pašreizējā ESP32 flash backup nav veikts; ir verificēts source baseline, nevis flash dump.

## 16. Source baseline / vecais Lenovo projekts

Vecais lokālais PlatformIO projekts uz Lenovo tika salīdzināts ar bootstrap avotu. Vecā `src/main.cpp` SHA-256 precīzi sakrita ar importēto sākuma source baseline:

`f13db85f3808701573d724663c463534455b23795a2a534fa3403610484008e9`

Vecajā projekta mapē bija arī:

- lokāls `include/secrets.h`;
- `.pio` build artefakti ar kompilētām credential vērtībām;
- `.vscode` lokālie ceļi;
- firmware bin/ELF artefakti.

Tie apzināti **netika** importēti publiskajā repo.

## 17. Zināmās pretrunas un superseded fakti

Šo sadaļu izmanto, lai turpmāk neatdzīvinātu vecas kļūdas.

| Vecais apgalvojums | Pašreizējais statuss |
|---|---|
| Fail-safe sūknim izmanto `NC` | **WRONG / superseded.** Pareizais dizains ir `NO`. |
| Flyback diode/kondensatori vēl tikai “ceļā” | **Superseded.** Atgūts pierādījums par `1N5408 + 100nF + 470µF` un ~15 s PASS testu. |
| `MAX_PUMP_SECONDS=600` | **Superseded.** Current firmware = 180 s. |
| 30 s auto-off nestrādā | **Historical incident.** Current firmware izmanto pārstrādātu lokālo session timer + 180 s hard limit. |
| Telegram TLS darbojas uz ESP32 | **Superseded.** Telegram bridge pārvietots uz RPi5. |
| GPIO25 jau izmantots MUX S1 | **Nav taisnība.** Current physical/code S1 joprojām GPIO12 līdz fiziskai pārlikšanai. |
| Vecais Claude raw eksports ir source-of-truth | **Nē.** Tas satur gan stale state, gan secrets, gan draftus. |
| Ideju kolāžas/DIN/IP65 attēli rāda faktisko montāžu | **Nē.** Tie ir dizaina ieteikumi, nevis pierādījums par reālo vadojumu. |

## 18. Kas joprojām jānostiprina ar svaigu pierādījumu

- vienreiz nofotografēt faktiskās releja `COM/NO/NC` spailes un marķēt sūkņa strāvas ceļu;
- nofotografēt pašreizējo `1N5408` polaritāti un `100nF + 470µF` izvietojumu pēc pēdējās fiziskās montāžas;
- ja aparatūra ir pārtaisīta, atkārtot īsu kontrolētu pump ON/OFF stabilitātes testu ar cilvēku klāt;
- pārbaudīt pašreizējo 15 sensoru veselību (`/mitrums`, `/raw`) — vecie `puke_5`/`Puķe 8` incidenti nav current status;
- fiziski pārvietot MUX S1 GPIO12 → GPIO25 un tikai tad mainīt firmware;
- paņemt no dzīva RPi5 aktuālo `balkons-bot`/Telegram bridge, `balkons-log` un `laistisana.sh`, sanitizēt un importēt atsevišķā PR;
- pēc RPi5 importa dokumentēt precīzu systemd unit/source path un HA vadības kontraktu.

## 19. Drošības noteikumi vēstures izmantošanai

- Nekad necommitot Claude raw ZIP/JSON/Markdown eksportu.
- Nekad necommitot vecās sarunās redzamus tokenus, paroles, API key, PAT, Telegram bot tokenus, HA tokenus vai OTA paroli.
- Nekad necommitot `.pio`, vecus firmware bin/ELF, flash dumpus vai arhīvus ar credentialiem.
- Vēsturisku “SUCCESS” neuzskatīt par pašreizējo runtime stāvokli, ja tas var būt mainījies.
- Ja fiziskā aparatūra ir pārvadota/pārvadota no jauna, vecs wiring apraksts ir jāuzskata par “historically verified”, nevis automātiski “currently verified”.

## 20. Īsā continuity karte

Ja nākamajā čatā vajag ātri turpināt projektu, pietiek ar šo secību:

1. `README.md` — kā būvēt/deployot un pašreizējie TODO.
2. `docs/HISTORICAL_KNOWLEDGE_BASE.md` — visa atgūtā Claude-era zināšanu bāze un pretrunas.
3. `docs/HARDWARE.md` — aktuālais pinout un fiziskā aizsardzība.
4. `docs/SAFETY.md` — drošības robežas.
5. `docs/MQTT_HOME_ASSISTANT.md` — topic/HA līgums.
6. `docs/SOURCE_BASELINE.md` — source provenance.
7. `src/main.cpp` — gala firmware source-of-truth.

Raw Claude vēsture turpmāk ir **arhīvs**, nevis ikdienas darba instruments.