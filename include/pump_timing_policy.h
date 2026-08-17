#pragma once

#include <cstdint>

namespace pump_timing_policy {

inline std::uint32_t normalizeRequestedSeconds(
    std::uint32_t requestedSeconds,
    std::uint32_t defaultSeconds,
    std::uint32_t maxSeconds) {
  if (requestedSeconds == 0U) {
    return defaultSeconds > maxSeconds ? maxSeconds : defaultSeconds;
  }

  return requestedSeconds > maxSeconds ? maxSeconds : requestedSeconds;
}

struct ExtensionResult {
  std::uint32_t plannedDurationMs;
  std::uint32_t addedMs;
};

inline ExtensionResult extendPlannedDuration(
    std::uint32_t currentDurationMs,
    std::uint32_t additionalSeconds,
    std::uint32_t maxSeconds) {
  const std::uint64_t maxDurationMs =
      static_cast<std::uint64_t>(maxSeconds) * 1000ULL;

  const std::uint64_t boundedCurrentMs =
      static_cast<std::uint64_t>(currentDurationMs) > maxDurationMs
          ? maxDurationMs
          : static_cast<std::uint64_t>(currentDurationMs);

  const std::uint64_t requestedDurationMs =
      boundedCurrentMs +
      static_cast<std::uint64_t>(additionalSeconds) * 1000ULL;

  const std::uint64_t boundedDurationMs =
      requestedDurationMs > maxDurationMs
          ? maxDurationMs
          : requestedDurationMs;

  return {
      static_cast<std::uint32_t>(boundedDurationMs),
      static_cast<std::uint32_t>(boundedDurationMs - boundedCurrentMs),
  };
}

inline std::uint32_t elapsedMs(
    std::uint32_t now,
    std::uint32_t sessionStartMs) {
  return static_cast<std::uint32_t>(now - sessionStartMs);
}

inline bool shouldStopForElapsedTime(
    std::uint32_t now,
    std::uint32_t sessionStartMs,
    std::uint32_t plannedDurationMs,
    std::uint32_t hardLimitMs) {
  const std::uint32_t elapsed = elapsedMs(now, sessionStartMs);
  return elapsed >= plannedDurationMs || elapsed >= hardLimitMs;
}

}  // namespace pump_timing_policy
