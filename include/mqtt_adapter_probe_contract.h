#pragma once

#include <cstddef>
#include <cstdint>

#include "command_payload_policy.h"

namespace mqtt_adapter_probe_contract {

constexpr std::uint16_t kOperationTimeoutSeconds = 1U;
constexpr std::uint16_t kKeepAliveSeconds = 30U;
constexpr char kProbeClientId[] = "balkonsmqttprobe";
constexpr std::size_t kMqtt311MandatoryClientIdMaxBytes = 23U;

struct InboundMetadata {
  std::uint8_t qos;
  bool duplicate;
  bool retained;
  std::uint16_t packetId;
  std::size_t payloadBytes;
};

inline bool hasSupportedInboundQos(std::uint8_t qos) {
  return qos <= 1U;
}

inline bool isCommandEnvelopeAllowed(const InboundMetadata& metadata) {
  return !metadata.retained &&
         hasSupportedInboundQos(metadata.qos) &&
         command_payload_policy::isCommandPayloadLengthAllowed(
             metadata.payloadBytes);
}

inline bool isAsciiAlphanumeric(char value) {
  return (value >= '0' && value <= '9') ||
         (value >= 'A' && value <= 'Z') ||
         (value >= 'a' && value <= 'z');
}

inline bool isProbeClientIdCompatible() {
  constexpr std::size_t length = sizeof(kProbeClientId) - 1U;

  if (length == 0U || length > kMqtt311MandatoryClientIdMaxBytes) {
    return false;
  }

  for (std::size_t index = 0; index < length; ++index) {
    if (!isAsciiAlphanumeric(kProbeClientId[index])) {
      return false;
    }
  }

  return true;
}

}  // namespace mqtt_adapter_probe_contract
