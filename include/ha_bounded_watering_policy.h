#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#ifndef BALCONY_HA_BOUNDED_PROTOCOL
#define BALCONY_HA_BOUNDED_PROTOCOL 0
#endif

namespace ha_bounded_watering {

static_assert(BALCONY_HA_BOUNDED_PROTOCOL == 0 ||
              BALCONY_HA_BOUNDED_PROTOCOL == 1, "invalid candidate flag");
constexpr bool kEnabled = BALCONY_HA_BOUNDED_PROTOCOL == 1;
constexpr std::uint32_t kCycleSeconds = 45U;
constexpr std::uint32_t kCycleMs = kCycleSeconds * 1000U;
constexpr std::uint32_t kTicketTtlMs = 10000U;
constexpr std::uint32_t kMaxQueuedMs = 5000U;
constexpr std::size_t kRequestChars = 16U;
constexpr std::size_t kTokenChars = 24U;

enum class Kind : std::uint8_t { invalid, ticket, start };

inline bool hexText(const char* text, std::size_t length) {
  if (text == nullptr) return false;
  for (std::size_t i = 0; i < length; ++i) {
    const char c = text[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
  }
  return true;
}

// Exact length, topic and bytes; never normalize new protocol payloads.
inline Kind decode(bool fromHA, bool retained, const char* data,
                   std::size_t length) {
  if (!fromHA || retained || data == nullptr) return Kind::invalid;
  if (length == 9U + kRequestChars &&
      std::memcmp(data, "ticket45:", 9U) == 0 &&
      hexText(data + 9U, kRequestChars)) return Kind::ticket;
  if (length == 8U + kTokenChars &&
      std::memcmp(data, "start45:", 8U) == 0 &&
      hexText(data + 8U, kTokenChars)) return Kind::start;
  return Kind::invalid;
}

inline bool elapsed(std::uint32_t now, std::uint32_t since,
                    std::uint32_t duration) {
  return static_cast<std::uint32_t>(now - since) >= duration;
}

inline bool queueFresh(std::uint32_t now, std::uint32_t received,
                       std::uint32_t queuedEpoch, std::uint32_t epoch) {
  return queuedEpoch == epoch && !elapsed(now, received, kMaxQueuedMs);
}

// Record before scanning, including failed/canceled scans. This bounded FIFO
// survives ticket invalidation, expiry, STOP, OTA and MQTT session changes in
// the same boot. It is not a persistent or global replay database.
class RecentRequests {
 public:
  static constexpr std::size_t kCapacity = 8U;

  bool admit(const char* request) {
    if (request == nullptr || std::strlen(request) != kRequestChars ||
        !hexText(request, kRequestChars)) return false;
    for (const auto& seen : ids_) {
      if (std::strcmp(seen, request) == 0) return false;
    }
    std::memcpy(ids_[next_], request, kRequestChars + 1U);
    next_ = (next_ + 1U) % kCapacity;
    return true;
  }

 private:
  char ids_[kCapacity][kRequestChars + 1U] = {};
  std::size_t next_ = 0U;
};

// Whole-record transfer is shared with native tests: no individual metadata
// field may be lost when a queue entry is consumed.
template <typename Command>
inline void takeQueuedCommand(Command& out, Command& slot) {
  out = slot;
  slot.payload = "";
}

class TicketGate {
 public:
  void invalidate() { state_ = State::empty; }

  void expire(std::uint32_t now) {
    if (state_ != State::empty && elapsed(now, issuedAt_, kTicketTtlMs)) {
      invalidate();
    }
  }

  // The caller supplies a unique token (boot entropy + nonwrapping sequence).
  // A new offer revokes any old offer. Never replace a claimed ticket.
  bool issue(const char* token, std::uint32_t now) {
    expire(now);
    if (state_ == State::claimed) return false;
    invalidate();
    if (token == nullptr || std::strlen(token) != kTokenChars ||
        !hexText(token, kTokenChars) ||
        std::strcmp(token, token_) == 0) return false;
    std::memcpy(token_, token, kTokenChars + 1U);
    issuedAt_ = now;
    state_ = State::offered;
    return true;
  }

  bool claim(const char* token, std::uint32_t now) {
    expire(now);
    if (state_ != State::offered || token == nullptr ||
        std::strcmp(token, token_) != 0) return false;
    // Consume before any network operation or GPIO write.
    state_ = State::claimed;
    return true;
  }

  bool claimFresh(std::uint32_t now) const {
    return state_ == State::claimed &&
           !elapsed(now, issuedAt_, kTicketTtlMs);
  }

  // Conservative deadline: 45 s from offer issuance, earlier than dispatch.
  // Called immediately before GPIO ON; network preparation cannot extend it.
  std::uint32_t remainingMs(std::uint32_t now) const {
    if (!claimFresh(now)) return 0U;
    return kCycleMs - static_cast<std::uint32_t>(now - issuedAt_);
  }

  const char* token() const { return token_; }

 private:
  enum class State : std::uint8_t { empty, offered, claimed };
  State state_ = State::empty;
  std::uint32_t issuedAt_ = 0U;
  char token_[kTokenChars + 1U] = {};
};

}  // namespace ha_bounded_watering
