#pragma once

namespace ota_error_policy {

inline bool shouldRestartAfterFailure(bool transferStarted) {
  return transferStarted;
}

}  // namespace ota_error_policy
