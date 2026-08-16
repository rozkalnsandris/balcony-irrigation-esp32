#pragma once

#include <cstdint>

namespace network_policy {

inline bool reconnectIntervalElapsed(
    std::uint32_t now,
    std::uint32_t lastAttempt,
    std::uint32_t intervalMs) {
  return static_cast<std::uint32_t>(now - lastAttempt) >= intervalMs;
}

inline bool shouldAttemptMqttReconnect(
    bool wifiConnected,
    bool mqttConnected,
    bool pumpRunning,
    std::uint32_t now,
    std::uint32_t lastAttempt,
    std::uint32_t intervalMs) {
  return wifiConnected &&
         !mqttConnected &&
         !pumpRunning &&
         reconnectIntervalElapsed(now, lastAttempt, intervalMs);
}

}  // namespace network_policy
