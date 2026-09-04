#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "mqtt_runtime_adapter.h"
#include <cstring>
#include <ArduinoOTA.h>
#include <time.h>

#include "esp_task_wdt.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_arduino_version.h"

#include "secrets.h"
#include "command_safety.h"
#include "command_payload_policy.h"
#include "command_parse_policy.h"
#include "network_reconnect_policy.h"
#include "ota_error_policy.h"
#include "pump_timing_policy.h"

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
constexpr uint32_t MQTT_TCP_CONNECT_TIMEOUT_MS = 1000UL;

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

MqttRuntimeAdapter mqtt;
WiFiUDP udp;

enum class MqttSessionInitState : uint8_t {
  Idle = 0,
  SubscribePump,
  AwaitPumpSuback,
  SubscribeCmd,
  AwaitCmdSuback,
  Online,
  DiscoverySensors,
  DiscoveryPump,
  PumpStatus,
  ConnectedLog,
  StartupLog,
  StartupPumpWarning,
  WifiRestoredLog,
  Done,
};

constexpr uint32_t MQTT_SESSION_CRITICAL_STEP_BUDGET_MS = 6500UL;
constexpr uint32_t MQTT_DISCOVERY_WINDOW_BUDGET_MS = 6500UL;

MqttSessionInitState mqttSessionInitState = MqttSessionInitState::Idle;
uint8_t mqttDiscoverySensorIndex = 0;
uint32_t mqttSessionStepStartedAt = 0;
uint32_t mqttDiscoveryWindowStartedAt = 0;
uint32_t oversizedTelegramDrops = 0;

bool mqttSessionReady() {
  return mqttSessionInitState == MqttSessionInitState::Done;
}

char mqttClientId[48] = {0};

bool wifiOnline = false;
bool wifiEverConnected = false;

bool otaTransferStarted = false;
bool timeConfigured = false;

bool pendingWiFiRestoredLog = false;
bool startupLogPending = true;
bool pumpWasRunningAtBoot = false;

uint32_t lastWiFiReconnectAttempt = 0;
uint32_t lastMqttReconnectAttempt = 0;
uint32_t lastMqttPublish = 0;
bool moisturePublishActive = false;
uint8_t moisturePublishSensor = 0;

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
  uint32_t stopEpoch = 0;
  String payload;
};

PendingCommand commandQueue[COMMAND_QUEUE_SIZE];

uint8_t commandQueueHead = 0;
uint8_t commandQueueTail = 0;
uint8_t commandQueueCount = 0;

uint32_t oversizedCommandDrops = 0;

// Katrs urgent STOP/OFF palielina epoch. Parastās komandas saglabā
// epoch, kurā tās tika saņemtas, lai pēc jaunāka STOP varētu
// atmest tikai stale pump-start/extend komandas, nezaudējot statusa
// vai diagnostikas komandas.
uint32_t pumpStopEpoch = 0;

bool urgentPumpStopPending = false;
bool urgentPumpStopFromHA = false;

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

bool telegramPayloadAllowed(const String& msg) {
  return mqtt_runtime_policy::canStoreTrackedPayload(msg.length());
}

void dropOversizedTelegramMessage(const String& msg) {
  oversizedTelegramDrops++;

  Serial.printf(
    "BRĪDINĀJUMS: Telegram ziņa par garu (%u > %u baiti)\n",
    static_cast<unsigned int>(msg.length()),
    static_cast<unsigned int>(
      mqtt_runtime_policy::kTrackedPayloadTextMaxBytes
    )
  );
}

void tgSend(const String& msg) {

  if (!telegramPayloadAllowed(msg)) {
    dropOversizedTelegramMessage(msg);
    return;
  }

  // Adaptera fixed copy ir lokāls staging solis. Ja session init vēl nav
  // pabeigts, jauns network PUBLISH netiks sūknēts līdz Done stāvoklim.
  if (
      !mqtt.trackedPublishBusy() &&
      mqtt.startTrackedPublish(
        T_OUT,
        msg.c_str()
      )
  ) {
    return;
  }

  enqueueTelegramMessage(msg);
}

void serviceTelegramDelivery() {

  // Pinned synchronous NetworkClient write path has no hard low-latency bound.
  // Kamēr sūknis darbojas, neģenerējam jaunu application-level MQTT write darbu.
  if (pumpRunning || !mqttSessionReady()) {
    return;
  }

  if (mqtt.trackedPublishBusy()) {
    mqtt.pumpTrackedPublish();
    return;
  }

  if (telegramQueueCount == 0) {
    return;
  }

  String msg =
      telegramQueue[telegramQueueHead];

  if (!telegramPayloadAllowed(msg)) {
    dropOversizedTelegramMessage(msg);

    telegramQueue[telegramQueueHead] = "";
    telegramQueueHead =
        (telegramQueueHead + 1) %
        TELEGRAM_QUEUE_SIZE;
    telegramQueueCount--;
    return;
  }

  if (
      !mqtt.startTrackedPublish(
        T_OUT,
        msg.c_str()
      )
  ) {
    return;
  }

  // No šī brīža adapterim pieder fixed copy; app queue head drīkst atbrīvot.
  telegramQueue[telegramQueueHead] = "";
  telegramQueueHead =
      (telegramQueueHead + 1) %
      TELEGRAM_QUEUE_SIZE;
  telegramQueueCount--;

  // Vienā izsaukumā maksimums viens jauns network PUBLISH mēģinājums.
  mqtt.pumpTrackedPublish();
}

// ============================================================
// LOGI
// ============================================================

bool mqttDiagnosticPublishAllowed() {
  if (pumpRunning || !mqtt.isConnected()) {
    return false;
  }

  return mqttSessionReady() ||
         mqttSessionInitState == MqttSessionInitState::ConnectedLog ||
         mqttSessionInitState == MqttSessionInitState::StartupLog ||
         mqttSessionInitState == MqttSessionInitState::WifiRestoredLog;
}

void writeEventLog(const String& msg, bool publishMqtt) {

  String line =
      getTimeString() +
      " | " +
      msg;

  Serial.println(
    "LOG: " +
    line
  );

  if (publishMqtt && mqttDiagnosticPublishAllowed()) {
    mqtt.publishBestEffort(
      T_LOG,
      line.c_str(),
      false
    );
  }

  sendSyslog(msg);
}

void logEvent(const String& msg) {
  writeEventLog(msg, true);
}

void logCommandReceipt(const String& command) {
  // espMqttClient/NetworkClient write ir synchronous. Komandas saņemšanas
  // diagnostika nedrīkst izveidot MQTT write darbu pirms Telegram atbildes.
  writeEventLog(
    "Komanda (MQTT): " +
    command,
    false
  );
}

// ============================================================
// SŪKŅA VADĪBA
// ============================================================

bool publishPumpStatusValue(bool running) {

  if (!mqtt.isConnected()) {
    return false;
  }

  if (
      !mqttSessionReady() &&
      mqttSessionInitState != MqttSessionInitState::PumpStatus
  ) {
    return false;
  }

  return mqtt.publishBestEffort(
    T_PUMP_ST,
    running ? "ON" : "OFF",
    true
  );
}

bool publishPumpStatus() {
  return publishPumpStatusValue(pumpRunning);
}

bool startPump(uint32_t seconds) {

  // Ja STOP/OFF tikko saņemts, vispirms jāpabeidz tā stāvokļa
  // reconciliācija. Tas nepieļauj pump startu vienā un tajā pašā
  // loop logā pirms urgent stop apstrādes.
  if (urgentPumpStopPending) {
    return false;
  }

  if (pumpRunning) {
    return false;
  }

  // Synchronous espMqttClient/NetworkClient outbox tiek servēts pirms inbound.
  // Tāpēc jaunu pump session sākam tikai no Ready + lokāli tukša outbox stāvokļa.
  if (
      !mqttSessionReady() ||
      !mqtt.isConnected() ||
      mqtt.queueSize() != 0U ||
      mqtt.trackedPublishBusy()
  ) {
    return false;
  }

  seconds =
      pump_timing_policy::normalizeRequestedSeconds(
        seconds,
        DEFAULT_PUMP_SECONDS,
        MAX_PUMP_SECONDS
      );

  // Retained ON tiek mēģināts, kamēr relejs vēl fiziski OFF. Ja synchronous
  // write neatbrīvo lokālo outbox, fail-closed paliekam OFF un pārtraucam sesiju,
  // lai novecojis ON vēlāk netiktu izsūtīts pēc atteikta pump starta.
  if (!publishPumpStatusValue(true)) {
    return false;
  }

  if (urgentPumpStopPending || mqtt.queueSize() != 0U) {
    if (mqtt.queueSize() != 0U) {
      mqtt.forceDisconnect();
    }
    return false;
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

  const auto extension =
      pump_timing_policy::extendPlannedDuration(
        pumpPlannedDurationMs,
        additionalSeconds,
        MAX_PUMP_SECONDS
      );

  pumpPlannedDurationMs =
      extension.plannedDurationMs;

  const uint32_t actuallyAddedMs =
      extension.addedMs;

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

  const uint32_t now =
      millis();

  constexpr uint32_t hardLimitMs =
      MAX_PUMP_SECONDS * 1000UL;

  // Absolūtais drošības limits.
  // To nevar pagarināt ar komandām.
  if (
      pump_timing_policy::hasElapsed(
        now,
        pumpSessionStartMs,
        hardLimitMs
      )
  ) {

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
      pump_timing_policy::hasElapsed(
        now,
        pumpSessionStartMs,
        pumpPlannedDurationMs
      )
  ) {

    stopPump(
      "plānotais laiks beidzies"
    );
  }
}

// ============================================================
// URGENT SŪKŅA STOP/OFF
// ============================================================

void requestUrgentPumpStop(bool fromHA) {

  // Drošības efekts notiek uzreiz callback kontekstā: tikai GPIO OFF.
  // MQTT publish/logging šeit apzināti neveicam, lai neizraisītu
  // MQTT callback re-entrancy.
  digitalWrite(
    RELAY_PIN,
    RELAY_OFF
  );

  pumpStopEpoch++;
  urgentPumpStopPending = true;
  urgentPumpStopFromHA = fromHA;
}

void serviceUrgentPumpStop() {

  if (!urgentPumpStopPending) {
    return;
  }

  bool fromHA = urgentPumpStopFromHA;

  urgentPumpStopPending = false;
  urgentPumpStopFromHA = false;

  if (pumpRunning) {

    stopPump(
      fromHA ?
      "Home Assistant OFF (urgent)" :
      "manuāla STOP komanda (urgent)"
    );

    return;
  }

  // Pat ja programmatūras stāvoklis jau bija OFF, uzturam fizisko
  // fail-safe stāvokli un atjaunojam retained MQTT statusu.
  digitalWrite(
    RELAY_PIN,
    RELAY_OFF
  );

  publishPumpStatus();

  if (!fromHA) {
    tgSend(
      "Sūknis jau bija izslēgts."
    );
  }
}

// Garāku sensoru lasījumu laikā pieņemam MQTT inputu, bet
// neuzsākam reconnect. Tas ļauj saņemt STOP/OFF starp sensoriem
// un tūlīt pēc mqtt.loop() atgriešanās pabeigt stop reconciliāciju.
void servicePumpCriticalNetworkInput() {

  if (!mqtt.isDisconnected()) {
    mqtt.service();
  }

  serviceUrgentPumpStop();
  servicePump();
  feedWatchdog();
}

// ============================================================
// OTA
// ============================================================

void forcePumpOffForOtaSafety() {
  digitalWrite(
    RELAY_PIN,
    RELAY_OFF
  );

  // Jebkurš OTA lifecycle/error notikums ir jaunāks fail-safe OFF punkts.
  // Tas arī padara iepriekš rindā esošus pump-start pieprasījumus stale.
  pumpStopEpoch++;
  pumpRunning = false;
  pumpPlannedDurationMs = 0;
  rtcPumpWasRunning = false;
}

void setupOTA() {

  if (
      WiFi.status() !=
      WL_CONNECTED
  ) {
    return;
  }

  otaTransferStarted = false;

  ArduinoOTA.setHostname(
    OTA_HOSTNAME
  );

  ArduinoOTA.setPassword(
    OTA_PASSWORD
  );

  ArduinoOTA.onStart([]() {

    otaTransferStarted = true;

    // OTA laikā sūknis obligāti OFF.
    forcePumpOffForOtaSafety();

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

      // Relejs ir OFF pie jebkuras OTA kļūdas, arī auth/begin kļūdas,
      // kas ArduinoOTA 3.3.11 var notikt pirms onStart().
      forcePumpOffForOtaSafety();

      if (
          !ota_error_policy::shouldRestartAfterFailure(
            otaTransferStarted
          )
      ) {
        Serial.println(
          "OTA kļūda pirms transfera sākuma — "
          "turpinu bez restarta"
        );
        return;
      }

      // Pēc onStart() WDT tika noņemts, tāpēc neveiksmīgs transfers
      // tiek pabeigts ar tīru restartu fail-safe OFF stāvoklī.
      delay(250);

      ESP.restart();
    }
  );

  // ArduinoOTA.begin() atgriež void. Ja, piemēram, UDP bind neizdodas,
  // publiskais API nedod success statusu. handle() droši no-op, ja instance
  // nav inicializēta; nākamā Wi-Fi online pāreja atkārtos šo setup mēģinājumu.
  ArduinoOTA.begin();

  Serial.println(
    "OTA setup pieprasīts (" +
    WiFi.localIP().toString() +
    "); ArduinoOTA.begin() success statusu neatgriež"
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

    // Pieprasām bounded adapter cleanup. Ja 250 ms logā tas vēl nav
    // termināls, serviceMQTT() turpinās service() arī ar Wi-Fi down.
    mqtt.forceDisconnect();

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

  // Ja rinda pilna, izmetam vecāko PARASTO komandu.
  // STOP/OFF šo FIFO vispār neizmanto.
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
  ].stopEpoch = pumpStopEpoch;

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

  out.stopEpoch =
      commandQueue[
        commandQueueHead
      ].stopEpoch;

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

bool publishDiscoverySensor(uint8_t sensor) {

  char topic[96];
  char payload[600];

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

  return mqtt.publishBestEffort(
    topic,
    payload,
    true
  );
}

bool publishDiscoveryPump() {

  char topic[96];
  char payload[600];

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
    "\"qos\":1,"
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

  return mqtt.publishBestEffort(
    topic,
    payload,
    true
  );
}

void resetMqttSessionInit() {
  mqttSessionInitState = MqttSessionInitState::Idle;
  mqttDiscoverySensorIndex = 0;
  mqttSessionStepStartedAt = 0;
  mqttDiscoveryWindowStartedAt = 0;
  moisturePublishActive = false;
  moisturePublishSensor = 0;
  mqtt.resetTrackedSubscription();
}

void mqttConnectedHandler(bool sessionPresent) {
  (void)sessionPresent;

  mqtt.resetTrackedSubscription();
  mqttDiscoverySensorIndex = 0;
  mqttSessionStepStartedAt = millis();
  mqttDiscoveryWindowStartedAt = 0;
  mqttSessionInitState = MqttSessionInitState::SubscribePump;
}

void mqttDisconnectedHandler(espMqttClientTypes::DisconnectReason reason) {
  (void)reason;
  resetMqttSessionInit();
}

void handleCriticalMqttSessionInitFailure() {
  // Ja pump jau darbojas un connected command path vēl ir pieejams,
  // to netear-downojam. Pēc pump OFF nākamais loop drīkst cleanup/reconnect.
  if (pumpRunning) {
    return;
  }

  mqtt.forceDisconnect();
}

void serviceMqttSessionInit() {

  if (
      !mqtt.isConnected() ||
      mqttSessionInitState == MqttSessionInitState::Idle ||
      mqttSessionInitState == MqttSessionInitState::Done
  ) {
    return;
  }

  const uint32_t now = millis();

  switch (mqttSessionInitState) {

    case MqttSessionInitState::SubscribePump:
      if (mqtt.startTrackedSubscription(T_PUMP_CMD)) {
        mqttSessionInitState = MqttSessionInitState::AwaitPumpSuback;
        mqttSessionStepStartedAt = now;
        return;
      }

      if (
          mqtt_runtime_policy::hasElapsed(
            now,
            mqttSessionStepStartedAt,
            MQTT_SESSION_CRITICAL_STEP_BUDGET_MS
          )
      ) {
        handleCriticalMqttSessionInitFailure();
      }
      return;

    case MqttSessionInitState::AwaitPumpSuback: {
      const auto state = mqtt.trackedSubscriptionState(now);

      if (state == mqtt_runtime_policy::TrackedSubscriptionState::awaitingAck) {
        return;
      }

      if (state != mqtt_runtime_policy::TrackedSubscriptionState::accepted) {
        handleCriticalMqttSessionInitFailure();
        return;
      }

      mqtt.resetTrackedSubscription();
      mqttSessionInitState = MqttSessionInitState::SubscribeCmd;
      mqttSessionStepStartedAt = now;
      return;
    }

    case MqttSessionInitState::SubscribeCmd:
      if (mqtt.startTrackedSubscription(T_CMD)) {
        mqttSessionInitState = MqttSessionInitState::AwaitCmdSuback;
        mqttSessionStepStartedAt = now;
        return;
      }

      if (
          mqtt_runtime_policy::hasElapsed(
            now,
            mqttSessionStepStartedAt,
            MQTT_SESSION_CRITICAL_STEP_BUDGET_MS
          )
      ) {
        handleCriticalMqttSessionInitFailure();
      }
      return;

    case MqttSessionInitState::AwaitCmdSuback: {
      const auto state = mqtt.trackedSubscriptionState(now);

      if (state == mqtt_runtime_policy::TrackedSubscriptionState::awaitingAck) {
        return;
      }

      if (state != mqtt_runtime_policy::TrackedSubscriptionState::accepted) {
        handleCriticalMqttSessionInitFailure();
        return;
      }

      mqtt.resetTrackedSubscription();
      mqttSessionInitState = MqttSessionInitState::Online;
      mqttSessionStepStartedAt = now;
      return;
    }

    case MqttSessionInitState::Online:
      if (
          mqtt.publishBestEffort(
            T_STATUS,
            "online",
            true
          )
      ) {
        mqttSessionInitState = MqttSessionInitState::DiscoverySensors;
        mqttDiscoverySensorIndex = 0;
        mqttDiscoveryWindowStartedAt = now;
        return;
      }

      if (
          mqtt_runtime_policy::hasElapsed(
            now,
            mqttSessionStepStartedAt,
            MQTT_SESSION_CRITICAL_STEP_BUDGET_MS
          )
      ) {
        handleCriticalMqttSessionInitFailure();
      }
      return;

    case MqttSessionInitState::DiscoverySensors:
      if (
          mqtt_runtime_policy::hasElapsed(
            now,
            mqttDiscoveryWindowStartedAt,
            MQTT_DISCOVERY_WINDOW_BUDGET_MS
          )
      ) {
        Serial.println(
          "MQTT discovery logs izsmelts — atlikusī sensoru discovery izlaista"
        );
        mqttSessionInitState = MqttSessionInitState::DiscoveryPump;
        return;
      }

      if (mqttDiscoverySensorIndex >= SENSOR_COUNT) {
        mqttSessionInitState = MqttSessionInitState::DiscoveryPump;
        return;
      }

      if (publishDiscoverySensor(mqttDiscoverySensorIndex)) {
        mqttDiscoverySensorIndex++;
      }
      return;

    case MqttSessionInitState::DiscoveryPump:
      if (
          mqtt_runtime_policy::hasElapsed(
            now,
            mqttDiscoveryWindowStartedAt,
            MQTT_DISCOVERY_WINDOW_BUDGET_MS
          ) ||
          publishDiscoveryPump()
      ) {
        mqttSessionInitState = MqttSessionInitState::PumpStatus;
        mqttSessionStepStartedAt = now;
      }
      return;

    case MqttSessionInitState::PumpStatus:
      if (publishPumpStatus()) {
        mqttSessionInitState = MqttSessionInitState::ConnectedLog;
        return;
      }

      if (
          mqtt_runtime_policy::hasElapsed(
            now,
            mqttSessionStepStartedAt,
            MQTT_SESSION_CRITICAL_STEP_BUDGET_MS
          )
      ) {
        handleCriticalMqttSessionInitFailure();
      }
      return;

    case MqttSessionInitState::ConnectedLog:
      logEvent("MQTT savienots");
      mqttSessionInitState = MqttSessionInitState::StartupLog;
      return;

    case MqttSessionInitState::StartupLog:
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
      }

      mqttSessionInitState = MqttSessionInitState::StartupPumpWarning;
      return;

    case MqttSessionInitState::StartupPumpWarning:
      if (pumpWasRunningAtBoot) {
        tgSend(
          "⚠️ Sistēma restartējās "
          "laistīšanas laikā. "
          "Sūknis TAGAD ir izslēgts. "
          "Pārbaudi manuāli, ja šaubies."
        );

        pumpWasRunningAtBoot = false;
      }

      mqttSessionInitState = MqttSessionInitState::WifiRestoredLog;
      return;

    case MqttSessionInitState::WifiRestoredLog:
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

      // Ready nav atkarīgs no Telegram PUBACK. Jauns Telegram network
      // enqueue un periodiskais moisture sākas tikai pēc šīs pārejas.
      mqttSessionInitState = MqttSessionInitState::Done;
      lastMqttPublish =
          millis() -
          MQTT_PUBLISH_INTERVAL_MS;
      return;

    case MqttSessionInitState::Idle:
    case MqttSessionInitState::Done:
      return;
  }
}

// ============================================================
// MQTT
// ============================================================

bool isCommandTrimChar(char value) {
  return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}

void trimCommandBuffer(char* command, std::size_t& length) {
  std::size_t first = 0;

  while (first < length && isCommandTrimChar(command[first])) {
    first++;
  }

  std::size_t last = length;
  while (last > first && isCommandTrimChar(command[last - 1U])) {
    last--;
  }

  const std::size_t trimmedLength = last - first;
  if (first > 0U && trimmedLength > 0U) {
    std::memmove(command, command + first, trimmedLength);
  }

  length = trimmedLength;
  command[length] = '\0';
}

void mqttMessageHandler(
  const char* topic,
  const std::uint8_t* payload,
  std::size_t length,
  const mqtt_runtime_policy::InboundMetadata& metadata
) {
  (void)metadata;

  bool fromHA = false;
  bool recognizedTopic = false;

  if (strcmp(topic, T_PUMP_CMD) == 0) {
    fromHA = true;
    recognizedTopic = true;
  } else if (strcmp(topic, T_CMD) == 0) {
    recognizedTopic = true;
  }

  if (!recognizedTopic) {
    return;
  }

  if (
      !command_payload_policy::isCommandPayloadLengthAllowed(
        length
      )
  ) {
    oversizedCommandDrops++;
    return;
  }

  char command[command_payload_policy::kMaxCommandPayloadBytes + 1U] = {0};

  if (length > 0U) {
    std::memcpy(command, payload, length);
  }
  command[length] = '\0';

  trimCommandBuffer(command, length);

  // Urgent STOP/OFF tiek pārbaudīts uz fixed buffer pirms dinamiska String.
  if (
      command_safety::isUrgentStop(
        fromHA,
        command
      )
  ) {
    requestUrgentPumpStop(fromHA);
    return;
  }

  String message(command);
  enqueueCommand(fromHA, message);
}

void mqttRejectedHandler(
  mqtt_runtime_policy::RejectReason reason,
  std::size_t totalBytes
) {
  if (reason == mqtt_runtime_policy::RejectReason::oversized) {
    oversizedCommandDrops++;
  }

  Serial.printf(
    "BRĪDINĀJUMS: MQTT komanda atmesta adapterī (reason=%u, len=%u)\n",
    static_cast<unsigned int>(reason),
    static_cast<unsigned int>(totalBytes)
  );
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
      WiFi.status() != WL_CONNECTED ||
      pumpRunning ||
      !mqtt.isDisconnected()
  ) {
    return;
  }

  Serial.println(
    "Mēģinu pieslēgt MQTT..."
  );

  if (mqtt.connectBlocking()) {
    Serial.println(
      "MQTT transports savienots; gaidu broker-confirmētu session init"
    );
    return;
  }

  static uint32_t lastFailLog = 0;

  Serial.println(
    "MQTT savienojums neizdevās"
  );

  if (
      millis() -
      lastFailLog >=
      60000UL
  ) {
    lastFailLog = millis();

    sendSyslog(
      "MQTT savienojums NEIZDEVĀS"
    );
  }
}

void serviceMQTT() {

  const bool wifiConnected =
      WiFi.status() ==
      WL_CONNECTED;

  const bool mqttConnected =
      mqtt.isConnected();

  if (!wifiConnected) {
    if (!mqtt.isDisconnected()) {
      mqtt.service();
    }
    return;
  }

  if (mqttConnected) {
    mqtt.service();
    serviceMqttSessionInit();
    return;
  }

  if (mqtt.isTransitioning()) {
    mqtt.service();

    if (
        mqtt_runtime_policy::shouldAbortTransitionalConnection(
          pumpRunning,
          mqtt.isConnected(),
          mqtt.isDisconnected()
        )
    ) {
      mqtt.abortTransition();
    }
    return;
  }

  const uint32_t now = millis();

  if (
      !network_policy::shouldAttemptMqttReconnect(
        wifiConnected,
        mqttConnected,
        pumpRunning,
        now,
        lastMqttReconnectAttempt,
        MQTT_RECONNECT_INTERVAL_MS
      )
  ) {
    return;
  }

  lastMqttReconnectAttempt = now;
  connectMQTT();
}

// ============================================================
// MQTT MITRUMA PUBLICĒŠANA
// ============================================================

void resetMoisturePublish() {
  moisturePublishActive = false;
  moisturePublishSensor = 0;
}

bool startMoisturePublish() {

  if (
      moisturePublishActive ||
      pumpRunning ||
      !mqttSessionReady() ||
      !mqtt.isConnected() ||
      mqtt.trackedPublishBusy() ||
      telegramQueueCount > 0 ||
      commandQueueCount > 0
  ) {
    return false;
  }

  moisturePublishActive = true;
  moisturePublishSensor = 0;
  return true;
}

void serviceMoisturePublish() {

  if (!moisturePublishActive) {
    return;
  }

  // Telegram atbilde un ienākošās komandas vienmēr ir prioritāras pār
  // periodisko best-effort sensoru telemetriju.
  if (
      commandQueueCount > 0 ||
      mqtt.trackedPublishBusy() ||
      telegramQueueCount > 0
  ) {
    return;
  }

  if (
      pumpRunning ||
      !mqttSessionReady() ||
      !mqtt.isConnected()
  ) {
    resetMoisturePublish();
    return;
  }

  if (moisturePublishSensor >= SENSOR_COUNT) {
    resetMoisturePublish();
    Serial.println(
      "MQTT mitrums nosūtīts"
    );
    return;
  }

  const uint8_t sensor =
      moisturePublishSensor++;

  int raw =
      readMoistureRaw(
        sensor
      );

  servicePumpCriticalNetworkInput();

  // Ja sensoru lasījuma laikā saņēmām komandu, neuzsākam background
  // MQTT write; komandu apstrādās nākamais loop cikls.
  if (
      commandQueueCount > 0 ||
      mqtt.trackedPublishBusy() ||
      telegramQueueCount > 0
  ) {
    return;
  }

  if (
      pumpRunning ||
      !mqttSessionReady() ||
      !mqtt.isConnected()
  ) {
    resetMoisturePublish();
    return;
  }

  String category =
      categorizeMoisture(
        raw
      );

  if (category.length() != 0) {
    char topic[48];

    snprintf(
      topic,
      sizeof(topic),
      "balkons/puke%d/mitrums",
      sensor + 1
    );

    // QoS0 telemetry ir best-effort. Queue pressure nozīmē drop, ne retry burst.
    mqtt.publishBestEffort(
      topic,
      category.c_str(),
      false
    );
  }

  servicePump();

  if (moisturePublishSensor >= SENSOR_COUNT) {
    resetMoisturePublish();
    Serial.println(
      "MQTT mitrums nosūtīts"
    );
  }
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

  logCommandReceipt(
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

    uint32_t minutes = 0U;

    const uint32_t maxMinutes =
        MAX_PUMP_SECONDS /
        60U;

    const bool validMinutes =
        command_parse_policy::parsePositiveDecimal(
          command.c_str() + 6,
          maxMinutes,
          minutes
        );

    if (validMinutes) {

      uint32_t requestedSeconds =
          minutes *
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

      servicePumpCriticalNetworkInput();

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

      servicePumpCriticalNetworkInput();

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
      640
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
        "Min. brīvā atmiņa: " +
        String(
          ESP.getMinFreeHeap() /
          1024
        ) +
        " KB\n";

    status +=
        "Lielākais heap bloks: " +
        String(
          ESP.getMaxAllocHeap() /
          1024
        ) +
        " KB\n";

    status +=
        "Loop steks min. brīvs: " +
        String(
          static_cast<unsigned long>(
            uxTaskGetStackHighWaterMark(
              nullptr
            )
          )
        ) +
        " B\n";

    status +=
        "Pārāk garas komandas: " +
        String(
          oversizedCommandDrops
        ) +
        "\n";

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

  if (commandQueueCount == 0) {
    return;
  }

  const PendingCommand& head =
      commandQueue[commandQueueHead];

  const bool mqttReady =
      mqttSessionReady() &&
      mqtt.isConnected();

  // QoS1 command delivery can leave PUBACK work in the local outbox after
  // callback return. Keep pump-start at the FIFO head until that protocol
  // work (and any tracked publish) is drained while the relay is still OFF.
  if (
      command_safety::shouldDeferQueuedPumpStartForNetwork(
        pumpRunning,
        mqttReady,
        mqtt.queueSize(),
        mqtt.trackedPublishBusy(),
        head.fromHA,
        head.payload.c_str()
      )
  ) {
    return;
  }

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

  if (
      command_safety::shouldSuppressQueuedPumpStart(
        item.stopEpoch,
        pumpStopEpoch,
        item.fromHA,
        item.payload.c_str()
      )
  ) {

    logEvent(
      "Drošība: stale pump-start komanda atmesta pēc jaunāka STOP/OFF"
    );

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

  mqtt.configure(
    MQTT_SERVER,
    MQTT_PORT,
    mqttClientId,
    MQTT_USERNAME,
    MQTT_PASSWORD,
    T_STATUS,
    "offline"
  );

  mqtt.setConnectedHandler(
    mqttConnectedHandler
  );

  mqtt.setDisconnectedHandler(
    mqttDisconnectedHandler
  );

  mqtt.setMessageHandler(
    mqttMessageHandler
  );

  mqtt.setRejectedHandler(
    mqttRejectedHandler
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

  // Ja urgent STOP palicis no iepriekšējā tīkla callback,
  // tas vienmēr dominē pirms jebkura cita loop darba.
  serviceUrgentPumpStop();

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
      WiFi.status() ==
      WL_CONNECTED
  ) {

    // ArduinoOTA.handle() publiski no-op, ja begin() nav inicializējis servisu.
    ArduinoOTA.handle();
  }

  // ----------------------------------------------------------
  // MQTT
  // ----------------------------------------------------------

  serviceMQTT();

  // mqtt.loop() varēja pieņemt urgent STOP/OFF. Relejs callbackā
  // jau tika fiziski izslēgts; šeit nekavējoties sakārtojam state,
  // statistiku, logus un retained statusu pirms parastās FIFO.
  serviceUrgentPumpStop();

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
  // Telegram atbildei ir prioritāte pār background sensoru telemetriju.
  // ----------------------------------------------------------

  if (mqttSessionReady()) {
    serviceTelegramDelivery();
  }

  // ----------------------------------------------------------
  // Periodiska mitruma publicēšana — pa vienam sensoram ciklā.
  // ----------------------------------------------------------

  if (
      !moisturePublishActive &&
      !pumpRunning &&
      mqttSessionReady() &&
      mqtt.isConnected() &&
      millis() -
      lastMqttPublish >=
      MQTT_PUBLISH_INTERVAL_MS
  ) {

    if (startMoisturePublish()) {
      lastMqttPublish =
          millis();
    }
  }

  serviceMoisturePublish();

  // Īss yield sistēmas taskiem.
  delay(1);
}