#pragma once

#include <cstddef>
#include <cstdint>

#include <espMqttClient.h>

#include "mqtt_runtime_policy.h"

class MqttRuntimeAdapter : public espMqttClient {
 public:
  using ConnectedHandler = void (*)(bool sessionPresent);
  using DisconnectedHandler = void (*)(espMqttClientTypes::DisconnectReason reason);
  using MessageHandler = void (*)(
      const char* topic,
      const std::uint8_t* payload,
      std::size_t length,
      const mqtt_runtime_policy::InboundMetadata& metadata);
  using RejectedHandler = void (*)(
      mqtt_runtime_policy::RejectReason reason,
      std::size_t totalBytes);

  MqttRuntimeAdapter();

  void configure(
      const char* server,
      std::uint16_t port,
      const char* clientId,
      const char* username,
      const char* password,
      const char* willTopic,
      const char* willPayload);

  void setConnectedHandler(ConnectedHandler handler);
  void setDisconnectedHandler(DisconnectedHandler handler);
  void setMessageHandler(MessageHandler handler);
  void setRejectedHandler(RejectedHandler handler);

  bool isConnected() const;
  bool isDisconnected() const;
  bool isTransitioning() const;

  bool startConnect();
  bool connectBlocking();
  bool abortTransition();
  bool forceDisconnect();
  void service();

  bool subscribeTopic(const char* topic, std::uint8_t qos);
  bool publishMessage(
      const char* topic,
      const char* payload,
      std::uint8_t qos,
      bool retain);

  bool subscribeBestEffort(const char* topic, std::uint8_t qos);
  bool publishBestEffort(
      const char* topic,
      const char* payload,
      bool retain = false);

  bool startTrackedPublish(
      const char* topic,
      const char* payload,
      bool retain = false);
  bool pumpTrackedPublish();
  bool trackedPublishBusy() const;
  bool trackedPublishInFlight() const;

 private:
  void resetAssembly();
  void rejectCurrent(
      mqtt_runtime_policy::RejectReason reason,
      std::size_t totalBytes);
  void handleIncoming(
      const espMqttClientTypes::MessageProperties& properties,
      const char* topic,
      const std::uint8_t* payload,
      std::size_t len,
      std::size_t index,
      std::size_t total);
  void handlePublishAck(std::uint16_t packetId);
  void resetTrackedInFlight();
  void clearTrackedPublish();

  ConnectedHandler connectedHandler_ = nullptr;
  DisconnectedHandler disconnectedHandler_ = nullptr;
  MessageHandler messageHandler_ = nullptr;
  RejectedHandler rejectedHandler_ = nullptr;

  bool assembling_ = false;
  bool dropCurrent_ = false;
  bool rejectReported_ = false;
  std::size_t expectedTotal_ = 0;
  std::size_t nextIndex_ = 0;
  mqtt_runtime_policy::InboundMetadata metadata_{0U, false, false, 0U};
  std::uint8_t commandBuffer_[command_payload_policy::kMaxCommandPayloadBytes] = {0};

  bool trackedActive_ = false;
  bool trackedInFlight_ = false;
  bool trackedRetain_ = false;
  std::uint16_t trackedPacketId_ = 0U;
  char trackedTopic_[mqtt_runtime_policy::kTrackedTopicBytes] = {0};
  char trackedPayload_[mqtt_runtime_policy::kTrackedPayloadBytes] = {0};
};
