#pragma once

#include <cstdint>

namespace command_parse_policy {

inline bool parsePositiveDecimal(
    const char* text,
    std::uint32_t maxValue,
    std::uint32_t& valueOut) {
  valueOut = 0U;

  if (text == nullptr || *text == '\0' || maxValue == 0U) {
    return false;
  }

  std::uint32_t value = 0U;

  for (const char* cursor = text; *cursor != '\0'; ++cursor) {
    if (*cursor < '0' || *cursor > '9') {
      return false;
    }

    const std::uint32_t digit =
        static_cast<std::uint32_t>(*cursor - '0');

    if (digit > maxValue || value > (maxValue - digit) / 10U) {
      return false;
    }

    value = value * 10U + digit;
  }

  if (value == 0U) {
    return false;
  }

  valueOut = value;
  return true;
}

}  // namespace command_parse_policy
