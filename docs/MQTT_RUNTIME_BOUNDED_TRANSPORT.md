# MQTT runtime bounded transport primitives

Issue: `balcony-irrigation-esp32#31`

This stacked source-only phase builds on the green adapter boundary in Draft PR #32.
It still does **not** modify `src/main.cpp` or perform any runtime MQTT traffic.

## Why these primitives are separate

The current firmware relies on two behaviors that should not be silently lost while
moving away from PubSubClient:

1. reconnect is a bounded blocking operation started only while the pump is OFF;
2. Telegram messages are kept in RAM until the current MQTT publish path accepts a
   send, rather than being dropped merely because a library accepted a packet into
   an internal queue.

espMqttClient is internally asynchronous even with `UseInternalTask::NO`. Its QoS0
publish path has no publish-complete callback; `onPublish` is emitted on PUBACK for
QoS1. A safe migration therefore cannot pretend that "packet accepted into outbox"
is identical to the old synchronous QoS0 publish return.

## Bounded connect

`connectBlocking()` keeps the current firmware's reconnect boundary instead of
exposing a new long-lived connecting state to pump logic.

The adapter still uses:

- underlying TCP connection timeout: 1000 ms;
- MQTT acknowledgement/outbox timeout: 2 s;
- keepalive: 30 s.

Because espMqttClient's `connectingMqtt` state does not use the normal outbox timeout
as a complete CONNACK deadline, the adapter adds an explicit total connection budget:

`1000 ms + 2000 ms + 500 ms scheduler margin = 3500 ms`.

The wait uses wrap-safe unsigned `millis()` arithmetic. If the candidate has not
reached either connected or disconnected by the deadline, the adapter requests a
forced disconnect and services cleanup for a separate bounded 250 ms window.
`forceDisconnect()` returns `false` if the client still has not reached the terminal
disconnected state when that cleanup window expires; callers must treat that as a
failed cleanup, not as proof that the transport was force-closed.

This remains acceptable only because the existing reconnect policy starts a new
attempt while the pump is OFF. Connected MQTT servicing stays in the Arduino loop
task so urgent STOP/OFF can arrive with low latency.

## Bounded best-effort QoS0

Normal telemetry, retained status and Home Assistant discovery remain QoS0 in this
migration phase. The adapter allows at most four current outbox packets before a
new best-effort enqueue is rejected.

After an accepted QoS0 publish or subscription, the adapter immediately calls one
candidate `loop()` pass so writable outbox work starts draining instead of allowing
a discovery/status burst to grow without bound.

This is still best-effort. It does not claim that QoS0 has received a broker
acknowledgement.

The later `main.cpp` integration must check/structure the connected-session burst so
one failed queue attempt does not become an unbounded retry loop and urgent STOP
servicing remains interleaved.

## Tracked Telegram publication

`balkons/telegram_out` needs different treatment because the firmware already keeps
an application-level RAM queue for notifications that could not be published.

The canonical RPi bot currently subscribes to `balkons/telegram_out` with Paho's
default subscription QoS0. The migration therefore uses QoS1 **only on the
ESP32 -> broker publisher leg** for the tracked notification slot. Broker -> bot
remains QoS0.

The adapter owns one fixed tracked slot:

- topic storage: 64 bytes;
- payload storage: 896 bytes;
- conservative worst-case MQTT QoS1 PUBLISH size: 969 bytes;
- compile-time `static_assert` requires that worst-case packet size to remain within
  the explicit 1024-byte TX buffer;
- no dynamic allocation in the tracked-copy layer;
- the payload may be accepted while MQTT is disconnected;
- once connected, `pumpTrackedPublish()` sends it with QoS1;
- `onPublish(packetId)` clears the tracked copy only after broker PUBACK;
- if MQTT disconnects before PUBACK, only the in-flight packet ID is cleared; the
  tracked topic/payload remain in adapter RAM and can be retried after reconnect.

This is at-least-once broker delivery. A rare lost PUBACK after broker acceptance can
cause the notification to be retried and therefore duplicated downstream. It is a
known tradeoff for preserving notification loss-resistance and must not be described
as exactly-once Telegram delivery.

The later firmware integration can keep the existing eight-message application
queue for additional notifications. One message at a time can move from that queue
into the adapter's tracked slot; only the adapter copy is removed on broker PUBACK.

## No pump-command QoS change

This tracked QoS1 design is only for firmware-originated `balkons/telegram_out`.
It does not alter Telegram-bot -> ESP32 pump command delivery policy from
`RPi5_main#194`.

In particular, `laist` and `laist_N` remain duplicate-sensitive and are not made
QoS1/retry-safe by this work.

## Still not a runtime migration

The stacked PR for these primitives compiles/tests the adapter only. `src/main.cpp`
remains on PubSubClient. A later deterministic migration-render phase will prove the
small runtime source delta in CI before any repository source switch is proposed.

`CURRENT_PRODUCTION_AUTHORIZATION=NONE`. No OTA/flash, real MQTT publish/probe,
broker or credential mutation, Home Assistant change, or pump command is authorized.
