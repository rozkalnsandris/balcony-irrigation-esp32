#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace command_safety {

inline bool isUrgentStop(bool fromHA, const char* payload) {
  if (payload == nullptr) {
    return false;
  }

  if (fromHA) {
    return std::strcmp(payload, "OFF") == 0;
  }

  return std::strcmp(payload, "stop") == 0;
}

inline bool isPumpStartCommand(bool fromHA, const char* payload) {
  if (payload == nullptr) {
    return false;
  }

  if (fromHA) {
    return std::strcmp(payload, "ON") == 0;
  }

  if (std::strcmp(payload, "laist") == 0) {
    return true;
  }

  return std::strncmp(payload, "laist_", 6) == 0;
}

inline bool shouldSuppressQueuedPumpStart(
    std::uint32_t queuedStopEpoch,
    std::uint32_t currentStopEpoch,
    bool fromHA,
    const char* payload) {
  return queuedStopEpoch != currentStopEpoch &&
         isPumpStartCommand(fromHA, payload);
}

inline bool shouldDeferQueuedPumpStartForNetwork(
    bool pumpRunning,
    bool mqttReady,
    std::size_t pendingPackets,
    bool trackedPublishBusy,
    bool fromHA,
    const char* payload) {
  return !pumpRunning &&
         (!mqttReady || pendingPackets > 0U || trackedPublishBusy) &&
         isPumpStartCommand(fromHA, payload);
}

}  // namespace command_safety
