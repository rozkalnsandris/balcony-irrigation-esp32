#!/usr/bin/env python3
"""Deterministically render the espMqttClient runtime migration candidate.

The renderer is intentionally fail-closed.  Normal CLI use first verifies the
exact Git blob identity of the canonical PubSubClient ``src/main.cpp`` and only
then applies R01-R13 exact literal/block transformations.  It never writes back
to the input path; callers must provide an explicit separate output path.
"""

from __future__ import annotations

import argparse
import hashlib
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Sequence

CANONICAL_MAIN_GIT_BLOB = "1b4fd87415cd9cce9b24eae2dd1f574aafe35fd7"
CANONICAL_MAIN_COMMIT = "da9bdeaf2eba6c8fda02a3eb15f070428c22e595"
IMPLEMENTATION_STACK_BASE = "a9f55d9926342ec9f27bc3f1aa04edb45fcb0b5b"


class RenderError(RuntimeError):
    """Fail-closed renderer error."""


@dataclass(frozen=True)
class LiteralRule:
    rule_id: str
    old: str
    new: str

    def apply(self, source: str) -> str:
        count = source.count(self.old)
        if count != 1:
            raise RenderError(
                f"{self.rule_id}: exact anchor count must be 1, got {count}"
            )
        return source.replace(self.old, self.new, 1)


@dataclass(frozen=True)
class BlockRule:
    rule_id: str
    start: str
    end: str
    new: str

    def apply(self, source: str) -> str:
        start_count = source.count(self.start)
        end_count = source.count(self.end)
        if start_count != 1:
            raise RenderError(
                f"{self.rule_id}: exact start anchor count must be 1, got {start_count}"
            )
        if end_count != 1:
            raise RenderError(
                f"{self.rule_id}: exact end anchor count must be 1, got {end_count}"
            )

        start_index = source.index(self.start)
        end_index = source.index(self.end, start_index + len(self.start))
        if end_index <= start_index:
            raise RenderError(f"{self.rule_id}: end anchor does not follow start anchor")

        return source[:start_index] + self.new + source[end_index:]


R01_NEW = '''#include "mqtt_runtime_adapter.h"\n#include <cstring>\n'''

R02_NEW = '''MqttRuntimeAdapter mqtt;\nWiFiUDP udp;\n\nenum class MqttSessionInitState : uint8_t {\n  Idle = 0,\n  SubscribePump,\n  AwaitPumpSuback,\n  SubscribeCmd,\n  AwaitCmdSuback,\n  Online,\n  DiscoverySensors,\n  DiscoveryPump,\n  PumpStatus,\n  ConnectedLog,\n  StartupLog,\n  StartupPumpWarning,\n  WifiRestoredLog,\n  Done,\n};\n\nconstexpr uint32_t MQTT_SESSION_CRITICAL_STEP_BUDGET_MS = 6500UL;\nconstexpr uint32_t MQTT_DISCOVERY_WINDOW_BUDGET_MS = 6500UL;\n\nMqttSessionInitState mqttSessionInitState = MqttSessionInitState::Idle;\nuint8_t mqttDiscoverySensorIndex = 0;\nuint32_t mqttSessionStepStartedAt = 0;\nuint32_t mqttDiscoveryWindowStartedAt = 0;\nuint32_t oversizedTelegramDrops = 0;\n\nbool mqttSessionReady() {\n  return mqttSessionInitState == MqttSessionInitState::Done;\n}\n'''

R03_NEW = r'''bool telegramPayloadAllowed(const String& msg) {
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

  if (!mqttSessionReady()) {
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

'''

R04_NEW = r'''bool mqttDiagnosticPublishAllowed() {
  if (!mqtt.isConnected()) {
    return false;
  }

  return mqttSessionReady() ||
         mqttSessionInitState == MqttSessionInitState::ConnectedLog ||
         mqttSessionInitState == MqttSessionInitState::StartupLog ||
         mqttSessionInitState == MqttSessionInitState::WifiRestoredLog;
}

void logEvent(const String& msg) {

  String line =
      getTimeString() +
      " | " +
      msg;

  Serial.println(
    "LOG: " +
    line
  );

  if (mqttDiagnosticPublishAllowed()) {
    mqtt.publishBestEffort(
      T_LOG,
      line.c_str(),
      false
    );
  }

  sendSyslog(msg);
}

// ============================================================
// SŪKŅA VADĪBA
// ============================================================

bool publishPumpStatus() {

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
    pumpRunning ? "ON" : "OFF",
    true
  );
}

'''

R05_NEW = r'''void servicePumpCriticalNetworkInput() {

  if (!mqtt.isDisconnected()) {
    mqtt.service();
  }

  serviceUrgentPumpStop();
  servicePump();
  feedWatchdog();
}

'''

R06_NEW = r'''  // Tikko zaudējām Wi-Fi.
  if (wifiOnline) {

    wifiOnline = false;

    // Pieprasām bounded adapter cleanup. Ja 250 ms logā tas vēl nav
    // termināls, serviceMQTT() turpinās service() arī ar Wi-Fi down.
    mqtt.forceDisconnect();

    Serial.println(
      "WiFi savienojums pazudis"
    );
  }

'''

R07_NEW = r'''bool publishDiscoverySensor(uint8_t sensor) {

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

'''

R08_NEW = r'''bool isCommandTrimChar(char value) {
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

'''

R09_NEW = r'''void connectMQTT() {

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

'''

R10_NEW = r'''void serviceMQTT() {

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

'''

R11_NEW = r'''void publishMoisture() {

  if (
      !mqttSessionReady() ||
      !mqtt.isConnected()
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

    servicePumpCriticalNetworkInput();

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

    // QoS0 telemetry ir best-effort. Queue pressure nozīmē drop, ne retry burst.
    mqtt.publishBestEffort(
      topic,
      category.c_str(),
      false
    );

    servicePump();
  }

  Serial.println(
    "MQTT mitrums nosūtīts"
  );
}

'''

R12_NEW = r'''  mqtt.configure(
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

'''

R13_NEW = r'''  // ----------------------------------------------------------
  // Pēc application session Ready apkalpojam vienu bounded Telegram soli.
  // ----------------------------------------------------------

  if (mqttSessionReady()) {
    serviceTelegramDelivery();
  }

'''

RULES: Sequence[LiteralRule | BlockRule] = (
    LiteralRule("R01", "#include <PubSubClient.h>\n", R01_NEW),
    LiteralRule(
        "R02",
        "WiFiClient mqttNet;\nPubSubClient mqtt(mqttNet);\nWiFiUDP udp;\n",
        R02_NEW,
    ),
    BlockRule(
        "R03",
        "bool publishTelegramNow(const String& msg) {",
        "// ============================================================\n// LOGI\n// ============================================================",
        R03_NEW,
    ),
    BlockRule(
        "R04",
        "void logEvent(const String& msg) {",
        "bool startPump(uint32_t seconds) {",
        R04_NEW,
    ),
    BlockRule(
        "R05",
        "void servicePumpCriticalNetworkInput() {",
        "// ============================================================\n// OTA\n// ============================================================",
        R05_NEW,
    ),
    BlockRule(
        "R06",
        "  // Tikko zaudējām Wi-Fi.\n  if (wifiOnline) {",
        "  uint32_t now =\n      millis();",
        R06_NEW,
    ),
    BlockRule(
        "R07",
        "void sendDiscovery() {",
        "// ============================================================\n// MQTT\n// ============================================================",
        R07_NEW,
    ),
    BlockRule(
        "R08",
        "void mqttCallback(\n",
        "void handleDeferredSystemLogs() {",
        R08_NEW,
    ),
    BlockRule(
        "R09",
        "void connectMQTT() {",
        "void serviceMQTT() {",
        R09_NEW,
    ),
    BlockRule(
        "R10",
        "void serviceMQTT() {",
        "// ============================================================\n// MQTT MITRUMA PUBLICĒŠANA\n// ============================================================",
        R10_NEW,
    ),
    BlockRule(
        "R11",
        "void publishMoisture() {",
        "// ============================================================\n// KOMANDAS\n// ============================================================",
        R11_NEW,
    ),
    BlockRule(
        "R12",
        "  // Arduino-ESP32 NetworkClient noklusējums ir 3000 ms.\n",
        "  // ----------------------------------------------------------\n  // Wi-Fi\n  // ----------------------------------------------------------",
        R12_NEW,
    ),
    BlockRule(
        "R13",
        "  // ----------------------------------------------------------\n  // Ja MQTT ir atgriezies, mēģinām iztukšot\n",
        "  // Īss yield sistēmas taskiem.",
        R13_NEW,
    ),
)

FORBIDDEN_TOKENS: tuple[str, ...] = (
    "<PubSubClient.h>",
    "WiFiClient mqttNet",
    "PubSubClient mqtt",
    "mqttNet.stop",
    "mqtt.setServer",
    "mqtt.setCallback",
    "mqtt.setBufferSize",
    "mqtt.setSocketTimeout",
    "mqtt.state()",
    "mqtt.publish(",
    "mqtt.subscribe(",
    "publishTelegramNow",
    "flushTelegramQueue",
    "sendDiscovery",
)

REQUIRED_MARKERS: tuple[str, ...] = (
    "MqttRuntimeAdapter mqtt;",
    "enum class MqttSessionInitState",
    "bool mqttSessionReady()",
    "void serviceMqttSessionInit()",
    "void serviceTelegramDelivery()",
    "void mqttConnectedHandler(bool sessionPresent)",
    "void mqttDisconnectedHandler(espMqttClientTypes::DisconnectReason reason)",
    "void mqttMessageHandler(",
    "bool publishDiscoverySensor(uint8_t sensor)",
    "bool publishDiscoveryPump()",
    "requestUrgentPumpStop(fromHA);",
    "MAX_PUMP_SECONDS = 180",
    "command_payload_policy::kMaxCommandPayloadBytes",
    "mqtt.startTrackedSubscription(T_PUMP_CMD)",
    "mqtt.startTrackedSubscription(T_CMD)",
    "mqtt.publishBestEffort(\n            T_STATUS,\n            \"online\",\n            true",
    "mqttSessionInitState = MqttSessionInitState::Done;",
)


def git_blob_sha(data: bytes) -> str:
    header = f"blob {len(data)}\0".encode("ascii")
    return hashlib.sha1(header + data).hexdigest()


def sha256_hex(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def verify_canonical_input(data: bytes) -> None:
    actual = git_blob_sha(data)
    if actual != CANONICAL_MAIN_GIT_BLOB:
        raise RenderError(
            "canonical src/main.cpp blob mismatch: "
            f"expected {CANONICAL_MAIN_GIT_BLOB}, got {actual}"
        )


def apply_rules_text(source: str, rules: Sequence[LiteralRule | BlockRule] = RULES) -> str:
    rendered = source
    for rule in rules:
        rendered = rule.apply(rendered)
    return rendered


def validate_rendered(rendered: str) -> None:
    residual = [token for token in FORBIDDEN_TOKENS if token in rendered]
    if residual:
        raise RenderError(
            "forbidden legacy MQTT token(s) survived render: " + ", ".join(residual)
        )

    missing = [marker for marker in REQUIRED_MARKERS if marker not in rendered]
    if missing:
        raise RenderError(
            "required generated marker(s) missing: " + ", ".join(missing)
        )

    urgent_index = rendered.find("requestUrgentPumpStop(fromHA);")
    dynamic_index = rendered.find("String message(command);")
    if urgent_index < 0 or dynamic_index < 0 or urgent_index >= dynamic_index:
        raise RenderError("urgent STOP must remain before dynamic String construction")

    pump_off_marker = "digitalWrite(\n    RELAY_PIN,\n    RELAY_OFF"
    if pump_off_marker not in rendered:
        raise RenderError("physical relay-OFF safety marker is missing")

    pump_sub = rendered.find("mqtt.startTrackedSubscription(T_PUMP_CMD)")
    cmd_sub = rendered.find("mqtt.startTrackedSubscription(T_CMD)")
    online = rendered.find('T_STATUS,\n            "online"')
    ready = rendered.find("mqttSessionInitState = MqttSessionInitState::Done;")
    telegram_service = rendered.find("serviceTelegramDelivery();")
    if not (0 <= pump_sub < cmd_sub < online < ready < telegram_service):
        raise RenderError(
            "generated session ordering must be pump SUBACK -> cmd SUBACK -> online -> Done -> Telegram"
        )


def render_bytes(data: bytes) -> bytes:
    verify_canonical_input(data)

    try:
        source = data.decode("utf-8")
    except UnicodeDecodeError as exc:
        raise RenderError("canonical source is not valid UTF-8") from exc

    rendered_text = apply_rules_text(source)
    validate_rendered(rendered_text)
    return rendered_text.encode("utf-8")


def render_file(input_path: Path, output_path: Path) -> str:
    input_resolved = input_path.resolve()
    output_resolved = output_path.resolve()
    if input_resolved == output_resolved:
        raise RenderError("renderer refuses to overwrite its input path")

    data = input_path.read_bytes()
    rendered = render_bytes(data)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_bytes(rendered)
    return sha256_hex(rendered)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Render the deterministic espMqttClient main.cpp migration candidate"
    )
    parser.add_argument(
        "input",
        type=Path,
        help="canonical PubSubClient src/main.cpp input",
    )
    parser.add_argument(
        "--output",
        required=True,
        type=Path,
        help="explicit separate output path for the generated candidate",
    )
    return parser


def main(argv: Iterable[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        rendered_sha256 = render_file(args.input, args.output)
    except (OSError, RenderError) as exc:
        print(f"ERROR: {exc}")
        return 2

    print(f"input_git_blob={CANONICAL_MAIN_GIT_BLOB}")
    print(f"rendered_sha256={rendered_sha256}")
    print(f"output={args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
