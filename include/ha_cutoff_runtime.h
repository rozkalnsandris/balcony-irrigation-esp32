#pragma once

#include "ha_cutoff_policy.h"

#if BALCONY_HA_BOUNDED_PROTOCOL
#include <Arduino.h>
#include "driver/gptimer.h"
#include "esp_attr.h"
#include "esp_idf_version.h"
#include "sdkconfig.h"
#include "soc/gpio_reg.h"
#include "soc/soc.h"

namespace ha_cutoff {

// Check the actual prebuilt SDK, not command-line defines pretending to change
// its configuration. An unsupported SDK must never authorize candidate ON.
#if defined(CONFIG_GPTIMER_ISR_CACHE_SAFE) && CONFIG_GPTIMER_ISR_CACHE_SAFE && \
    defined(CONFIG_GPTIMER_ISR_HANDLER_IN_IRAM) && CONFIG_GPTIMER_ISR_HANDLER_IN_IRAM
constexpr bool kCacheSafeSdk = true;
#pragma message("HA_CUTOFF_SDK_CACHE_SAFE=YES")
#else
constexpr bool kCacheSafeSdk = false;
#pragma message("HA_CUTOFF_SDK_CACHE_SAFE=NO (candidate start inhibited)")
#endif

static_assert(CONFIG_IDF_TARGET_ESP32, "GPIO register path is ESP32-specific");
static_assert(ESP_IDF_VERSION_MAJOR == 5 && ESP_IDF_VERSION_MINOR == 5,
              "review GPTimer contract before changing SDK series");

// The timer is allocated and used by the Arduino loop task on one core.
// The ISR is allocated on that core. No driver call under the application
// spinlock, and no driver call, logging, heap or network in the callback.
class Guard {
 public:
  static constexpr uint8_t kRelayPin = 26U;
  static constexpr uint32_t kResolutionHz = 1000000U;

  // Preload the active-low OFF latch before output mode is selected.
  static void preloadOff() { REG_WRITE(GPIO_OUT_W1TS_REG, 1UL << kRelayPin); }

  bool begin() {
    if (!kCacheSafeSdk) return false;
    if (timer_ != nullptr) return false;
    gptimer_config_t config{};
    config.clk_src = GPTIMER_CLK_SRC_DEFAULT;
    config.direction = GPTIMER_COUNT_UP;
    config.resolution_hz = kResolutionHz;
    if (gptimer_new_timer(&config, &timer_) != ESP_OK) return fail();
    uint32_t resolution = 0;
    if (gptimer_get_resolution(timer_, &resolution) != ESP_OK ||
        resolution != kResolutionHz) return fail();
    gptimer_event_callbacks_t callbacks{};
    callbacks.on_alarm = onAlarm;
    if (gptimer_register_event_callbacks(timer_, &callbacks, this) != ESP_OK ||
        gptimer_enable(timer_) != ESP_OK ||
        gptimer_start(timer_) != ESP_OK) return fail();
    // Free-running count; the one-shot alarm is not reprogrammed until
    // its previous callback has executed and OFF has been reconciled.
    configured_ = true;
    return true;
  }

  bool ready() {
    portENTER_CRITICAL(&lock_);
    const bool result = configured_ && state_.idle();
    portEXIT_CRITICAL(&lock_);
    return result;
  }

  bool sampleCount(uint64_t& count) {
    if (!ready()) return false;
    if (gptimer_get_raw_count(timer_, &count) != ESP_OK) return fail();
    return true;
  }

  bool arm(uint64_t count, uint32_t remainingMs, uint32_t sampled) {
    const uint64_t ticks = alarmTicks(remainingMs);
    // ESP32 Timer Group has a 64-bit counter; use the conservative 54-bit
    // minimum supported by the driver contract and refuse near wrap.
    if (ticks == 0 || count >= (1ULL << 54) - ticks) return false;
    portENTER_CRITICAL(&lock_);
    const bool accepted = configured_ && state_.arm();
    portEXIT_CRITICAL(&lock_);
    if (!accepted) return false;
    alarm_ = count + ticks;
    sampled_ = sampled;
    gptimer_alarm_config_t alarm{};
    alarm.alarm_count = alarm_;
    alarm.flags.auto_reload_on_alarm = false;
    if (gptimer_set_alarm_action(timer_, &alarm) != ESP_OK) return fail();
    return true;
  }

  bool start() {
    uint64_t count = 0;
    if (gptimer_get_raw_count(timer_, &count) != ESP_OK) return fail();
    portENTER_CRITICAL(&lock_);
    const bool accepted = state_.start(startWindow(count, alarm_, millis(), sampled_));
    if (accepted) {
      REG_WRITE(GPIO_OUT_W1TC_REG, 1UL << kRelayPin);
    } else {
      REG_WRITE(GPIO_OUT_W1TS_REG, 1UL << kRelayPin);
      state_.cancel();
    }
    portEXIT_CRITICAL(&lock_);
    return accepted;
  }

  void off() {
    portENTER_CRITICAL(&lock_);
    REG_WRITE(GPIO_OUT_W1TS_REG, 1UL << kRelayPin);
    state_.cancel();
    portEXIT_CRITICAL(&lock_);
  }

  bool running() {
    portENTER_CRITICAL(&lock_);
    const bool result = state_.running();
    portEXIT_CRITICAL(&lock_);
    return result;
  }

  bool needsReconciliation() {
    portENTER_CRITICAL(&lock_);
    const bool result = state_.tripped() || state_.faulted();
    portEXIT_CRITICAL(&lock_);
    return result;
  }

  bool reconcile() {
    portENTER_CRITICAL(&lock_);
    const bool completed = state_.tripped();
    state_.reconcile();
    portEXIT_CRITICAL(&lock_);
    return completed;
  }

 private:
  // IRAM code, DRAM instance (declared in main.cpp), direct GPIO OFF.
  static bool IRAM_ATTR onAlarm(gptimer_handle_t,
                                const gptimer_alarm_event_data_t*, void* ctx) {
    auto* self = static_cast<Guard*>(ctx);
    portENTER_CRITICAL_ISR(&self->lock_);
    REG_WRITE(GPIO_OUT_W1TS_REG, 1UL << kRelayPin);
    self->state_.alarm();
    portEXIT_CRITICAL_ISR(&self->lock_);
    return false;
  }

  bool fail() {
    portENTER_CRITICAL(&lock_);
    REG_WRITE(GPIO_OUT_W1TS_REG, 1UL << kRelayPin);
    state_.fault();
    configured_ = false;
    portEXIT_CRITICAL(&lock_);
    return false;
  }

  portMUX_TYPE lock_ = portMUX_INITIALIZER_UNLOCKED;
  State state_{};
  gptimer_handle_t timer_ = nullptr;
  bool configured_ = false;
  uint64_t alarm_ = 0;
  uint32_t sampled_ = 0;
};

}  // namespace ha_cutoff
#endif
