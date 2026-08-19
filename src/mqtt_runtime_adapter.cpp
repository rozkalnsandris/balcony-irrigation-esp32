#if defined(MQTT_RUNTIME_ADAPTER_BUILD)

#include "mqtt_runtime_adapter.h"

#include <Arduino.h>

#include <cstring>

MqttRuntimeAdapter::MqttRuntimeAdapter()
    : espMqttClient(espMqttClientTypes::UseInternalTask::NO) {
  _client.client.setConnectionTimeout(
      mqtt_runtime_policy::kTcpConnectionTimeoutMs);

  setKeepAlive(mqtt_runtime_policy::kKeepAliveSeconds)
      .setTimeout(mqtt_runtime_policy::kMqttAckTimeoutSeconds)
      .setCleanSession(true);

  onConnect([this](bool sessionPresent) {
    if (connectedHandler_ != nullptr) {
      connectedHandler_(sessionPresent);
    }
  });

  onDisconnect([this](espMqttClientTypes::DisconnectReason reason) {
    resetAssembly();
    resetTrackedInFlight();
    if (disconnectedHandler_ != nullptr) {
      disconnectedHandler_(reason);
    }
  });

  onMessage(
      [this](
          const espMqttClientTypes::MessageProperties& properties,
          const char* topic,
          const std::uint8_t* payload,
          std::size_t len,
          std::size_t index,
          std::size_t total) {
        handleIncoming(properties, topic, payload, len, index, total);
      });

  onPublish([this](std::uint16_t packetId) {
    handlePublishAck(packetId);
  });
}

void MqttRuntimeAdapter::configure(
    const char* server,
    std::uint16_t port,
    const char* clientId,
    const char* username,
    const char* password,
    const char* willTopic,
    const char* willPayload) {
  setServer(server, port)
      .setClientId(clientId)
      .setCredentials(username, password)
      .setWill(willTopic, 0U, true, willPayload)
      .setKeepAlive(mqtt_runtime_policy::kKeepAliveSeconds)
      .setTimeout(mqtt_runtime_policy::kMqttAckTimeoutSeconds)
      .setCleanSession(true);
}

void MqttRuntimeAdapter::setConnectedHandler(ConnectedHandler handler) {
  connectedHandler_ = handler;
}

void MqttRuntimeAdapter::setDisconnectedHandler(DisconnectedHandler handler) {
  disconnectedHandler_ = handler;
}

void MqttRuntimeAdapter::setMessageHandler(MessageHandler handler) {
  messageHandler_ = handler;
}

void MqttRuntimeAdapter::setRejectedHandler(RejectedHandler handler) {
  rejectedHandler_ = handler;
}

bool MqttRuntimeAdapter::isConnected() const {
  return connected();
}

bool MqttRuntimeAdapter::isDisconnected() const {
  return disconnected();
}

bool MqttRuntimeAdapter::isTransitioning() const {
  return !connected() && !disconnected();
}

bool MqttRuntimeAdapter::startConnect() {
  if (!disconnected()) {
    return false;
  }

  return espMqttClient::connect();
}

bool MqttRuntimeAdapter::connectBlocking() {
  if (!startConnect()) {
    return false;
  }

  const std::uint32_t startedAt = millis();

  while (isTransitioning() &&
         !mqtt_runtime_policy::hasElapsed(
             millis(),
             startedAt,
             mqtt_runtime_policy::kConnectAttemptBudgetMs)) {
    espMqttClient::loop();
    delay(1);
  }

  if (connected()) {
    return true;
  }

  if (isTransitioning()) {
    forceDisconnect();
  }

  return false;
}

bool MqttRuntimeAdapter::abortTransition() {
  if (!isTransitioning()) {
    return false;
  }

  resetAssembly();
  return forceDisconnect();
}

bool MqttRuntimeAdapter::forceDisconnect() {
  if (disconnected()) {
    resetAssembly();
    resetTrackedInFlight();
    return true;
  }

  espMqttClient::disconnect(true);

  const std::uint32_t startedAt = millis();
  while (!disconnected() &&
         !mqtt_runtime_policy::hasElapsed(
             millis(),
             startedAt,
             mqtt_runtime_policy::kDisconnectCleanupBudgetMs)) {
    espMqttClient::loop();
    delay(1);
  }

  resetAssembly();

  if (disconnected()) {
    resetTrackedInFlight();
    return true;
  }

  return false;
}

void MqttRuntimeAdapter::service() {
  espMqttClient::loop();
}

bool MqttRuntimeAdapter::subscribeTopic(const char* topic, std::uint8_t qos) {
  if (!connected()) {
    return false;
  }

  return espMqttClient::subscribe(topic, qos) != 0U;
}

bool MqttRuntimeAdapter::publishMessage(
    const char* topic,
    const char* payload,
    std::uint8_t qos,
    bool retain) {
  if (!connected()) {
    return false;
  }

  return espMqttClient::publish(topic, qos, retain, payload) != 0U;
}

bool MqttRuntimeAdapter::subscribeBestEffort(
    const char* topic,
    std::uint8_t qos) {
  if (!connected() ||
      !mqtt_runtime_policy::canQueueBestEffort(queueSize())) {
    return false;
  }

  const bool accepted = espMqttClient::subscribe(topic, qos) != 0U;
  if (accepted) {
    espMqttClient::loop();
  }

  return accepted;
}

bool MqttRuntimeAdapter::publishBestEffort(
    const char* topic,
    const char* payload,
    bool retain) {
  if (!connected() ||
      !mqtt_runtime_policy::canQueueBestEffort(queueSize())) {
    return false;
  }

  const bool accepted =
      espMqttClient::publish(topic, 0U, retain, payload) != 0U;
  if (accepted) {
    espMqttClient::loop();
  }

  return accepted;
}

bool MqttRuntimeAdapter::startTrackedPublish(
    const char* topic,
    const char* payload,
    bool retain) {
  if (trackedActive_ || topic == nullptr || payload == nullptr) {
    return false;
  }

  const std::size_t topicLength = std::strlen(topic);
  const std::size_t payloadLength = std::strlen(payload);

  if (topicLength + 1U > sizeof(trackedTopic_) ||
      payloadLength + 1U > sizeof(trackedPayload_)) {
    return false;
  }

  std::memcpy(trackedTopic_, topic, topicLength + 1U);
  std::memcpy(trackedPayload_, payload, payloadLength + 1U);

  trackedActive_ = true;
  trackedInFlight_ = false;
  trackedRetain_ = retain;
  trackedPacketId_ = 0U;
  return true;
}

bool MqttRuntimeAdapter::pumpTrackedPublish() {
  if (!trackedActive_) {
    return false;
  }

  if (trackedInFlight_) {
    return true;
  }

  if (!connected() ||
      !mqtt_runtime_policy::canQueueBestEffort(queueSize())) {
    return false;
  }

  const std::uint16_t packetId =
      espMqttClient::publish(
          trackedTopic_,
          1U,
          trackedRetain_,
          trackedPayload_);

  if (packetId == 0U) {
    return false;
  }

  trackedPacketId_ = packetId;
  trackedInFlight_ = true;
  espMqttClient::loop();
  return true;
}

bool MqttRuntimeAdapter::trackedPublishBusy() const {
  return trackedActive_;
}

bool MqttRuntimeAdapter::trackedPublishInFlight() const {
  return trackedInFlight_;
}

void MqttRuntimeAdapter::resetAssembly() {
  assembling_ = false;
  dropCurrent_ = false;
  rejectReported_ = false;
  expectedTotal_ = 0U;
  nextIndex_ = 0U;
  metadata_ = {0U, false, false, 0U};
}

void MqttRuntimeAdapter::rejectCurrent(
    mqtt_runtime_policy::RejectReason reason,
    std::size_t totalBytes) {
  dropCurrent_ = true;

  if (!rejectReported_ && rejectedHandler_ != nullptr) {
    rejectReported_ = true;
    rejectedHandler_(reason, totalBytes);
  }
}

void MqttRuntimeAdapter::handleIncoming(
    const espMqttClientTypes::MessageProperties& properties,
    const char* topic,
    const std::uint8_t* payload,
    std::size_t len,
    std::size_t index,
    std::size_t total) {
  if (index == 0U) {
    resetAssembly();
    assembling_ = true;
    expectedTotal_ = total;
    metadata_ = {
        properties.qos,
        properties.dup,
        properties.retain,
        properties.packetId,
    };

    const mqtt_runtime_policy::RejectReason envelopeResult =
        mqtt_runtime_policy::validateEnvelope(
            total,
            properties.qos,
            properties.retain);

    if (envelopeResult != mqtt_runtime_policy::RejectReason::none) {
      rejectCurrent(envelopeResult, total);
    }
  }

  if (!assembling_) {
    assembling_ = true;
    expectedTotal_ = total;
    nextIndex_ = index;
    rejectCurrent(mqtt_runtime_policy::RejectReason::malformedChunks, total);
  }

  if (!dropCurrent_) {
    const bool metadataMatches =
        properties.qos == metadata_.qos &&
        properties.dup == metadata_.duplicate &&
        properties.retain == metadata_.retained &&
        properties.packetId == metadata_.packetId;

    if (!metadataMatches ||
        !mqtt_runtime_policy::isChunkSequenceValid(
            nextIndex_,
            expectedTotal_,
            index,
            len,
            total) ||
        (len > 0U && payload == nullptr)) {
      rejectCurrent(mqtt_runtime_policy::RejectReason::malformedChunks, total);
    }
  }

  if (!dropCurrent_ && len > 0U) {
    std::memcpy(commandBuffer_ + index, payload, len);
    nextIndex_ = index + len;
  }

  const bool finalChunk = index <= total && len <= total - index &&
                          index + len == total;

  if (finalChunk) {
    if (!dropCurrent_ && messageHandler_ != nullptr) {
      messageHandler_(topic, commandBuffer_, total, metadata_);
    }

    resetAssembly();
  }
}

void MqttRuntimeAdapter::handlePublishAck(std::uint16_t packetId) {
  if (!trackedActive_ || !trackedInFlight_ ||
      packetId != trackedPacketId_) {
    return;
  }

  clearTrackedPublish();
}

void MqttRuntimeAdapter::resetTrackedInFlight() {
  trackedInFlight_ = false;
  trackedPacketId_ = 0U;
}

void MqttRuntimeAdapter::clearTrackedPublish() {
  trackedActive_ = false;
  trackedInFlight_ = false;
  trackedRetain_ = false;
  trackedPacketId_ = 0U;
  trackedTopic_[0] = '\0';
  trackedPayload_[0] = '\0';
}

#endif
