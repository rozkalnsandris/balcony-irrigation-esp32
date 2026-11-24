#include "ha_cutoff_runtime.h"

#if BALCONY_HA_BOUNDED_PROTOCOL
namespace ha_cutoff {

// Non-inline definition keeps Xtensa literal pools in IRAM with the callback.
// See espressif/esp-idf#16989. Do not remove IRAM_ATTR to work around linking.
bool IRAM_ATTR Guard::onAlarm(gptimer_handle_t,
                             const gptimer_alarm_event_data_t*, void* ctx) {
  auto* self = static_cast<Guard*>(ctx);
  portENTER_CRITICAL_ISR(&self->lock_);
  REG_WRITE(GPIO_OUT_W1TS_REG, 1UL << kRelayPin);
  self->state_.alarm();
  portEXIT_CRITICAL_ISR(&self->lock_);
  return false;
}

}  // namespace ha_cutoff
#endif
