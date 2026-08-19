#if defined(ESP_MQTT_COMPILE_PROBE)

#include <cstdint>

#include "mqtt_client.h"

namespace esp_mqtt_compile_probe {

constexpr int kNetworkTimeoutMs = 1000;
constexpr int kKeepAliveSeconds = 30;

void compilePublicConfigProbe() {
  esp_mqtt_client_config_t config{};

  config.network.timeout_ms = kNetworkTimeoutMs;
  config.network.disable_auto_reconnect = true;
  config.session.keepalive = kKeepAliveSeconds;
  config.session.protocol_ver = MQTT_PROTOCOL_V_3_1_1;

  (void)config;
}

void compileEventMetadataProbe(esp_mqtt_event_handle_t event) {
  if (event == nullptr) {
    return;
  }

  const int messageId = event->msg_id;
  const int qos = event->qos;
  const bool duplicate = event->dup;
  const bool retained = event->retain;
  const int dataLength = event->data_len;
  const int totalDataLength = event->total_data_len;
  const int currentDataOffset = event->current_data_offset;

  (void)messageId;
  (void)qos;
  (void)duplicate;
  (void)retained;
  (void)dataLength;
  (void)totalDataLength;
  (void)currentDataOffset;
}

// Compile/type probe only. Deliberately no esp_mqtt_client_init(), start(),
// reconnect(), publish(), subscribe(), enqueue(), or other network/runtime call.

}  // namespace esp_mqtt_compile_probe

#endif
