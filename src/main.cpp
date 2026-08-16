#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include <time.h>

#include "esp_task_wdt.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_arduino_version.h"

#include "secrets.h"

#ifndef FIRMWARE_GIT_REV
#define FIRMWARE_GIT_REV "unknown"
#endif

// Palielina Arduino loop() task steku.
SET_LOOP_TASK_STACK_SIZE(16 * 1024);

// ============================================================
// PAMATA IESTATĪJUMI
// ============================================================

constexpr char MQTT_SERVER[] = "192.168.0.180";
constexpr uint16_t MQTT_PORT = 1883;

constexpr char MQTT_CLIENT_ID_BASE[] = "balkons_esp32";

constexpr char T_STATUS[]   = "balkons/status";
constexpr char T_PUMP_CMD[] = "balkons/sukna/komanda";
constexpr char T_PUMP_ST[]  = "balkons/sukna/stends";
constexpr char T_LOG[]      = "balkons/log";
constexpr char T_CMD[]      = "balkons/cmd";
constexpr char T_OUT[]      = "balkons/telegram_out";

constexpr char SYSLOG_SERVER[] = "192.168.0.180";
constexpr uint16_t SYSLOG_PORT = 514;

constexpr char NTP_SERVER_1[] = "pool.ntp.org";
constexpr char NTP_SERVER_2[] = "time.cloudflare.com";
constexpr char NTP_SERVER_3[] = "time.google.com";

constexpr char TZ_INFO[] = "CET-1CEST,M3.5.0,M10.5.0/3";

constexpr char OTA_HOSTNAME[] = "balkons-esp32";

// ============================================================
// DROŠĪBA / TAIMERI
// ============================================================

constexpr uint32_t DEFAULT_PUMP_SECONDS = 30;
constexpr uint32_t MAX_PUMP_SECONDS = 180;

constexpr uint32_t WDT_TIMEOUT_S = 30;

constexpr uint32_t WIFI_RECONNECT_INTERVAL_MS = 10000UL;
constexpr uint32_t MQTT_RECONNECT_INTERVAL_MS = 5000UL;
constexpr uint32_t MQTT_PUBLISH_INTERVAL_MS = 60000UL;

constexpr uint16_t MQTT_KEEPALIVE_S = 30;
constexpr uint16_t MQTT_SOCKET_TIMEOUT_S = 2;
constexpr uint16_t MQTT_BUFFER_SIZE = 1024;

// ============================================================
// PIN IESTATĪJUMI
// ============================================================

constexpr uint8_t RELAY_PIN = 26;

constexpr uint8_t MUX_S0 = 13;

// SVARĪGI:
// Pašlaik fiziskais vads tev ir uz GPIO12.
// Gala montāžā ieteicams pārvietot šo vadu uz GPIO25
// un tikai tad nomainīt šo vērtību no 12 uz 25.
constexpr uint8_t MUX_S1 = 12;

constexpr uint8_t MUX_S2 = 14;
constexpr uint8_t MUX_S3 = 27;

constexpr uint8_t MUX_SIG = 34;

constexpr uint8_t SENSOR_COUNT = 15;

// Relejs ir active-low.
constexpr uint8_t RELAY_ON = LOW;
constexpr uint8_t RELAY_OFF = HIGH;

// ============================================================
// MITRUMA KALIBRĀCIJA
// ============================================================

// Atskaites punkti:
// galīgi sauss ~2217
// vajag laistīt ~1850
// mitrs ~1175

constexpr int MOISTURE_DRY_THRESHOLD = 2000;
constexpr int MOISTURE_WET_THRESHOLD = 1400;

// ============================================================
// RTC ATMIŅA
// ============================================================

// Ja ESP32 restartējas laistīšanas laikā un RTC atmiņa saglabājas,
// pēc restarta varam brīdināt lietotāju.
RTC_DATA_ATTR bool rtcPumpWasRunning = false;

// ============================================================
// TĪKLS / MQTT
// ============================================================

WiFiClient mqttNet;
PubSubClient mqtt(mqttNet);
WiFiUDP udp;

char mqttClientId[48] = {0};

bool wifiOnline = false;
bool wifiEverConnected = false;

bool otaStarted = false;
bool timeConfigured = false;

bool pendingWiFiRestoredLog = false;
bool startupLogPending = true;
bool pumpWasRunningAtBoot = false;

uint32_t lastWiFiReconnectAttempt = 0;
uint32_t lastMqttReconnectAttempt = 0;
uint32_t lastMqttPublish = 0;

// ============================================================
// WATCHDOG
// ============================================================

bool watchdogSubscribed = false;

// ============================================================
// SŪKNIS
// ============================================================

bool pumpRunning = false;

// Absolūtais viena laistīšanas seansa sākums.
uint32_t pumpSessionStartMs = 0;

// Plānotais kopējais seansa ilgums no pumpSessionStartMs.
// Pagarinot laistīšanu, palielinām šo vērtību,
// bet nekad virs MAX_PUMP_SECONDS.
uint32_t pumpPlannedDurationMs = 0;

bool notifyPumpDone = false;

uint32_t lastRunSeconds = 0;

unsigned long totalWaterings = 0;
unsigned long totalPumpSeconds = 0;

time_t lastWateringTime = 0;

// ============================================================
// MQTT IENĀKOŠO KOMANDU RINDA
// ============================================================

constexpr uint8_t COMMAND_QUEUE_SIZE = 8;

struct PendingCommand {
  bool fromHA = false;
  String payload;
};

PendingCommand commandQueue[COMMAND_QUEUE_SIZE];

uint8_t commandQueueHead = 0;
uint8_t commandQueueTail = 0;
uint8_t commandQueueCount = 0;

// ============================================================
// TELEGRAM IZEJOŠO ZIŅU RINDA
// ============================================================

// Ja MQTT īslaicīgi pazūd tieši sūkņa izslēgšanās brīdī,
// Telegram paziņojumu nezaudējam.
// Saglabājam un nosūtām pēc MQTT atjaunošanās.

constexpr uint8_t TELEGRAM_QUEUE_SIZE = 8;

String telegramQueue[TELEGRAM_QUEUE_SIZE];

uint8_t telegramQueueHead = 0;
uint8_t telegramQueueTail = 0;
uint8_t telegramQueueCount = 0;

// ============================================================
// PALĪGFUNKCIJAS
// ============================================================

String getTimeString() {
  time_t now = time(nullptr);

  // NTP vēl nav sinhronizējies.
  if (now < 1700000000) {
    return "nezināms laiks";
  }

  struct tm timeinfo;
  localtime_r(&now, &timeinfo);

  char buf[32];

  strftime(
    buf,
    sizeof(buf),
    "%d.%m.%Y %H:%M:%S",
    &timeinfo
  );

  return String(buf);
}

String getUptimeString() {
  uint64_t uptimeSeconds =
      static_cast<uint64_t>(esp_timer_get_time()) / 1000000ULL;

  uint64_t days = uptimeSeconds / 86400ULL;
  uint64_t hours = (uptimeSeconds % 86400ULL) / 3600ULL;
  uint64_t minutes = (uptimeSeconds % 3600ULL) / 60ULL;

  char buf[64];

  if (days > 0) {
    snprintf(
      buf,
      sizeof(buf),
      "%llu d %llu h %llu min",
      static_cast<unsigned long long>(days),
      static_cast<unsigned long long>(hours),
      static_cast<unsigned long long>(minutes)
    );
  } else if (hours > 0) {
    snprintf(
      buf,
      sizeof(buf),
      "%llu h %llu min",
      static_cast<unsigned long long>(hours),
      static_cast<unsigned long long>(minutes)
    );
  } else {
    snprintf(
      buf,
      sizeof(buf),
      "%llu min",
      static_cast<unsigned long long>(minutes)
    );
  }

  return String(buf);
}

String resetReasonStr() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:
      return "Strāvas ieslēgšana";

    case ESP_RST_SW:
      return "Programmatūras restarts";

    case ESP_RST_PANIC:
      return "PANIC — kods avarēja";

    case ESP_RST_INT_WDT:
      return "Interrupt watchdog";

    case ESP_RST_TASK_WDT:
      return "TASK WATCHDOG — kods iesprūda";

    case ESP_RST_WDT:
      return "Cits watchdog";

    case ESP_RST_BROWNOUT:
      return "BROWNOUT — sprieguma kritums";

    case ESP_RST_DEEPSLEEP:
      return "Deep sleep";

    case ESP_RST_EXT:
      return "Ārējais reset";

    default:
      return "Nezināms";
  }
}

void sendSyslog(const String& msg) {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  String packet = "<134>balkons-esp32 " + msg;

  udp.beginPacket(
    SYSLOG_SERVER,
    SYSLOG_PORT
  );

  udp.print(packet);
  udp.endPacket();
}

// ============================================================
// WATCHDOG
// ============================================================

void setupWatchdog() {

#if ESP_ARDUINO_VERSION_MAJOR >= 3

  esp_task_wdt_config_t wdtConfig = {
    .timeout_ms = WDT_TIMEOUT_S * 1000UL,
    .idle_core_mask = 0,
    .trigger_panic = true
  };

  // Noskaidrojam pašreizējo TWDT stāvokli.
  esp_err_t status = esp_task_wdt_status(nullptr);

  esp_err_t result;

  if (status == ESP_ERR_INVALID_STATE) {

    // TWDT vēl nav inicializēts.
    result = esp_task_wdt_init(&wdtConfig);

  } else {

    // TWDT jau darbojas.
    result = esp_task_wdt_reconfigure(&wdtConfig);
  }

  if (result != ESP_OK) {
    Serial.printf(
      "WDT konfigurācijas kļūda: %s\n",
      esp_err_to_name(result)
    );

    watchdogSubscribed = false;
    return;
  }

  // Pārbaudām, vai pašreizējais loop task jau ir abonēts.
  status = esp_task_wdt_status(nullptr);

  if (status == ESP_OK) {

    watchdogSubscribed = true;

  } else {

    result = esp_task_wdt_add(nullptr);

    if (result == ESP_OK) {
      watchdogSubscribed = true;
    } else {
      watchdogSubscribed = false;

      Serial.printf(
        "WDT task abonēšanas kļūda: %s\n",
        esp_err_to_name(result)
      );
    }
  }

#else

  esp_err_t result =
      esp_task_wdt_init(WDT_TIMEOUT_S, true);

  // Dažās vecākās ESP-IDF versijās WDT var būt
  // jau inicializēts.
  if (
      result != ESP_OK &&
      result != ESP_ERR_INVALID_STATE
  ) {
    Serial.printf(
      "WDT init kļūda: %s\n",
      esp_err_to_name(result)
    );

    watchdogSubscribed = false;
    return;
  }

  result = esp_task_wdt_add(nullptr);

  if (result == ESP_OK) {
    watchdogSubscribed = true;
  } else {
    Serial.printf(
      "WDT task abonēšanas kļūda: %s\n",
      esp_err_to_name(result)
    );

    watchdogSubscribed = false;
  }

#endif

  if (watchdogSubscribed) {
    Serial.println("Watchdog aktīvs");
  }
}

void feedWatchdog() {
  if (watchdogSubscribed) {
    esp_task_wdt_reset();
  }
}

void disableWatchdogForOTA() {
  if (!watchdogSubscribed) {
    return;
  }

  esp_task_wdt_delete(nullptr);
  watchdogSubscribed = false;
}

// ============================================================
// TELEGRAM ZIŅU RINDA
// ============================================================

void enqueueTelegramMessage(const String& msg) {

  // Ja rinda pilna, izmetam vecāko ziņu,
  // lai jaunākā netiktu pazaudēta.
  if (telegramQueueCount >= TELEGRAM_QUEUE_SIZE) {

    telegramQueue[telegramQueueHead] = "";

    telegramQueueHead =
        (telegramQueueHead + 1) %
        TELEGRAM_QUEUE_SIZE;

    telegramQueueCount--;

    Serial.println(
      "BRĪDINĀJUMS: Telegram rinda pilna, "
      "vecākā ziņa izmesta"
    );
  }

  telegramQueue[telegramQueueTail] = msg;

  telegramQueueTail =
      (telegramQueueTail + 1) %
      TELEGRAM_QUEUE_SIZE;

  telegramQueueCount++;
}

bool publishTelegramNow(const String& msg) {

  if (!mqtt.connected()) {
    return false;
  }

  return mqtt.publish(
    T_OUT,
    msg.c_str()
  );
}

void tgSend(const String& msg) {

  if (publishTelegramNow(msg)) {
    return;
  }

  enqueueTelegramMessage(msg);
}

void flushTelegramQueue() {

  while (
      mqtt.connected() &&
      telegramQueueCount > 0
  ) {

    String msg =
        telegramQueue[telegramQueueHead];

    if (!publishTelegramNow(msg)) {
      break;
    }

    telegramQueue[telegramQueueHead] = "";

    telegramQueueHead =
        (telegramQueueHead + 1) %
        TELEGRAM_QUEUE_SIZE;

    telegramQueueCount--;
  }
}

// ============================================================
// LOGI
// ============================================================

void logEvent(const String& msg) {

  String line =
      getTimeString() +
      " | " +
      msg;

  Serial.println(
    "LOG: " +
    line
  );

  if (mqtt.connected()) {
    mqtt.publish(
      T_LOG,
      line.c_str()
    );
  }

  sendSyslog(msg);
}

// ============================================================
// SŪKŅA VADĪBA
// ============================================================

void publishPumpStatus() {

  if (!mqtt.connected()) {
    return;
  }

  mqtt.publish(
    T_PUMP_ST,
    pumpRunning ? "ON" : "OFF",
    true
  );
}

bool startPump(uint32_t seconds) {

  if (pumpRunning) {
    return false;
  }

  if (seconds == 0) {
    seconds = DEFAULT_PUMP_SECONDS;
  }

  if (seconds > MAX_PUMP_SECONDS) {
    seconds = MAX_PUMP_SECONDS;
  }

  uint32_t now = millis();

  digitalWrite(
    RELAY_PIN,
    RELAY_ON
  );

  pumpRunning = true;

  pumpSessionStartMs = now;

  pumpPlannedDurationMs =
      seconds * 1000UL;

  totalWaterings++;

  lastWateringTime =
      time(nullptr);

  rtcPumpWasRunning = true;

  logEvent(
    "Sūknis IESLĒGTS uz " +
    String(seconds) +
    " sek"
  );

  publishPumpStatus();

  tgSend(
    "💧 Laistīšana sākta! (" +
    String(seconds) +
    " sek)"
  );

  return true;
}

uint32_t extendPump(uint32_t additionalSeconds) {

  if (!pumpRunning) {
    return 0;
  }

  uint32_t maxDurationMs =
      MAX_PUMP_SECONDS * 1000UL;

  uint32_t requestedAddMs =
      additionalSeconds * 1000UL;

  uint32_t oldDurationMs =
      pumpPlannedDurationMs;

  uint64_t requestedDuration =
      static_cast<uint64_t>(
        pumpPlannedDurationMs
      ) +
      requestedAddMs;

  if (
      requestedDuration >
      maxDurationMs
  ) {
    pumpPlannedDurationMs =
        maxDurationMs;
  } else {
    pumpPlannedDurationMs =
        static_cast<uint32_t>(
          requestedDuration
        );
  }

  uint32_t actuallyAddedMs =
      pumpPlannedDurationMs -
      oldDurationMs;

  if (actuallyAddedMs > 0) {

    logEvent(
      "Sūkņa laiks PAGARINĀTS par " +
      String(actuallyAddedMs / 1000UL) +
      " sek; kopējais limits: " +
      String(pumpPlannedDurationMs / 1000UL) +
      " sek"
    );
  }

  return actuallyAddedMs / 1000UL;
}

void stopPump(
  const String& reason = "manuāli"
) {

  // Fiziski izslēdzam releju vienmēr,
  // pat ja programmatūra domā, ka sūknis jau OFF.
  digitalWrite(
    RELAY_PIN,
    RELAY_OFF
  );

  if (pumpRunning) {

    uint32_t elapsedMs =
        millis() -
        pumpSessionStartMs;

    lastRunSeconds =
        elapsedMs / 1000UL;

    totalPumpSeconds +=
        lastRunSeconds;

    notifyPumpDone = true;
  }

  pumpRunning = false;

  pumpPlannedDurationMs = 0;

  rtcPumpWasRunning = false;

  logEvent(
    "Sūknis IZSLĒGTS — " +
    reason
  );

  publishPumpStatus();
}

void servicePump() {

  if (!pumpRunning) {
    return;
  }

  uint32_t elapsedMs =
      millis() -
      pumpSessionStartMs;

  constexpr uint32_t hardLimitMs =
      MAX_PUMP_SECONDS * 1000UL;

  // Absolūtais drošības limits.
  // To nevar pagarināt ar komandām.
  if (elapsedMs >= hardLimitMs) {

    stopPump(
      "sasniegts maksimālais " +
      String(MAX_PUMP_SECONDS) +
      " sek limits"
    );

    return;
  }

  // Parastais ieplānotais izslēgšanas laiks.
  if (
      pumpPlannedDurationMs > 0 &&
      elapsedMs >= pumpPlannedDurationMs
  ) {

    stopPump(
      "plānotais laiks beidzies"
    );
  }
}

// ============================================================
// OTA
// ============================================================

void setupOTA() {

  if (otaStarted) {
    return;
  }

  if (
      WiFi.status() !=
      WL_CONNECTED
  ) {
    return;
  }

  ArduinoOTA.setHostname(
    OTA_HOSTNAME
  );

  ArduinoOTA.setPassword(
    OTA_PASSWORD
  );

  ArduinoOTA.onStart([]() {

    // OTA laikā sūknis obligāti OFF.
    digitalWrite(
      RELAY_PIN,
      RELAY_OFF
    );

    pumpRunning = false;

    pumpPlannedDurationMs = 0;

    rtcPumpWasRunning = false;

    disableWatchdogForOTA();

    Serial.println(
      "OTA sākas — "
      "sūknis izslēgts, "
      "WDT atslēgts"
    );
  });

  ArduinoOTA.onEnd([]() {

    Serial.println(
      "\nOTA pabeigts — "
      "gaidu restartu"
    );
  });

  ArduinoOTA.onError(
    [](ota_error_t error) {

      Serial.println(
        "OTA kļūda #" +
        String(error)
      );

      // WDT OTA laikā tika noņemts.
      // Drošākais stāvoklis pēc neveiksmīga OTA
      // ir tīrs restarts ar releju OFF.
      digitalWrite(
        RELAY_PIN,
        RELAY_OFF
      );

      delay(250);

      ESP.restart();
    }
  );

  ArduinoOTA.begin();

  otaStarted = true;

  Serial.println(
    "OTA gatavs (" +
    WiFi.localIP().toString() +
    ")"
  );
}

// ============================================================
// LAIKA SINHRONIZĀCIJA
// ============================================================

void setupTimeOnce() {

  if (timeConfigured) {
    return;
  }

  // configTime(0, 0, ...) pārraksta TZ uz UTC.
  // Izmantojam configTzTime(), lai CET/CEST pārejas noteikumus
  // piemērotu arī pēc SNTP inicializācijas.
  configTzTime(
    TZ_INFO,
    NTP_SERVER_1,
    NTP_SERVER_2,
    NTP_SERVER_3
  );

  timeConfigured = true;

  Serial.println(
    "NTP konfigurēts"
  );
}

// ============================================================
// Wi-Fi
// ============================================================

void setupWiFi() {

  WiFi.mode(
    WIFI_STA
  );

  WiFi.persistent(
    false
  );

  WiFi.setAutoReconnect(
    true
  );

  // Ierīce darbojas no pastāvīgas barošanas.
  // Atslēdzam Wi-Fi sleep, lai samazinātu
  // latentumu un nejaušus MQTT timeout.
  WiFi.setSleep(
    false
  );

  WiFi.setHostname(
    OTA_HOSTNAME
  );

  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD
  );

  lastWiFiReconnectAttempt =
      millis();

  Serial.println(
    "WiFi pieslēgšanās sākta..."
  );
}

void serviceWiFi() {

  bool connected =
      WiFi.status() ==
      WL_CONNECTED;

  if (connected) {

    if (!wifiOnline) {

      wifiOnline = true;

      Serial.println(
        "WiFi savienots! IP: " +
        WiFi.localIP().toString()
      );

      if (wifiEverConnected) {
        pendingWiFiRestoredLog = true;
      }

      wifiEverConnected = true;

      setupTimeOnce();
      setupOTA();
    }

    return;
  }

  // Tikko zaudējām Wi-Fi.
  if (wifiOnline) {

    wifiOnline = false;

    mqttNet.stop();

    Serial.println(
      "WiFi savienojums pazudis"
    );
  }

  uint32_t now =
      millis();

  if (
      now -
      lastWiFiReconnectAttempt >=
      WIFI_RECONNECT_INTERVAL_MS
  ) {

    lastWiFiReconnectAttempt =
        now;

    Serial.println(
      "Mēģinu atjaunot WiFi..."
    );

    // Nebloķējam loop().
    // ESP32 savienošanos turpina fonā.
    WiFi.reconnect();
  }
}

// ============================================================
// SENSORI
// ============================================================

void selectSensor(
  int channel
) {

  digitalWrite(
    MUX_S0,
    (channel >> 0) & 1
  );

  digitalWrite(
    MUX_S1,
    (channel >> 1) & 1
  );

  digitalWrite(
    MUX_S2,
    (channel >> 2) & 1
  );

  digitalWrite(
    MUX_S3,
    (channel >> 3) & 1
  );

  // Ļaujam MUX signālam stabilizēties.
  delay(10);

  // Pirmo ADC mērījumu pēc kanāla maiņas izmetam.
  analogRead(MUX_SIG);

  delay(5);
}

// Atgriež:
// -1 = sensors nav uzskatāms par pievienotu/stabilu
// citādi = RAW ADC vidējā vērtība.
int readMoistureRaw(
  int sensorIndex
) {

  selectSensor(
    sensorIndex
  );

  constexpr int SAMPLES = 10;

  int minVal = 4095;
  int maxVal = 0;

  long sum = 0;

  for (
      int i = 0;
      i < SAMPLES;
      i++
  ) {

    int value =
        analogRead(
          MUX_SIG
        );

    if (value < minVal) {
      minVal = value;
    }

    if (value > maxVal) {
      maxVal = value;
    }

    sum += value;

    delay(2);
  }

  int average =
      sum /
      SAMPLES;

  int spread =
      maxVal -
      minVal;

  // Ļoti nestabils signāls.
  if (spread > 200) {
    return -1;
  }

  // Praktiski ADC robeža.
  if (
      average < 100 ||
      average > 4090
  ) {
    return -1;
  }

  return average;
}

String categorizeMoisture(
  int raw
) {

  if (raw == -1) {
    return "";
  }

  if (
      raw >
      MOISTURE_DRY_THRESHOLD
  ) {
    return "sauss";
  }

  if (
      raw <
      MOISTURE_WET_THRESHOLD
  ) {
    return "mitrs";
  }

  return "videjs";
}

String categoryEmoji(
  const String& category
) {

  if (category == "sauss") {
    return "🔴";
  }

  if (category == "videjs") {
    return "🟡";
  }

  if (category == "mitrs") {
    return "🔵";
  }

  return "⚪";
}

// ============================================================
// MQTT KOMANDU RINDA
// ============================================================

void enqueueCommand(
  bool fromHA,
  const String& payload
) {

  // Ja rinda pilna, izmetam vecāko komandu.
  // Tas dod priekšroku jaunākajām komandām,
  // piemēram STOP.
  if (
      commandQueueCount >=
      COMMAND_QUEUE_SIZE
  ) {

    commandQueue[
      commandQueueHead
    ].payload = "";

    commandQueueHead =
        (commandQueueHead + 1) %
        COMMAND_QUEUE_SIZE;

    commandQueueCount--;

    Serial.println(
      "BRĪDINĀJUMS: komandu rinda pilna, "
      "vecākā komanda izmesta"
    );
  }

  commandQueue[
    commandQueueTail
  ].fromHA = fromHA;

  commandQueue[
    commandQueueTail
  ].payload = payload;

  commandQueueTail =
      (commandQueueTail + 1) %
      COMMAND_QUEUE_SIZE;

  commandQueueCount++;
}

bool dequeueCommand(
  PendingCommand& out
) {

  if (
      commandQueueCount == 0
  ) {
    return false;
  }

  out.fromHA =
      commandQueue[
        commandQueueHead
      ].fromHA;

  out.payload =
      commandQueue[
        commandQueueHead
      ].payload;

  commandQueue[
    commandQueueHead
  ].payload = "";

  commandQueueHead =
      (commandQueueHead + 1) %
      COMMAND_QUEUE_SIZE;

  commandQueueCount--;

  return true;
}

// ============================================================
// HOME ASSISTANT DISCOVERY
// ============================================================

void sendDiscovery() {

  char topic[96];
  char payload[600];

  for (
      int sensor = 0;
      sensor < SENSOR_COUNT;
      sensor++
  ) {

    snprintf(
      topic,
      sizeof(topic),
      "homeassistant/sensor/"
      "balkons_puke%d/config",
      sensor + 1
    );

    snprintf(
      payload,
      sizeof(payload),

      "{"
      "\"name\":\"Puķe %d\","
      "\"stat_t\":\"balkons/puke%d/mitrums\","
      "\"exp_aft\":180,"
      "\"icon\":\"mdi:flower\","
      "\"uniq_id\":\"balkons_puke%d\","
      "\"avty_t\":\"balkons/status\","
      "\"dev\":{"
        "\"ids\":[\"balkons_esp32\"],"
        "\"name\":\"Balkona Laistīšana\","
        "\"mf\":\"Andris\","
        "\"mdl\":\"ESP32\""
      "}"
      "}",

      sensor + 1,
      sensor + 1,
      sensor + 1
    );

    mqtt.publish(
      topic,
      payload,
      true
    );

    feedWatchdog();

    delay(10);
  }

  snprintf(
    topic,
    sizeof(topic),
    "homeassistant/switch/"
    "balkons_sukna/config"
  );

  snprintf(
    payload,
    sizeof(payload),

    "{"
    "\"name\":\"Sūknis\","
    "\"cmd_t\":\"balkons/sukna/komanda\","
    "\"stat_t\":\"balkons/sukna/stends\","
    "\"pl_on\":\"ON\","
    "\"pl_off\":\"OFF\","
    "\"icon\":\"mdi:water-pump\","
    "\"uniq_id\":\"balkons_sukna\","
    "\"avty_t\":\"balkons/status\","
    "\"dev\":{"
      "\"ids\":[\"balkons_esp32\"],"
      "\"name\":\"Balkona Laistīšana\","
      "\"mf\":\"Andris\","
      "\"mdl\":\"ESP32\""
    "}"
    "}"
  );

  mqtt.publish(
    topic,
    payload,
    true
  );

  Serial.println(
    "MQTT discovery nosūtīts"
  );
}

// ============================================================
// MQTT
// ============================================================

void mqttCallback(
  char* topic,
  byte* payload,
  unsigned int length
) {

  String message;

  message.reserve(
    length
  );

  for (
      unsigned int i = 0;
      i < length;
      i++
  ) {
    message +=
        static_cast<char>(
          payload[i]
        );
  }

  String topicString(
    topic
  );

  if (
      topicString ==
      T_PUMP_CMD
  ) {

    enqueueCommand(
      true,
      message
    );

  } else if (
      topicString ==
      T_CMD
  ) {

    enqueueCommand(
      false,
      message
    );
  }
}

void handleDeferredSystemLogs() {

  if (startupLogPending) {

    startupLogPending = false;

    logEvent(
      "Sistēma startēja — "
      "restarta iemesls: " +
      resetReasonStr() +
      ", brīvā atmiņa: " +
      String(
        ESP.getFreeHeap() /
        1024
      ) +
      " KB, firmware: " +
      String(FIRMWARE_GIT_REV)
    );

    if (pumpWasRunningAtBoot) {

      tgSend(
        "⚠️ Sistēma restartējās "
        "laistīšanas laikā. "
        "Sūknis TAGAD ir izslēgts. "
        "Pārbaudi manuāli, ja šaubies."
      );

      pumpWasRunningAtBoot = false;
    }
  }

  if (pendingWiFiRestoredLog) {

    pendingWiFiRestoredLog = false;

    logEvent(
      "WiFi ATJAUNOTS pēc pazušanas, "
      "IP: " +
      WiFi.localIP().toString() +
      ", signāls: " +
      String(
        WiFi.RSSI()
      ) +
      " dBm"
    );
  }
}

void connectMQTT() {

  if (
      WiFi.status() !=
      WL_CONNECTED
  ) {
    return;
  }

  if (
      mqtt.connected()
  ) {
    return;
  }

  Serial.println(
    "Mēģinu pieslēgt MQTT..."
  );

  bool connected =
      mqtt.connect(
        mqttClientId,
        MQTT_USERNAME,
        MQTT_PASSWORD,
        T_STATUS,
        0,
        true,
        "offline"
      );

  if (connected) {

    Serial.println(
      "MQTT savienots!"
    );

    mqtt.publish(
      T_STATUS,
      "online",
      true
    );

    // PubSubClient var abonēt ar QoS 1.
    mqtt.subscribe(
      T_PUMP_CMD,
      1
    );

    mqtt.subscribe(
      T_CMD,
      1
    );

    sendDiscovery();

    publishPumpStatus();

    logEvent(
      "MQTT savienots"
    );

    // Pēc reconnect uzreiz ļaujam
    // nosūtīt aktuālos sensoru datus.
    lastMqttPublish =
        millis() -
        MQTT_PUBLISH_INTERVAL_MS;

    handleDeferredSystemLogs();

    // Nosūtām Telegram ziņas,
    // kas gaidīja MQTT atjaunošanos.
    flushTelegramQueue();

  } else {

    static uint32_t lastFailLog = 0;

    int state =
        mqtt.state();

    Serial.println(
      "MQTT neizdevās, rc=" +
      String(state)
    );

    if (
        millis() -
        lastFailLog >=
        60000UL
    ) {

      lastFailLog =
          millis();

      // MQTT pats nav pieejams,
      // bet syslog caur UDP vēl var strādāt.
      logEvent(
        "MQTT savienojums NEIZDEVĀS, rc=" +
        String(state)
      );
    }
  }
}

void serviceMQTT() {

  if (
      WiFi.status() !=
      WL_CONNECTED
  ) {
    return;
  }

  if (
      mqtt.connected()
  ) {

    mqtt.loop();

    return;
  }

  uint32_t now =
      millis();

  if (
      now -
      lastMqttReconnectAttempt >=
      MQTT_RECONNECT_INTERVAL_MS
  ) {

    lastMqttReconnectAttempt =
        now;

    connectMQTT();
  }
}

// ============================================================
// MQTT MITRUMA PUBLICĒŠANA
// ============================================================

void publishMoisture() {

  if (
      !mqtt.connected()
  ) {
    return;
  }

  char topic[48];

  for (
      int sensor = 0;
      sensor < SENSOR_COUNT;
      sensor++
  ) {

    int raw =
        readMoistureRaw(
          sensor
        );

    feedWatchdog();

    String category =
        categorizeMoisture(
          raw
        );

    if (
        category.length() ==
        0
    ) {
      continue;
    }

    snprintf(
      topic,
      sizeof(topic),
      "balkons/puke%d/mitrums",
      sensor + 1
    );

    mqtt.publish(
      topic,
      category.c_str()
    );

    // Sensora lasīšanas laikā arī pārbaudām
    // sūkņa lokālo taimeri.
    servicePump();
  }

  Serial.println(
    "MQTT mitrums nosūtīts"
  );
}

// ============================================================
// KOMANDAS
// ============================================================

void processHACommand(
  String command
) {

  command.trim();

  if (
      command ==
      "ON"
  ) {

    if (!pumpRunning) {

      startPump(
        MAX_PUMP_SECONDS
      );
    }

  } else if (
      command ==
      "OFF"
  ) {

    if (pumpRunning) {

      stopPump(
        "Home Assistant OFF"
      );
    }
  }
}

void processCommand(
  String command
) {

  command.trim();

  logEvent(
    "Komanda (MQTT): " +
    command
  );

  // ----------------------------------------------------------
  // /laist
  // ----------------------------------------------------------

  if (
      command ==
      "laist"
  ) {

    if (pumpRunning) {

      uint32_t added =
          extendPump(
            DEFAULT_PUMP_SECONDS
          );

      if (added > 0) {

        tgSend(
          "💧 Sūknis jau darbojas — "
          "pagarinu par " +
          String(added) +
          " sekundēm!"
        );

      } else {

        tgSend(
          "⚠️ Sūknim jau sasniegts "
          "maksimālais " +
          String(
            MAX_PUMP_SECONDS
          ) +
          " sekunžu limits."
        );
      }

    } else {

      startPump(
        DEFAULT_PUMP_SECONDS
      );
    }
  }

  // ----------------------------------------------------------
  // /laist_X
  // ----------------------------------------------------------

  else if (
      command.startsWith(
        "laist_"
      )
  ) {

    int minutes =
        command
          .substring(6)
          .toInt();

    int maxMinutes =
        MAX_PUMP_SECONDS /
        60;

    if (
        minutes > 0 &&
        minutes <= maxMinutes
    ) {

      uint32_t requestedSeconds =
          static_cast<uint32_t>(
            minutes
          ) *
          60UL;

      if (pumpRunning) {

        uint32_t added =
            extendPump(
              requestedSeconds
            );

        if (added > 0) {

          tgSend(
            "💧 Laistīšana pagarināta par " +
            String(added) +
            " sek!"
          );

        } else {

          tgSend(
            "⚠️ Jau sasniegts maksimālais " +
            String(
              MAX_PUMP_SECONDS
            ) +
            " sekunžu limits."
          );
        }

      } else {

        startPump(
          requestedSeconds
        );
      }

    } else {

      tgSend(
        "❌ Ievadi 1-" +
        String(
          maxMinutes
        ) +
        " minūtes"
      );
    }
  }

  // ----------------------------------------------------------
  // /stop
  // ----------------------------------------------------------

  else if (
      command ==
      "stop"
  ) {

    if (pumpRunning) {

      stopPump(
        "manuāla STOP komanda"
      );

    } else {

      tgSend(
        "Sūknis jau bija izslēgts."
      );
    }
  }

  // ----------------------------------------------------------
  // /mitrums
  // ----------------------------------------------------------

  else if (
      command ==
      "mitrums"
  ) {

    String result =
        "🌱 Mitruma stāvoklis:\n\n";

    result.reserve(
      600
    );

    int activeCount = 0;

    for (
        int sensor = 0;
        sensor < SENSOR_COUNT;
        sensor++
    ) {

      int raw =
          readMoistureRaw(
            sensor
          );

      feedWatchdog();

      servicePump();

      String category =
          categorizeMoisture(
            raw
          );

      if (
          category.length() ==
          0
      ) {

        result +=
            "Puķe " +
            String(
              sensor + 1
            ) +
            ": nav pievienots\n";

      } else {

        result +=
            categoryEmoji(
              category
            ) +
            " Puķe " +
            String(
              sensor + 1
            ) +
            ": " +
            category +
            "\n";

        activeCount++;
      }
    }

    result +=
        "\nAktīvi sensori: " +
        String(
          activeCount
        ) +
        "/" +
        String(
          SENSOR_COUNT
        );

    tgSend(
      result
    );
  }

  // ----------------------------------------------------------
  // /raw
  // ----------------------------------------------------------

  else if (
      command ==
      "raw"
  ) {

    String result =
        "🔧 RAW ADC vērtības:\n\n";

    result.reserve(
      500
    );

    for (
        int sensor = 0;
        sensor < SENSOR_COUNT;
        sensor++
    ) {

      selectSensor(
        sensor
      );

      long sum = 0;

      for (
          int i = 0;
          i < 10;
          i++
      ) {

        sum +=
            analogRead(
              MUX_SIG
            );

        delay(2);
      }

      int average =
          sum /
          10;

      feedWatchdog();

      servicePump();

      result +=
          "Puķe " +
          String(
            sensor + 1
          ) +
          ": " +
          String(
            average
          ) +
          "\n";
    }

    tgSend(
      result
    );
  }

  // ----------------------------------------------------------
  // /statuss
  // ----------------------------------------------------------

  else if (
      command ==
      "statuss"
  ) {

    String status =
        "⚙️ Statuss:\n";

    status.reserve(
      480
    );

    status +=
        "Sūknis: " +
        String(
          pumpRunning ?
          "🟢 Darbojas" :
          "🔴 Izslēgts"
        ) +
        "\n";

    if (pumpRunning) {

      uint32_t elapsedMs =
          millis() -
          pumpSessionStartMs;

      uint32_t remainingMs = 0;

      if (
          pumpPlannedDurationMs >
          elapsedMs
      ) {

        remainingMs =
            pumpPlannedDurationMs -
            elapsedMs;
      }

      // Noapaļojam uz augšu,
      // lai 14999 ms rādītu 15 sek.
      uint32_t remainingSeconds =
          (
            remainingMs +
            999UL
          ) /
          1000UL;

      status +=
          "Atlikušais laiks: " +
          String(
            remainingSeconds
          ) +
          " sek\n";
    }

    if (
        WiFi.status() ==
        WL_CONNECTED
    ) {

      status +=
          "WiFi: " +
          WiFi.localIP().toString() +
          "\n";

      status +=
          "Signāls: " +
          String(
            WiFi.RSSI()
          ) +
          " dBm\n";

    } else {

      status +=
          "WiFi: 🔴 atvienots\n";
    }

    status +=
        "MQTT: " +
        String(
          mqtt.connected() ?
          "🟢 savienots" :
          "🔴 atvienots"
        ) +
        "\n";

    status +=
        "Brīvā atmiņa: " +
        String(
          ESP.getFreeHeap() /
          1024
        ) +
        " KB\n";

    status +=
        "Laiks: " +
        getTimeString() +
        "\n";

    status +=
        "Firmware: " +
        String(FIRMWARE_GIT_REV) +
        "\n";

    status +=
        "Uptime: " +
        getUptimeString();

    tgSend(
      status
    );
  }

  // ----------------------------------------------------------
  // /statistika
  // ----------------------------------------------------------

  else if (
      command ==
      "statistika"
  ) {

    String stats =
        "📊 Statistika:\n\n";

    stats +=
        "Laistīšanas reizes: " +
        String(
          totalWaterings
        ) +
        "\n";

    stats +=
        "Kopējais sūkņa laiks: " +
        String(
          totalPumpSeconds
        ) +
        " sek\n";

    if (
        lastWateringTime >
        1700000000
    ) {

      struct tm timeinfo;

      localtime_r(
        &lastWateringTime,
        &timeinfo
      );

      char buf[32];

      strftime(
        buf,
        sizeof(buf),
        "%d.%m.%Y %H:%M:%S",
        &timeinfo
      );

      stats +=
          "Pēdējā laistīšana: " +
          String(buf);
    }

    tgSend(
      stats
    );
  }

  // ----------------------------------------------------------
  // Nezināma komanda
  // ----------------------------------------------------------

  else {

    tgSend(
      "❓ Nezināma komanda: " +
      command
    );
  }
}

void processCommandQueue() {

  PendingCommand item;

  // Vienā loop ciklā apstrādājam vienu komandu.
  // Tas neļauj lielai komandbumbai pārāk ilgi
  // aizturēt tīkla un sūkņa drošības funkcijas.
  if (
      !dequeueCommand(
        item
      )
  ) {
    return;
  }

  if (item.fromHA) {

    processHACommand(
      item.payload
    );

  } else {

    processCommand(
      item.payload
    );
  }
}

// ============================================================
// SETUP
// ============================================================

void setup() {

  Serial.begin(
    115200
  );

  delay(100);

  // ----------------------------------------------------------
  // Relejs VISPIRMS droši OFF.
  // ----------------------------------------------------------

  pinMode(
    RELAY_PIN,
    OUTPUT
  );

  digitalWrite(
    RELAY_PIN,
    RELAY_OFF
  );

  // Saglabājam informāciju par iepriekšējo restartu,
  // bet uzreiz notīrām RTC flagu, jo sūknis jau ir OFF.
  pumpWasRunningAtBoot =
      rtcPumpWasRunning;

  rtcPumpWasRunning =
      false;

  // ----------------------------------------------------------
  // MUX
  // ----------------------------------------------------------

  pinMode(
    MUX_S0,
    OUTPUT
  );

  pinMode(
    MUX_S1,
    OUTPUT
  );

  pinMode(
    MUX_S2,
    OUTPUT
  );

  pinMode(
    MUX_S3,
    OUTPUT
  );

  pinMode(
    MUX_SIG,
    INPUT
  );

  // Explicit 12-bit ADC.
  // Tavi pašreizējie sliekšņi ir kalibrēti 0-4095 diapazonam.
  analogReadResolution(
    12
  );

  // ----------------------------------------------------------
  // Watchdog
  // ----------------------------------------------------------

  setupWatchdog();

  // ----------------------------------------------------------
  // Unikāls MQTT Client ID
  // ----------------------------------------------------------

  uint64_t chipId =
      ESP.getEfuseMac();

  snprintf(
    mqttClientId,
    sizeof(mqttClientId),
    "%s-%06llX",
    MQTT_CLIENT_ID_BASE,
    static_cast<unsigned long long>(
      chipId &
      0xFFFFFFULL
    )
  );

  Serial.println(
    "MQTT Client ID: " +
    String(
      mqttClientId
    )
  );

  // ----------------------------------------------------------
  // MQTT
  // ----------------------------------------------------------

  mqtt.setServer(
    MQTT_SERVER,
    MQTT_PORT
  );

  mqtt.setBufferSize(
    MQTT_BUFFER_SIZE
  );

  mqtt.setKeepAlive(
    MQTT_KEEPALIVE_S
  );

  mqtt.setSocketTimeout(
    MQTT_SOCKET_TIMEOUT_S
  );

  mqtt.setCallback(
    mqttCallback
  );

  // ----------------------------------------------------------
  // Wi-Fi
  // ----------------------------------------------------------

  setupWiFi();

  Serial.println(
    "Sistēma startēta. "
    "Gaidu WiFi/MQTT... 🌱"
  );
}

// ============================================================
// LOOP
// ============================================================

void loop() {

  feedWatchdog();

  // Sūkņa drošību pārbaudām pašā loop sākumā.
  servicePump();

  // ----------------------------------------------------------
  // Wi-Fi
  // ----------------------------------------------------------

  serviceWiFi();

  // ----------------------------------------------------------
  // OTA
  // ----------------------------------------------------------

  if (
      otaStarted &&
      WiFi.status() ==
      WL_CONNECTED
  ) {

    ArduinoOTA.handle();
  }

  // ----------------------------------------------------------
  // MQTT
  // ----------------------------------------------------------

  serviceMQTT();

  // Pēc iespējami bloķējoša tīkla mēģinājuma
  // vēlreiz pārbaudām sūkņa laiku.
  servicePump();

  // ----------------------------------------------------------
  // MQTT komandas
  // ----------------------------------------------------------

  processCommandQueue();

  // Komanda varēja ieslēgt vai izslēgt sūkni.
  servicePump();

  // ----------------------------------------------------------
  // Periodiska mitruma publicēšana
  // ----------------------------------------------------------

  if (
      mqtt.connected() &&
      millis() -
      lastMqttPublish >=
      MQTT_PUBLISH_INTERVAL_MS
  ) {

    lastMqttPublish =
        millis();

    publishMoisture();
  }

  // ----------------------------------------------------------
  // Pabeigtas laistīšanas paziņojums
  // ----------------------------------------------------------

  if (
      notifyPumpDone
  ) {

    notifyPumpDone =
        false;

    tgSend(
      "✅ Laistīšana pabeigta! (" +
      String(
        lastRunSeconds
      ) +
      " sek)"
    );
  }

  // ----------------------------------------------------------
  // Ja MQTT ir atgriezies, mēģinām iztukšot
  // atlikušās Telegram ziņas.
  // ----------------------------------------------------------

  if (
      mqtt.connected() &&
      telegramQueueCount > 0
  ) {

    flushTelegramQueue();
  }

  // Īss yield sistēmas taskiem.
  delay(1);
}
