# ESP-MQTT Arduino-only feasibility probe

Issue: `balcony-irrigation-esp32#28`

## Purpose

This is a **source-only compile/link probe** used to compare Espressif ESP-MQTT
with the already green `espMqttClient` feasibility PR #29.

The question is deliberately narrow: on the repository's exact pinned pioarduino
platform (`55.03.311`, Arduino framework), are the ESP-IDF 5.5 ESP-MQTT public
headers, configuration/event types, and precompiled core symbols directly available
without changing the firmware to an ESP-IDF or mixed-framework build?

This probe does not initialize or start an MQTT client and performs no network
operation.

## Exact upstream baseline

The pinned ESP-IDF 5.5.5 tree references ESP-MQTT submodule commit:

`0e9ec171b448db2101ea537300976b7ffe595941`

The matching public `mqtt_client.h` contract provides:

- `esp_mqtt_client_config_t::network.timeout_ms`;
- `esp_mqtt_client_config_t::network.disable_auto_reconnect`;
- `esp_mqtt_client_config_t::session.keepalive`;
- `esp_mqtt_client_config_t::session.protocol_ver`;
- `esp_mqtt_event_t` metadata including `msg_id`, `qos`, `dup`, `retain`,
  `data_len`, `total_data_len`, and `current_data_offset`.

These are attractive for the irrigation safety model because the current firmware
needs an explicit 1000 ms network bound, externally controlled reconnect policy,
and duplicate/retained-message visibility.

## Probe environment

`esp32_esp_mqtt_probe` extends the existing Arduino-only `esp32_base` environment.
It does not add a new MQTT dependency and does not change `src/main.cpp`.

The compile-only source `src/esp_mqtt_compile_probe.cpp`:

- includes the public `mqtt_client.h`;
- type-checks `network.timeout_ms = 1000`;
- type-checks `network.disable_auto_reconnect = true`;
- type-checks keepalive 30 s and MQTT 3.1.1 protocol selection;
- reads the inbound message metadata needed by a future adapter;
- never calls `esp_mqtt_client_init`, `start`, `reconnect`, `publish`,
  `subscribe`, `enqueue`, or any other network/runtime API.

The functions are not called by the firmware. They exist only to force the exact
header/API contract through the pinned CI toolchain.

## Link-only symbol gate

A successful header compile alone does not prove that the Arduino-only framework
package also exposes the precompiled ESP-MQTT implementation at link time.

The probe environment therefore additionally passes linker undefined-symbol roots:

- `esp_mqtt_client_init`
- `esp_mqtt_client_start`

using `-Wl,-u,<symbol>`.

This forces the final firmware link to resolve those ESP-MQTT symbols while still
making **no function call** and creating no MQTT client. If the symbols are not in
the Arduino-only precompiled libraries, CI must fail at link time instead of giving
us a false-positive header-only result.

## Interpretation

If both compile and link gates pass, the current Arduino-only build class directly
contains the ESP-MQTT public API and binary implementation. A later, separately
reviewed adapter spike can then evaluate initialization/event/task integration
without first changing framework mode.

If the link gate fails, that is also useful evidence: issue #28 will preserve the
header-pass/link-fail distinction before testing an ESP-IDF/mixed-framework path.
We will not hide a failed minimal integration by silently broadening the build
architecture in the same commit.

## Runtime architecture remains unproven

Exact ESP-MQTT 5.5.5 source review shows that `esp_mqtt_client_start()` creates a
dedicated FreeRTOS MQTT task. That is a materially different concurrency model from
the current PubSubClient callback driven synchronously by the Arduino loop task.

Therefore even a fully green compile/link probe does **not** approve a migration.
A future runtime design must explicitly protect the urgent STOP/stop-epoch/shared
pump state across task boundaries and measure STOP latency.

## Safety boundary

This probe changes no production runtime path. It performs no OTA/flash, MQTT
publish/probe, broker/credential mutation, Home Assistant change, or pump command.
`CURRENT_PRODUCTION_AUTHORIZATION=NONE`.
