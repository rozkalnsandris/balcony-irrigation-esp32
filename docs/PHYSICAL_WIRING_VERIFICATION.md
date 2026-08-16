# Fiziskās montāžas pārbaudes checklist

Šis dokuments ir issue #2 **read-only / pump-OFF** pārbaudes līgums. Tas palīdz vienreiz nostiprināt faktiskās pašreizējās releja un motora aizsardzības detaļas, nepaļaujoties tikai uz vēsturisko Claude-era aprakstu.

## Drošības robeža

Šis checklist **neautorizē sūkņa ieslēgšanu**.

Pirms pieskaršanās vadiem vai termināļiem:

1. pārliecinies, ka sūknis ir OFF;
2. atvieno sūkņa/motora barošanu, ja jāaiztiek spailes vai komponenti;
3. neveic pārspraušanu zem sprieguma;
4. neveic MQTT, Home Assistant vai Telegram pump-ON komandu;
5. ja ir neskaidrība par termināļu marķējumu vai polaritāti, nofotografē un apstājies — neuzmini.

## A. Releja fail-safe kontakts

Mērķis: pierādīt faktisko pašreizējo sūkņa strāvas ceļu.

Nepieciešamie foto:

- releja moduļa kopskats, kur redzams `COM / NO / NC` marķējums;
- tuvplāns ar visām trim skrūvspailēm un pievienotajiem vadiem;
- foto, kur var izsekot sūkņa barošanas vadam līdz releja kontaktam.

Jāfiksē tikai tas, ko var skaidri redzēt:

- kurš vads ir `COM`;
- vai sūkņa pārslēgtais vads patiešām ir uz `NO`;
- vai `NC` sūkņa ķēdei nav izmantots.

Pareizais projekta dizains ir **NO**, lai releja/ESP32 vadības zuduma gadījumā sūknis paliktu OFF. Līdz foto pierādījumam exact pašreizējais termināļu vadojums paliek `UNVERIFIED_PHYSICAL_STATE`.

## B. 1N5408 flyback diode

Vēsturē ir dokumentēts, ka `1N5408` tika uzstādīta pāri R385 sūkņa termināļiem un pēc tam ~15 s tests bija stabils. Šeit jāpārbauda tikai pašreizējais montāžas stāvoklis.

Nepieciešamie foto:

- abi sūkņa termināļi vienā kadrā;
- diode pilnā garumā, lai redzama tās josla/polaritātes marķējums;
- pietiekami plašs kadrs, lai redzams, uz kuriem sūkņa poliem diode ir pievienota.

No foto jāpieraksta:

- kur atrodas diodes joslas gals;
- kuri sūkņa `+` un `-` vadi ir redzami vai droši identificējami;
- vai diode joprojām fiziski atrodas pāri motora termināļiem.

Ja `+/-` no foto nevar droši noteikt, polaritāti **neizsecina**.

## C. 100 nF + 470 µF decoupling

Vēsturiskais suppression komplekts ir:

- `100 nF` keramiskais kondensators;
- `470 µF` elektrolītiskais/bulk kondensators;
- kopā ar `1N5408` flyback diodi.

Nepieciešamie foto:

- 100 nF komponenta novietojums un abi pieslēguma punkti;
- 470 µF kondensatora novietojums;
- 470 µF polaritātes marķējums/stripe, ja tas ir redzams;
- kopskats, kur redzama saistība starp sūkni, barošanu, releju un suppression komponentiem.

Jāpieraksta tikai redzamie fakti; vēsturisko shēmu nedrīkst izmantot kā aizvietojumu pašreizējai montāžai.

## D. MUX S1 / GPIO12 robeža

Šīs pārbaudes laikā MUX vads **nav jāpārliek**.

Pašreizējais firmware izmanto:

- `MUX S1 = GPIO12`.

Plānotais gala variants ir GPIO25, taču firmware nedrīkst mainīt uz GPIO25 pirms fiziska pārlikšana tiešām ir veikta un dokumentēta.

Ja fotografējot ērti redzams MUX S1 vads, var pievienot vienu foto kā pašreizējā stāvokļa pierādījumu, bet šī issue robežās tas nav obligāts.

## E. Pierādījumu ieraksts GitHub

Kad foto ir pieejami, issue #2 jāpapildina ar:

- datumu/laiku;
- foto vai to GitHub attachmentiem;
- `Relay pump path: COM -> NO` tikai tad, ja tas tiešām ir redzams;
- `1N5408 orientation: VERIFIED` tikai tad, ja `+/-` un joslas gals ir droši identificēts;
- `100nF placement: VERIFIED` / `470µF placement: VERIFIED` tikai tad, ja savienojumi ir redzami;
- jebkuru neatrisinātu `UNKNOWN` punktu bez minējumiem.

## F. Kad drīkst domāt par dzīvu sūkņa testu

Ja montāža kopš vēsturiskā ~15 s PASS **nav mainīta** un foto tikai apstiprina esošo stāvokli, jauns sūkņa tests nav automātiski vajadzīgs.

Ja releja ķēde, diode, kondensatori, sūkņa barošana vai vadi kopš tā laika ir pārtaisīti, tad atsevišķā owner-gated solī var definēt vienu īsu kontrolētu ON/OFF testu ar:

- cilvēku pie ierīces;
- tūlītēju fizisku power-off iespēju;
- iepriekš pārbaudītu ESP32/MQTT stāvokli;
- pēc testa obligātu pump-OFF un runtime statusa pārbaudi.

Līdz šādai atsevišķai autorizācijai: **inspection only, pump OFF**.
