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

## Operacionālā politika GitHubam

- GitHub Actions nedrīkst saturēt automātisku production OTA/upload uz balkona ESP32. Repo CI izmanto tikai `esp32_ci` build vidi; OTA konfigurācija ir atsevišķā `esp32_ota` vidē.
- PR/merge pats par sevi nedrīkst ieslēgt sūkni.
- Skriptus, kas izsauc HA `switch.turn_on`, nedrīkst palaist kā “testu”.
- Secrets nedrīkst būt commit vēsturē, pat privātā repo.

## Atvērtais fiziskais risks

Vēsturē palicis neatrisināts/nenoslēgts back-EMF un barošanas trokšņu jautājums. Flyback diodes un kondensatoru esamība jāapstiprina uz faktiskās iekārtas, nevis no sarunas atmiņas.
