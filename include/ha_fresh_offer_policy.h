#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "ha_bounded_watering_policy.h"
#include "sensor_enable_policy.h"

namespace ha_fresh_offer {

constexpr std::uint32_t kScanBudgetMs = 2000U;
constexpr std::size_t kFirmwareChars = 40U;
constexpr std::size_t kSampleCount = sensor_enable_policy::kEnabledSensorCount;
// offer2:<16 request>:<24 ticket>:<40 revision>:<10 codes>:off
constexpr std::size_t kOfferChars = 7U + 16U + 1U + 24U + 1U +
                                    kFirmwareChars + 1U + kSampleCount + 4U;
static_assert(kOfferChars < 128U, "bounded offer must fit reply buffer");

inline bool exactHex(const char* text, std::size_t length) {
  return text != nullptr && std::strlen(text) == length &&
         ha_bounded_watering::hexText(text, length);
}

inline char sampleCode(const char* category) {
  if (category == nullptr) return '\0';
  if (std::strcmp(category, "sauss") == 0) return 'D';
  if (std::strcmp(category, "mitrs") == 0) return 'W';
  if (std::strcmp(category, "videjs") == 0) return 'M';
  if (category[0] == '\0') return 'U';  // existing ADC invalid/unavailable
  return '\0';
}

inline bool scanCurrent(std::uint32_t now, std::uint32_t started,
                        std::uint32_t epoch, std::uint32_t currentEpoch,
                        bool readyAndOff) {
  return readyAndOff && epoch == currentEpoch &&
         !ha_bounded_watering::elapsed(now, started, kScanBudgetMs);
}

class Scan {
 public:
  bool append(int channel, const char* category) {
    while (next_ < sensor_enable_policy::kSensorCount &&
           !sensor_enable_policy::isEnabled(next_)) ++next_;
    const char code = sampleCode(category);
    if (!valid_ || channel != next_ || count_ >= kSampleCount || code == '\0') {
      valid_ = false;
      return false;
    }
    codes_[count_++] = code;
    ++next_;
    return true;
  }

  bool complete() const { return valid_ && count_ == kSampleCount; }
  const char* codes() const { return codes_; }

 private:
  bool valid_ = true;
  int next_ = 0;
  std::size_t count_ = 0;
  char codes_[kSampleCount + 1U] = {};
};

// One atomic offer: never issue a separate cached telemetry message. A clean
// revision is required; a dirty/unknown build cannot issue a candidate offer.
inline bool formatOffer(char* out, std::size_t capacity, const char* request,
                        const char* token, const char* revision, const Scan& scan) {
  if (out == nullptr || capacity == 0U) return false;
  out[0] = '\0';
  if (capacity <= kOfferChars || !scan.complete() ||
      !exactHex(request, ha_bounded_watering::kRequestChars) ||
      !exactHex(token, ha_bounded_watering::kTokenChars) ||
      !exactHex(revision, kFirmwareChars)) return false;
  const int written = std::snprintf(out, capacity, "offer2:%s:%s:%s:%s:off",
                                    request, token, revision, scan.codes());
  if (written != static_cast<int>(kOfferChars)) {
    out[0] = '\0';
    return false;
  }
  return true;
}

}  // namespace ha_fresh_offer
