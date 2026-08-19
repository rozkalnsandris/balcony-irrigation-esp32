#if defined(MQTT_ADAPTER_COMPILE_PROBE)

#include <cstddef>
#include <cstdint>

#include <espMqttClient.h>

#include "mqtt_adapter_probe_contract.h"

namespace mqtt_adapter_probe {

void compileApiProbe() {
  espMqttClient client(espMqttClientTypes::UseInternalTask::NO);

  client
      .setKeepAlive(mqtt_adapter_probe_contract::kKeepAliveSeconds)
      .setTimeout(mqtt_adapter_probe_contract::kOperationTimeoutSeconds)
      .setClientId(mqtt_adapter_probe_contract::kProbeClientId)
      .setCleanSession(true);

  client.onMessage(
      [](const espMqttClientTypes::MessageProperties& properties,
         const char* topic,
         const std::uint8_t* payload,
         std::size_t len,
         std::size_t index,
         std::size_t total) {
        (void)topic;
        (void)payload;
        (void)len;
        (void)index;

        const mqtt_adapter_probe_contract::InboundMetadata metadata{
            properties.qos,
            properties.dup,
            properties.retain,
            properties.packetId,
            total,
        };

        (void)metadata;
      });

  client.onPublish([](std::uint16_t packetId) {
    (void)packetId;
  });

  // Compile/link probe only. Do not call connect(), publish(), subscribe(),
  // loop(), or any other network-producing method here.
}

}  // namespace mqtt_adapter_probe

#endif
