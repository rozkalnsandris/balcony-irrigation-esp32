#if defined(MQTT_RUNTIME_ADAPTER_BUILD)

#include "mqtt_runtime_adapter.h"

#include <cstring>

MqttRuntimeAdapter::MqttRuntimeAdapter()
    : espMqttClient(espMqttClientTypes::UseInternalTask::NO) {
  onConnect([this](bool sessionPresent) {
    if (connectedHandler_ != nullptr) {
      connectedHandler_(sessionPresent);
    }
  });

  onDisconnect([this](espMqttClientTypes::DisconnectReason reason) {
    resetAssembly();
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
}

void MqttRuntimeAdapter::configure(
    const char* server,
    std::uint16_t port,
    const char* clientId,
    const char* username,
    const char* password,
    const char* willTopic,
    const char* willPayload) {
  _client.client.setConnectionTimeout(
      mqtt_runtime_policy::kTcpConnectionTimeoutMs);

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

  return connect();
}

bool MqttRuntimeAdapter::abortTransition() {
  if (!isTransitioning()) {
    return false;
  }

  resetAssembly();
  return disconnect(true);
}

void MqttRuntimeAdapter::service() {
  loop();
}

bool MqttRuntimeAdapter::subscribeTopic(const char* topic, std::uint8_t qos) {
  if (!connected()) {
    return false;
  }

  return subscribe(topic, qos) != 0U;
}

bool MqttRuntimeAdapter::publishMessage(
    const char* topic,
    const char* payload,
    std::uint8_t qos,
    bool retain) {
  if (!connected()) {
    return false;
  }

  return publish(topic, qos, retain, payload) != 0U;
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

#endif
