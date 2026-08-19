# MQTT runtime adapter design

Issue: `balcony-irrigation-esp32#31`

## Status

This document describes the first source-only adapter boundary selected after the
MQTT client feasibility comparison in #28.

Selected candidate:

- `bertmelis/espMqttClient @ 1.7.3`
- release/version commit:
  `decfa510584c88cd1f0d24e591b92ac37d5bae2e`

Strategic fallback: Espressif ESP-MQTT, proven available from the same Arduino-only
pioarduino toolchain in Draft PR #30.

This phase does **not** replace the production PubSubClient path in `src/main.cpp`.
It adds and compiles the adapter boundary independently before runtime integration.

## Why espMqttClient is first

Both serious candidates compile on the exact pinned platform. ESP-MQTT has cleaner
public timeout/reconnect configuration but creates a dedicated MQTT FreeRTOS task.
The current irrigation firmware deliberately keeps urgent pump STOP and MQTT input
handling in the Arduino loop task. Moving that safety state across tasks would add
new synchronization and latency risks.

espMqttClient can be constructed with `UseInternalTask::NO`. Its `loop()` then
advances the MQTT state machine only when the application calls it. That keeps the
existing single-task execution model and allows the current reconnect policy to
remain the outer decision gate.

## Timing contract

The current firmware distinguishes two layers and the adapter preserves that
separation:

- underlying Arduino `NetworkClient` TCP connection timeout: **1000 ms**;
- MQTT/outbox acknowledgement timeout: **2 seconds**;
- MQTT keepalive: **30 seconds**.

espMqttClient does not expose the underlying TCP connection timeout publicly. The
adapter therefore uses one narrow subclass-only access to its protected
`ClientSync`, then calls the pinned Arduino-ESP32 `NetworkClient` public
`setConnectionTimeout(1000)` API.

This is intentional implementation coupling. The candidate version is pinned and CI
must compile this exact boundary. If a future candidate update changes the protected
layout, the build must fail instead of silently falling back to an unbounded/default
connection timeout.

## Compile-time candidate policy

The adapter build explicitly sets:

- `EMC_ALLOW_NOT_CONNECTED_PUBLISH=0`
- `EMC_RX_BUFFER_SIZE=1024`
- `EMC_TX_BUFFER_SIZE=1024`

The first flag preserves current PubSubClient semantics: application publishes must
not be queued while MQTT is disconnected/connecting. The RX/TX values keep the
candidate buffers aligned with the current 1024-byte MQTT buffer policy rather than
silently adopting a different default.

The wrapper also checks `connected()` before every publish/subscribe even with the
compile-time guard, so the safety contract is visible in our own source.

## Connection state model

PubSubClient's current `connect()` path is synchronous. espMqttClient is not:
`connect()` starts a state transition and later `loop()` calls advance TCP/MQTT
connection setup until `onConnect` is emitted.

The runtime integration phase therefore must separate:

1. **reconnect decision** — existing policy decides whether a new attempt may start;
2. **startConnect()** — starts only from fully-disconnected state;
3. **service()** — advances connecting/connected/disconnecting state;
4. **connected event** — marks session initialization pending;
5. **post-service session initialization** — performs subscriptions, retained online
   status, Home Assistant discovery, pump status, logs and deferred Telegram flush
   outside the library callback.

If the pump becomes active while the MQTT client is transitional but not connected,
the transition is aborted before more candidate network work is serviced. A healthy
connected session continues to be serviced while the pump runs so urgent STOP/OFF
can still arrive.

## Inbound command boundary

Only the two command topics are subscribed by the current firmware. The adapter
therefore treats every inbound payload as a bounded command envelope before handing
it to `main.cpp`.

Required checks before dynamic `String` construction:

- total payload <= 32 bytes;
- QoS is 0 or 1;
- retained flag is false;
- chunks are contiguous (`index` matches the next expected byte);
- `total` remains stable across chunks;
- QoS/DUP/retain/packetId metadata remains stable across chunks;
- a fragment may not overrun `total`.

A fixed 32-byte adapter buffer assembles the complete command. `main.cpp` receives a
callback only after a complete valid message is available. Rejected messages never
reach its command parser.

DUP and packetId are surfaced as metadata. They do not create a new application
idempotency rule. In particular, duplicate-sensitive `laist` and `laist_N` commands
must not be silently suppressed or retried under this migration.

## Urgent STOP invariant

The runtime integration must preserve the existing callback ordering:

1. complete bounded command is delivered from espMqttClient's `onMessage` while
   `service()` is executing in the Arduino loop task;
2. firmware identifies `stop` / HA `OFF`;
3. relay GPIO is driven OFF immediately in that callback path;
4. no MQTT publish/log is performed re-entrantly from urgent STOP handling;
5. after the MQTT service call returns, normal state/log/retained-status
   reconciliation runs before ordinary queued command work.

The adapter does not own relay or pump state.

## Outgoing compatibility

The later runtime integration must preserve topic and retain behavior for:

- `balkons/status` last-will `offline` and retained `online`;
- `balkons/sukna/stends` retained pump state;
- Home Assistant discovery retained config;
- `balkons/log` non-retained logs;
- `balkons/telegram_out` non-retained messages;
- moisture telemetry non-retained messages.

This migration does not change the Telegram bot publisher's command QoS policy from
`RPi5_main#194`.

## Phase-A CI gate

The current phase deliberately leaves `src/main.cpp` on PubSubClient and adds a
separate `esp32_mqtt_runtime_adapter` environment. That environment compiles the
real adapter against espMqttClient 1.7.3 while the normal `esp32_ci` build proves the
existing firmware remains unchanged.

Native tests cover the library-independent policy: envelope rejection, fragment
sequence, transition-abort decision, timeout/buffer constants and DUP metadata
representation.

A green Phase-A PR proves only that the adapter boundary is buildable and its pure
policy is tested. Runtime integration is a later source-only phase under #31.

## Production boundary

`CURRENT_PRODUCTION_AUTHORIZATION=NONE`.

No source-only PR under #31 authorizes OTA/flash, real MQTT publish/probe, broker or
credential mutation, Home Assistant mutation, or a pump command. Production work
requires a later exact-source/artifact preflight and separate one-shot owner
authorization.
