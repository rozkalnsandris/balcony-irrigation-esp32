#pragma once

#include <cstddef>

namespace command_payload_policy {

constexpr std::size_t kMaxCommandPayloadBytes = 32U;

inline bool isCommandPayloadLengthAllowed(std::size_t length) {
  return length <= kMaxCommandPayloadBytes;
}

}  // namespace command_payload_policy
