#pragma once

#include <cstddef>
#include <cstdint>

#include "command_payload_policy.h"

namespace mqtt_runtime_policy {

constexpr std::uint32_t kTcpConnectionTimeoutMs = 1000U;
constexpr std::uint16_t kMqttAckTimeoutSeconds = 2U;
constexpr std::uint16_t kKeepAliveSeconds = 30U;
constexpr std::size_t kRxBufferBytes = 1024U;
constexpr std::size_t kTxBufferBytes = 1024U;

enum class RejectReason : std::uint8_t {
  none = 0,
  oversized = 1,
  retained = 2,
  unsupportedQos = 3,
  malformedChunks = 4,
};

struct InboundMetadata {
  std::uint8_t qos;
  bool duplicate;
  bool retained;
  std::uint16_t packetId;
};

inline RejectReason validateEnvelope(
    std::size_t totalBytes,
    std::uint8_t qos,
    bool retained) {
  if (retained) {
    return RejectReason::retained;
  }

  if (qos > 1U) {
    return RejectReason::unsupportedQos;
  }

  if (!command_payload_policy::isCommandPayloadLengthAllowed(totalBytes)) {
    return RejectReason::oversized;
  }

  return RejectReason::none;
}

inline bool isChunkSequenceValid(
    std::size_t expectedIndex,
    std::size_t expectedTotal,
    std::size_t index,
    std::size_t len,
    std::size_t total) {
  if (total != expectedTotal || index != expectedIndex) {
    return false;
  }

  if (index > total || len > total - index) {
    return false;
  }

  return true;
}

inline bool shouldAbortTransitionalConnection(
    bool pumpRunning,
    bool connected,
    bool disconnected) {
  return pumpRunning && !connected && !disconnected;
}

}  // namespace mqtt_runtime_policy
