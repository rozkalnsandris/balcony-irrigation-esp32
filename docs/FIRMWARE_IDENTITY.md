# Firmware build identity

## Mērķis

Katram PlatformIO build firmware tagad ir iebūvēta Git revīzija, lai pēc OTA var tieši pierādīt, kurš source commit darbojas uz ESP32. Tas novērš situāciju, kur deploy var pārbaudīt tikai netieši pēc restartēta uptime, laika vai tīkla stāvokļa.

## Kā revīzija tiek iegūta

`platformio.ini` izmanto PlatformIO dynamic build flags:

```ini
build_flags = !python scripts/git_rev_macro.py
```

`scripts/git_rev_macro.py` nolasa pilno `git rev-parse --verify HEAD` SHA un izveido C/C++ makro `FIRMWARE_GIT_REV`.

Ja darba kokā ir tracked izmaiņas vai neignorēti untrackoti faili, revīzijai tiek pievienots `-dirty`. Ignorētie lokālie faili, tostarp `include/secrets.h` un `.pio/`, paši par sevi build nepadara dirty.

Ja Git revīziju noteikt nevar, PlatformIO build failo. `src/main.cpp` ir tikai compile-time fallback `unknown` gadījumam, ja source tiek kompilēts ārpus parastā PlatformIO/Git ceļa.

## Kur runtime to redz

Komanda:

```text
/statuss
```

atbildē pievieno:

```text
Firmware: <40-char Git SHA>
```

Arī startup logs pievieno firmware revīziju pie restarta iemesla un brīvās atmiņas.

## Droša OTA verifikācija

Pēc explicit owner autorizēta OTA:

1. pirms build/upload pārbauda, ka lokālais checkout ir uz `main`, clean un sinhronizēts ar `origin/main`;
2. pieraksta `git rev-parse HEAD`;
3. veic authenticated `esp32_ota` upload;
4. sagaida ESP32 atgriešanos tīklā/MQTT;
5. nosūta `/statuss`;
6. `Firmware:` vērtībai **precīzi jāsakrīt** ar autorizēto `main` SHA un tā nedrīkst beigties ar `-dirty`.

Ja revīzija nesakrīt, deploy nedrīkst klasificēt kā verificētu pat tad, ja ESP32 ir online.

## Drošības robeža

Šī funkcija ir tikai observability/provenance. Tā nemaina:

- releja GPIO vai active-low semantiku;
- sūkņa 30 s noklusējumu vai 180 s hard limitu;
- Home Assistant `ON/OFF` semantiku;
- MUX pinout;
- MQTT credentials vai OTA paroli;
- faktu, ka merge pats par sevi **nedrīkst veikt OTA**.
