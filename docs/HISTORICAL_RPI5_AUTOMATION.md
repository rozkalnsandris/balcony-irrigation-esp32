# Vēsturiskā RPi5 / Home Assistant laistīšanas automatizācija

Šis fails saglabā Claude-era RPi5/HA automatizācijas faktus, kas nav pašreizējā ESP32 firmware source daļa.

**Svarīgi:** šeit aprakstītais ir vēsturisks stāvoklis. Pirms jebkādas production izmaiņas aktuālie faili, systemd unit un schedule jānolasa no dzīva RPi5. No vecā čata nedrīkst rekonstruēt production secrets.

## Arhitektūras robeža

Telegram TLS tika pārvietots no ESP32 uz RPi5. ESP32 palika hardware/safety/MQTT controller, bet RPi5 apstrādāja Telegram un augstāka līmeņa Home Assistant vadību.

Vēsturiskā RPi5 auditā bija redzami:

- `balkons-bot.service` — Balkona Telegram-MQTT bots;
- `balkons-log.service` — Balkona laistīšanas MQTT logs.

Citā 2026-07-17 incidenta logā `balkons-bot.service` atkārtoti beidzās ar `Failed with result 'exit-code'` aptuveni ik pēc 10 sekundēm. Tas ir vēsturisks outage pierādījums un pamatojums tam, ka Telegram bridge nekad nedrīkst būt vienīgais pump OFF mehānisms.

## `laistisana.sh` vēsturiskais kontrakts

Atgūtajos ierakstos RPi5 skripts `laistisana.sh`:

1. izsauca Home Assistant REST API, lai ieslēgtu sūkni;
2. ON komandai izmantoja retry — līdz 3 mēģinājumiem ar apmēram 3 s pauzi;
3. gaidīja konfigurētu `DURATION`;
4. sūtīja OFF;
5. izmantoja shell `trap` kā papildus OFF drošības tīklu kļūdas gadījumā;
6. sūtīja Telegram paziņojumu par sākumu, beigām vai kļūdu;
7. akreditācijas datus turēja privātā env failā, nevis source.

Vienā saglabātā pārmaiņu punktā `DURATION` tika mainīts **no 110 s uz 60 s**.

## Vēsturiskais grafiks

Vienā Claude-era production stāvoklī vakara laistīšana bija:

- **katru dienu 23:00**;
- `laistisana.sh` ieslēdza sūkni;
- gaidīja **60 sekundes**;
- 23:01 sūtīja OFF;
- Telegram paziņoja par sākumu un pabeigšanu;
- `trap` bija fallback kļūdas gadījumam.

Šis grafiks bija aktīvs konkrētajā vēsturiskajā brīdī. **Tas nav pašreizējā schedule apliecinājums.** Pirms aktivizēt/atjaunot jebkādu cron/systemd timer, jāpārbauda dzīvais RPi5.

## Reāls 60 s izpildījums

Vēsturē ir atgūts veiksmīgs cikls:

- sūkņa ON caur HA REST API ar retry wrapperi;
- 60 s darbība;
- OFF;
- Telegram paziņojums;
- gala stāvoklis OFF.

Tas pierāda, ka šī automatizācijas versija vismaz vienā reālā ciklā darbojās end-to-end.

## `ha_call_service` problēma

Vēsturē tika novērots, ka Hermes Home Assistant integrācijas `ha_call_service` dažkārt varēja atgriezt `success`, lai gan komanda līdz ESP32 faktiski nebija nonākusi.

Tāpēc konkrētajā periodā laistīšanai priekšroka tika dota `laistisana.sh` ar tiešu HA REST API + retry, nevis `ha_call_service` rezultāta aklai uzticēšanai.

## Vēsturiskie Home Assistant entity nosaukumi

Atgūtajos ierakstos parādās:

- `switch.balkona_laistisana_suknis` — sūkņa HA switch;
- `smart_wi_fi_plug` un `smart_wi_fi_plug_power` — atsevišķos statusa ierakstos redzamas kā OFF / 0 W.

No šiem ierakstiem **nedrīkst secināt**, ka smart plug ir pašreizējā sūkņa obligāta drošības ķēdes daļa. Tas ir tikai vēsturē redzēts HA state konteksts.

## Sūkņa taimera vēsturiskais incidents

Pirms pašreizējā firmware drošības taimera nostiprināšanas bija reāli testi, kuros:

- pēc apmēram 22 s sūknis joprojām bija ON;
- pēc apmēram 40–45 s sūknis joprojām bija ON;
- vienā mēģinājumā tas tika izslēgts manuāli;
- citā OFF sakrita ar ESP32 reboot.

Toreiz secinājums bija, ka 30 s auto-OFF konkrētajā firmware/automatizācijas versijā nestrādāja droši.

Vēlāk ārējais skripts kļuva par papildu OFF slāni un ESP32 firmware tika nostiprināts ar lokālu seansa taimeri. Vecajos materiālos sastopamais `MAX_PUMP_SECONDS=600` ir **superseded**; pašreizējais firmware source-of-truth ir **180 s**.

## Secrets un importēšanas noteikumi

Vecajos RPi5/Claude materiālos bija Home Assistant un Telegram akreditācijas dati. Publiskajā repo nedrīkst nonākt:

- HA bearer token;
- Telegram bot token/chat ID, ja tas nav apzināti publisks;
- Wi-Fi/MQTT/OTA paroles;
- veci `.env` faili;
- raw logu/arhīvu kopijas ar secrets.

Pirms aktuālā RPi5 source importa:

1. nolasīt dzīvos failus read-only;
2. noteikt faktiskos systemd unit/schedule;
3. sanitizēt secrets uz `.example`/env kontraktu;
4. pievienot repo atsevišķā PR;
5. neaktivizēt pumpi/importa testu laikā.

Plašāka projekta vēsture un superseded-fakti: [`HISTORICAL_KNOWLEDGE_BASE.md`](HISTORICAL_KNOWLEDGE_BASE.md).