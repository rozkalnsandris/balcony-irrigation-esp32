# Drošības modelis

Šī sistēma vada fizisku ūdens sūkni, tāpēc firmware drošības robežas ir svarīgākas par ārējām automatizācijām.

## Firmware drošības mehānismi

1. Relejs `setup()` sākumā tiek uzstādīts OFF pirms pārējās inicializācijas.
2. Vienas sūkņa sesijas absolūtais hard-limit ir 180 s.
3. Laika pagarināšana nevar pārsniegt šo limitu.
4. OTA sākumā relejs tiek piespiests OFF; neveiksmīga OTA gadījumā ierīce restartējas ar OFF starta stāvokli.
5. RTC flags ļauj pēc restarta brīdināt, ja iepriekš sūknis darbojās.
6. Watchdog aizsargā pret iesprūdušu loop tasku.
7. Wi-Fi un MQTT reconnect ir veidots tā, lai sūkņa lokālais taimeris turpinātu tikt pārbaudīts neatkarīgi no ārējiem servisiem.
8. Komandas tiek rindotas; vienā loop ciklā tiek apstrādāta viena komanda.

## Fiziskais fail-safe modelis

Pareizais projekta dizains ir releja **NO (Normally Open)** kontakts sūkņa strāvas ceļā. Tas nozīmē, ka releja vadības zuduma, ESP32 crash vai nepalaišanās gadījumā sūknim pēc noklusējuma jābūt OFF.

Vecā vēstures draftā bija kļūdains `NC` apgalvojums; tas ir superseded. Koriģētajā vēstures materiālā NO ir skaidri noteikts kā pareizais variants, taču exact pašreizējās fiziskās spailes vēl ir vērts vienreiz nofotografēt un dokumentēt.

## Back-EMF aizsardzības statuss

Back-EMF vairs nav korekti saukt par “nekad neatrisinātu”. Claude-era vēsturē ir atgūts:

- agrāks incidents, kur ESP32 restartējās tieši pēc sūkņa atslēgšanas;
- dokumentēts aparatūras labojums ar `1N5408` flyback diodi, `100 nF` keramisko un `470 µF` bulk/elektrolītisko kondensatoru;
- vismaz viens reāls ~15 s post-fix sūkņa tests, kur sūknis atgriezās OFF un ESP32 nepazuda.

Tāpēc statuss ir **historical fix + short-test PASS**, nevis “nav uzstādīts”.

Tas tomēr neaizstāj svaigu fizisku pārbaudi pēc jebkādas pārlikšanas. Pirms jaunas aparatūras pārtaisīšanas vai ilgākiem sūkņa cikliem jāpārbauda faktiskā diodes polaritāte, kondensatoru izvietojums un releja termināļi.

## Kāpēc ārējais OFF nav vienīgais drošības slānis

Vēsturē bija periods, kad 30 s auto-OFF pārbaude neizdevās un sūknis palika ON līdz ārējam OFF vai restartam. Tas motivēja pašreizējo modeli:

- ESP32 lokālais seansa taimeris;
- 180 s absolūtais hard-limit;
- ārējie HA/RPi5 skripti ir papildu slānis, nevis vienīgais OFF mehānisms.

Vēsturiskais `MAX_PUMP_SECONDS=600` ir superseded. Current source-of-truth ir 180 s.

## RPi5/Telegram drošības robeža

Telegram TLS tika pārvietots no ESP32 uz RPi5. Vēsturē `balkons-bot.service` ir gan normāli darbojošies periodi, gan atkārtotu crash/restartu logs. Tāpēc Telegram bridge nedrīkst būt safety-critical sūkņa taimeris.

Vēsturiskais `laistisana.sh` izmantoja HA REST API retry un shell `trap` OFF kā papildu drošības tīklu. Aktuālā skripta versija vēl jāpaņem no dzīva RPi5 pirms importēšanas repo.

## Operacionālā politika GitHubam

- GitHub Actions nedrīkst saturēt automātisku production OTA/upload uz balkona ESP32. Repo CI izmanto tikai `esp32_ci` build vidi; OTA konfigurācija ir atsevišķā `esp32_ota` vidē.
- PR/merge pats par sevi nedrīkst ieslēgt sūkni.
- Skriptus, kas izsauc HA `switch.turn_on`, nedrīkst palaist kā parastu source testu.
- Secrets nedrīkst būt commit vēsturē, pat privātā repo.
- Fizisku sūkņa testu drīkst veikt tikai pēc atsevišķas owner autorizācijas, ar cilvēku klāt un tūlītēju fizisku power-off iespēju.

## Pašreizējie atvērtie drošības pierādījumi

- nofotografēt faktiskās `COM/NO/NC` spailes;
- nofotografēt pašreizējo `1N5408` polaritāti un `100nF + 470µF` izvietojumu;
- ja wiring ir mainīts kopš historical PASS, atkārtot tikai īsu kontrolētu testu;
- MUX S1 atstāt GPIO12 firmware līdz fiziska pārlikšana uz GPIO25 tiešām ir veikta.

Pilna recovered provenance: [`HISTORICAL_KNOWLEDGE_BASE.md`](HISTORICAL_KNOWLEDGE_BASE.md).