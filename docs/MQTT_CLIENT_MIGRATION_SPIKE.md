# MQTT client migration compile spike

Issue: `balcony-irrigation-esp32#28`

## Status

This is a **source-only compatibility spike**. It does not migrate the production
firmware away from PubSubClient and it authorizes no OTA/flash, MQTT traffic,
broker mutation, credential change, Home Assistant change, or pump command.

`src/main.cpp` remains on the existing PubSubClient runtime path. The default
`esp32_ci` environment also remains unchanged. A dedicated PlatformIO environment
only compiles a candidate adapter/API probe in CI.

## Why this spike exists

The firmware currently pins `knolleary/PubSubClient @ 2.8.0`. Upstream now marks
PubSubClient as not maintained. Issue #28 therefore compares maintained MQTT
clients without mixing a library migration into pump-safety or protocol changes.

The first candidate selected after the corrected Phase B2 review is:

- project: `bertmelis/espMqttClient`
- version: `1.7.3`
- release/version commit:
  `decfa510584c88cd1f0d24e591b92ac37d5bae2e`
- release date: 2026-06-22
- framework/platform metadata: Arduino + ESP32
- license: MIT

The earlier tentative preference for ArduinoMqttClient was superseded before any
branch was written after a deeper maintenance check showed its latest release is
0.1.8 from 2024-01-31.

## Candidate properties being proved

The release source exposes the properties needed for a safer future adapter:

- `MessageProperties.qos`
- `MessageProperties.dup`
- `MessageProperties.retain`
- `MessageProperties.packetId`
- payload `len`, `index`, and `total`
- outgoing publish acknowledgement callback with packet ID
- an ESP32 constructor using `UseInternalTask::NO`, allowing the application to
  own calls to the MQTT loop instead of accepting an autonomous internal task

The compile probe intentionally selects `UseInternalTask::NO`. That does **not**
yet prove a full migration is safe; it only proves the exact candidate API can be
compiled against the repository's pinned pioarduino platform.

## Probe environment

`esp32_mqtt_adapter_probe` extends the existing ESP32 base environment and adds
`bertmelis/espMqttClient @ 1.7.3` alongside PubSubClient. PubSubClient is retained
because `src/main.cpp` is intentionally not migrated by this spike.

The compile-only source `src/mqtt_adapter_probe.cpp`:

- creates an `espMqttClient` with `UseInternalTask::NO`;
- explicitly sets a 1-second candidate MQTT operation timeout;
- explicitly sets a 30-second keepalive;
- uses a deterministic non-secret probe ClientId;
- type-checks incoming message metadata and outgoing publish acknowledgement;
- never calls `connect()`, `publish()`, `subscribe()`, `loop()`, or any other
  network-producing operation.

The function itself is not called by firmware. It exists only to force compile/link
validation of the candidate API.

## Pure adapter boundary

`include/mqtt_adapter_probe_contract.h` is deliberately independent of the MQTT
library. It models only the metadata required by our safety contract:

- QoS
- duplicate flag
- retained flag
- packet ID
- total payload bytes

The probe contract currently requires:

1. command payload at or below the existing 32-byte limit;
2. inbound QoS 0 or 1 only;
3. retained command messages rejected fail-closed;
4. duplicate metadata surfaced to higher-level command safety rather than silently
   rejected or treated as proof of a repeated command;
5. a deterministic probe ClientId that stays within the MQTT 3.1.1 mandatory
   alphanumeric 1-23 byte compatibility set.

This spike does not introduce application-level duplicate suppression. Pump-start
commands (`laist`, `laist_N`) remain duplicate-sensitive in the existing protocol,
so any future deduplication/idempotency policy needs a separately reviewed command
contract.

## Existing safety policies that must remain independent

A future migration is acceptable only if it preserves the already tracked policies:

- urgent STOP/OFF drives the physical relay OFF before ordinary queued work;
- a newer stop epoch suppresses stale queued pump-start commands;
- MQTT command payloads are bounded before dynamic `String` construction;
- a new MQTT reconnect is not started while the pump is running;
- the absolute pump hard limit remains 180 seconds;
- retained command messages are never accepted.

The compile spike changes none of those runtime paths.

## CI gate

The existing `firmware-ci` workflow remains authoritative. It already performs:

- public-repository safety check;
- native command-safety tests;
- normal ESP32 CI build;
- bounded Cppcheck analysis;
- OTA environment **build only**.

This spike adds one build-only command after the non-secret CI configuration is
created:

`pio run -e esp32_mqtt_adapter_probe`

No upload target is invoked.

Native tests additionally prove the pure candidate contract for payload bounds,
retained-message rejection, supported QoS, duplicate metadata visibility, ClientId
compatibility, and explicit timeout/keepalive constants.

## Success and next gate

A green spike means only:

- espMqttClient 1.7.3 resolves and compiles on the exact pinned platform;
- the API shape expected by the adapter is valid;
- the pure candidate contract passes native tests;
- existing firmware build/static-analysis gates still pass.

It does **not** approve a runtime migration. After a successful spike, issue #28
must decide whether to prepare a separate migration PR or to run an ESP-MQTT
feasibility spike first. Any production OTA/flash remains separately owner-gated.
