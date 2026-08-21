# Deterministic MQTT runtime migration renderer

This document defines the source-only C2 renderer for issue #31. It does **not**
switch the tracked firmware runtime, change `src/main.cpp`, alter PlatformIO, or
perform any device/runtime action.

## Exact source boundary

The renderer is stacked on C1 head:

`a9f55d9926342ec9f27bc3f1aa04edb45fcb0b5b`

The only accepted input is the canonical PubSubClient `src/main.cpp` whose Git
blob is:

`1b4fd87415cd9cce9b24eae2dd1f574aafe35fd7`

That blob comes from main commit:

`da9bdeaf2eba6c8fda02a3eb15f070428c22e595`

The renderer computes the Git blob SHA from the raw input bytes before UTF-8
decoding or transformation. Any byte change therefore fails before matching any
migration anchor.

## R01-R13 transformation contract

`scripts/render_mqtt_runtime_candidate.py` applies the frozen migration regions
in order:

- R01 replaces the PubSubClient include with the runtime adapter include.
- R02 replaces the PubSubClient network/client globals with `MqttRuntimeAdapter`
  and the firmware-owned session-init state.
- R03 replaces immediate/looping Telegram publication with the adapter's fixed
  tracked QoS1 slot plus the existing eight-item application queue.
- R04 changes diagnostic log and retained pump-status publication to bounded
  adapter operations without coupling relay state to MQTT acceptance.
- R05 changes pump-critical network service to `mqtt.service()` while preserving
  the strict `network -> urgent STOP -> pump timer -> watchdog` order and adding
  no reconnect path.
- R06 replaces direct `mqttNet.stop()` with bounded adapter disconnect cleanup.
- R07 replaces the synchronous discovery burst with one-operation-at-a-time
  session initialization.
- R08 replaces the PubSubClient callback with the adapter's validated fixed
  payload callback; urgent STOP is checked before dynamic `String` creation.
- R09 replaces blocking PubSubClient connection/burst initialization with
  `connectBlocking()` and callback-started session initialization.
- R10 replaces direct `mqtt.loop()` service/reconnect logic with adapter
  connected/transitional/disconnected ownership and the existing pump-OFF
  reconnect gate.
- R11 changes moisture telemetry to application-ready bounded QoS0 publication.
- R12 replaces PubSubClient setup setters with adapter configuration/handlers.
- R13 replaces the old Telegram drain loop with one bounded delivery step after
  the application session reaches `Done`.

Each literal anchor, or each exact block start/end anchor pair, must occur exactly
once. Zero or multiple matches are fatal. There is no regular-expression,
whitespace-normalizing, or fuzzy fallback path. Because the whole input blob is
verified first, block interiors cannot drift while still being accepted.

## Session readiness and bounded failure policy

Transport connection and application readiness are deliberately separate.
`mqttSessionReady()` is true only when `MqttSessionInitState::Done` is reached.
The ordered gate is:

1. enqueue `T_PUMP_CMD` QoS1 SUBSCRIBE and wait for its matching broker QoS1
   SUBACK;
2. enqueue `T_CMD` QoS1 SUBSCRIBE and wait for its matching broker QoS1 SUBACK;
3. enqueue retained `balkons/status=online`;
4. attempt sensor discovery one sensor per service call;
5. attempt pump discovery;
6. enqueue retained pump status;
7. perform bounded/noncritical connection and deferred logging steps;
8. transition to `Done`;
9. only after `Done`, permit a new application-level Telegram network enqueue
   and periodic moisture publication.

The tracked SUBACK timeout remains the C1 latched 6500 ms contract. Retained
online and retained pump-status local enqueue steps also use a 6500 ms critical
budget. If a critical step expires while the pump is OFF, the session is torn
down so normal reconnect policy can retry from the first command subscription.
If the pump is running, a healthy connected command path is not deliberately
removed merely to recover initialization.

Home Assistant discovery uses one common 6500 ms noncritical window. If queue
pressure prevents completion inside that window, the remaining discovery work is
skipped/degraded and readiness can continue; there is no infinite discovery spin.
Diagnostic/deferred MQTT log publication is also noncritical.

`Done` is intentionally **not** dependent on Telegram PUBACK. A Telegram message
may be locally staged before readiness, but `serviceTelegramDelivery()` does not
request a new application-level network PUBLISH until `Done`. A QoS1 PUBLISH
already owned by pinned espMqttClient from an earlier session may still be
protocol-retransmitted by the library across reconnect, as defined by C1.

## Safety assertions

Post-render validation rejects the old PubSubClient surface including the old
client/global, setup APIs, direct publish/subscribe calls, old Telegram drain,
and synchronous discovery function. It also requires generated structural
markers for the adapter, session state machine, tracked subscriptions, discovery
helpers and incoming-message handler.

The renderer additionally checks that:

- urgent STOP reaches `requestUrgentPumpStop(fromHA)` before `String message` is
  constructed;
- the physical `digitalWrite(RELAY_PIN, RELAY_OFF)` safety marker survives;
- the 180 second maximum pump-session constant survives;
- the command payload bound remains tied to
  `command_payload_policy::kMaxCommandPayloadBytes`;
- source ordering is pump SUBACK gate -> command SUBACK gate -> retained online
  -> `Done` -> bounded Telegram service.

The renderer does not invent DUP-based command deduplication. `laist` and
`laist_N` remain duplicate-sensitive.

## Invocation and output ownership

Normal use requires an explicit separate output path:

```sh
python scripts/render_mqtt_runtime_candidate.py \
  src/main.cpp \
  --output /tmp/mqtt-runtime-main.cpp
```

The input path and output path may not resolve to the same file. The renderer
prints the accepted input Git blob and SHA256 of the rendered bytes. Rendering
the same canonical input twice must produce identical bytes and SHA256.

The C2 authored commit keeps tracked `src/main.cpp` unchanged. A later C3 CI
slice may render to a temporary path, inspect the diff/structure, temporarily
compile that candidate under a dedicated environment, restore canonical source,
and require a clean Git diff. C2 itself adds no CI workflow or PlatformIO change.

## Tests

`scripts/test_render_mqtt_runtime_candidate.py` uses only the Python standard
library. It covers:

- exact R01-R13 manifest;
- wrong Git blob rejection;
- missing/duplicated literal and block anchors;
- deterministic output/hash;
- already-rendered input rejection;
- forbidden residual rejection;
- urgent STOP ordering;
- session ordering;
- refusal to overwrite input;
- explicit-output input preservation;
- canonical repository-source rendering when run from a real repository
  checkout containing `src/main.cpp`.

The test script is intended to be invoked explicitly in C2 review and then wired
into the C3 CI candidate lane.

## Production boundary

`CURRENT_PRODUCTION_AUTHORIZATION=NONE`.

This renderer is source/review tooling only. It authorizes no merge, OTA/flash,
live MQTT traffic, broker/credential mutation, Home Assistant mutation, or pump
command.
