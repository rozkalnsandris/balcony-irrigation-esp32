# Firmware build identity

## Mērķis

Katram PlatformIO build firmware tagad ir iebūvēta Git revīzija, lai pēc OTA var tieši pierādīt, kurš source commit darbojas uz ESP32. Tas novērš situāciju, kur deploy var pārbaudīt tikai netieši pēc restartēta uptime, laika vai tīkla stāvokļa.

## Kā revīzija tiek iegūta

`platformio.ini` izmanto PlatformIO PRE Advanced Scripting hook:

```ini
extra_scripts = pre:scripts/git_rev_macro.py
```

`scripts/git_rev_macro.py` tiek ielādēts pašā PlatformIO build procesā, tātad tas izmanto PlatformIO Python vidi un nav atkarīgs no hosta `/bin/sh` komandas ar nosaukumu `python`.

Skripts nolasa pilno `git rev-parse --verify HEAD` SHA un pievieno `FIRMWARE_GIT_REV` caur PlatformIO `CPPDEFINES`. String vērtības quoting veic `env.StringifyMacro(...)`, nevis manuāla shell escaping loģika.

Ja darba kokā ir tracked izmaiņas vai neignorēti untrackoti faili, revīzijai tiek pievienots `-dirty`. Ignorētie lokālie faili, tostarp `include/secrets.h` un `.pio/`, paši par sevi build nepadara dirty.

Ja Git revīziju noteikt nevar, PlatformIO build failo. `src/main.cpp` ir tikai compile-time fallback `unknown` gadījumam, ja source tiek kompilēts ārpus parastā PlatformIO/Git ceļa.

## 2026-08-16 Lenovo portability incident

Pirmais owner-autorizētais OTA mēģinājums pēc runtime identity ieviešanas apstājās droši **pirms compilation/upload**, jo iepriekšējā konfigurācija izmantoja:

```ini
build_flags = !python scripts/git_rev_macro.py
```

Lenovo `/bin/sh` neatrada hosta komandu `python`, lai gan PlatformIO paša Python vide eksistēja. Tas nebija OTA auth, transport vai ESP32 darbības traucējums. Labojums ir PRE `extra_scripts` hook augstāk, kas šo host alias dependency pilnībā noņem.

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
