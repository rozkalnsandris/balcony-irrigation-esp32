#pragma once

#include <cstdint>

// Physical CD74HC4067 MUX channel indexes are zero-based.
namespace sensor_enable_policy {

constexpr std::uint8_t kSensorCount = 15U;

// Flower 2: unreliable signal. Flowers 14/15: removed.
constexpr bool isEnabled(int channelIndex) {
  return channelIndex >= 0 &&
         channelIndex < kSensorCount &&
         channelIndex != 1 &&
         channelIndex != 13 &&
         channelIndex != 14;
}

constexpr std::uint8_t countEnabled() {
  std::uint8_t count = 0U;
  for (int channel = 0; channel < kSensorCount; ++channel) {
    if (isEnabled(channel)) {
      ++count;
    }
  }
  return count;
}

constexpr std::uint8_t kEnabledSensorCount = countEnabled();
static_assert(kEnabledSensorCount == 12U, "Unexpected active sensor count");

}  // namespace sensor_enable_policy
