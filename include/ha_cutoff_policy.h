#pragma once

#include <cstdint>
#include "ha_bounded_watering_policy.h"

namespace ha_cutoff {

// Engineering margin for clock rounding, setup, interrupt latency and relay
// release; not a measured worst-case guarantee. No late start near the alarm.
constexpr std::uint32_t kMarginMs = 1000U;
constexpr std::uint32_t kMinStartLeadMs = 100U;
constexpr std::uint64_t kTicksPerMs = 1000U;

inline std::uint64_t alarmTicks(std::uint32_t remainingMs) {
  return remainingMs > kMarginMs + kMinStartLeadMs &&
         remainingMs <= ha_bounded_watering::kCycleMs
      ? (remainingMs - kMarginMs) * kTicksPerMs : 0U;
}

inline bool startWindow(std::uint64_t count, std::uint64_t alarm,
                        std::uint32_t now, std::uint32_t sampled) {
  // Refuse if preparation/readback was delayed, even if the ISR has not run.
  return count < alarm &&
         alarm - count > kMinStartLeadMs * kTicksPerMs &&
         !ha_bounded_watering::elapsed(now, sampled, kMinStartLeadMs);
}

enum class Phase : std::uint8_t { idle, armed, running, canceled, tripped, fault };

// Caller serializes all transitions and GPIO writes with the same short lock.
// Inline ISR transitions avoid a call to flash-resident policy code.
class State {
 public:
  bool idle() const { return phase_ == Phase::idle; }
  bool running() const { return phase_ == Phase::running; }
  bool tripped() const { return phase_ == Phase::tripped; }
  bool faulted() const { return phase_ == Phase::fault; }
  bool arm() {
    if (!idle()) return false;
    phase_ = Phase::armed;
    return true;
  }
  bool start(bool inWindow) {
    if (phase_ != Phase::armed || !inWindow) return false;
    phase_ = Phase::running;
    return true;
  }
  void cancel() {
    if (phase_ == Phase::armed || phase_ == Phase::running)
      phase_ = Phase::canceled;
  }
  void fault() { phase_ = Phase::fault; }
  __attribute__((always_inline)) inline void alarm() {
    if (phase_ == Phase::armed || phase_ == Phase::running ||
        phase_ == Phase::canceled) {
      phase_ = Phase::tripped;
    } else {
      phase_ = Phase::fault;
    }
  }
  // Only after the one-shot callback completed, on its owning core, and
  // main-loop OFF/ACK reconciliation. An early OFF does not permit reuse.
  void reconcile() {
    if (tripped()) phase_ = Phase::idle;
  }
 private:
  Phase phase_ = Phase::idle;
};

}  // namespace ha_cutoff
