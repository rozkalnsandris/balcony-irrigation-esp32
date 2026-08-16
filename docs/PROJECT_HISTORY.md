# Projekta vēstures kopsavilkums

Šis nav pilns Claude čatu eksports. Tas ir sanitizēts projekta lēmumu kopsavilkums.

## 2026-05 → 2026-06

Projekts attīstījās no vienkāršas balkona automātiskās laistīšanas idejas līdz ESP32 sistēmai ar daudziem augsnes sensoriem, multipleksoru, sūkni, releju un Home Assistant/MQTT integrāciju. Claude eksportā šim projektam ir vairākas garas sarunas, tostarp 2026-05-01 pamata saruna un 2026-06-27 lielais turpinājums.

## Arhitektūras lēmumi

- 15 sensori tiek lasīti caur CD74HC4067, nevis patērējot 15 atsevišķas ESP32 analogās ieejas.
- MQTT ir centrālā integrācijas saskarne ar Home Assistant/RPi5.
- Telegram TLS loģika tika iznesta no ESP32 uz RPi5 pusi, lai samazinātu ESP32 nestabilitātes risku.
- Sūkņa drošības taimeris tika nostiprināts pašā ESP32, lai OFF nebūtu atkarīgs tikai no HA/cron/tīkla.
- OTA kļuva par standarta firmware atjaunināšanas ceļu; parole jāglabā ārpus Git.

## Vēsturiska RPi5 automatizācija

Vēsturē eksistē `laistisana.sh`, kas caur Home Assistant REST API izmantoja retry, lock un `trap` OFF drošībai. Tā grafiki un aktivizācijas stāvoklis dažādos brīžos mainījās, tāpēc šo kopiju nedrīkst uzskatīt par aktuālo production avotu, kamēr tā nav paņemta no dzīva RPi5.

## Pašreizējie neatrisinātie punkti

- fiziski verificēt NO releja kontaktu;
- verificēt flyback diodi/decoupling aizsardzību;
- kontrolēti pārbaudīt vairākus sūkņa ON/OFF ciklus pēc elektriskās aizsardzības;
- gala montāžā izlemt/pabeigt GPIO12 → GPIO25 MUX S1 pārcelšanu;
- importēt tikai sanitizētas, aktuālas RPi5 integrācijas versijas.

## Kāpēc raw Claude eksports nav repo

Eksportā bija paroles, tokeni un citi sensitīvi dati, kā arī novecojis stāvoklis. Repo satur tikai sanitizētu tehnisko rezultātu, nevis sarunu arhīvu.
