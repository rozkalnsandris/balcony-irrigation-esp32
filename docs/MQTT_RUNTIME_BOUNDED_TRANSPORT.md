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

## Broker-confirmed command subscriptions

The future connected-session initializer must not publish retained `online` merely
because the MQTT transport reached CONNACK. Both command subscriptions must first be
confirmed by broker SUBACK.

The adapter therefore owns one sequential tracked-subscription slot:

- the requested QoS is fixed to QoS1;
- the SUBSCRIBE must return a nonzero packet ID before the tracker becomes active;
- only the matching packet ID may resolve the tracker;
- the SUBACK payload must contain exactly one return code;
- only granted QoS1 (`0x01`) is accepted;
- granted QoS0, granted QoS2, broker failure (`0x80`), unknown values, null payload,
  or any return-code count other than one are rejected fail-closed;
- an unrelated/stale packet ID is ignored;
- the overall SUBACK budget is 6500 ms and uses wrap-safe unsigned time arithmetic.

The 6500 ms deadline is **latched**. At 6499 ms a matching QoS1 SUBACK may still
succeed. At 6500 ms or later the tracker becomes rejected with `timedOut`, and a late
SUBACK cannot turn that timed-out operation back into success.

An ordinary transport disconnect resets the tracked-subscription slot because pinned
espMqttClient 1.7.3 removes SUBSCRIBE packets from its outbox during disconnect
cleanup. A later session initializer therefore restarts from the first command topic.

The later runtime phase must keep transport-connected, session-initializing, and
application-ready states separate. Retained `balkons/status=online` is forbidden
until both command subscriptions have independently completed this exact QoS1 SUBACK
gate.

## Tracked Telegram publication

`balkons/telegram_out` needs different treatment because the firmware already keeps
an application-level RAM queue for notifications that could not be published.

The canonical RPi bot currently subscribes to `balkons/telegram_out` with Paho's
default subscription QoS0. The migration therefore uses QoS1 **only on the
ESP32 -> broker publisher leg** for the tracked notification slot. Broker -> bot
remains QoS0.

The adapter owns one fixed tracked slot:

- topic storage: 64 bytes, allowing at most 63 text bytes plus the NUL terminator;
- payload storage: 896 bytes, allowing at most 895 text bytes plus the NUL terminator;
- conservative worst-case MQTT QoS1 PUBLISH size: 969 bytes;
- compile-time `static_assert` requires that worst-case packet size to remain within
  the explicit 1024-byte TX buffer;
- no dynamic allocation in the tracked-copy layer;
- the payload may be staged while MQTT is disconnected;
- once connected, `pumpTrackedPublish()` sends the staged copy with QoS1;
- the adapter models ownership explicitly as `empty`, `staged`, or `inFlight`;
- after a nonzero publish packet ID is returned, that exact packet ID remains owned
  by the adapter until the matching PUBACK arrives;
- ordinary disconnect/reconnect does **not** clear or downgrade an `inFlight` tracked
  publish and does not enqueue a second application-level PUBLISH.

This reconnect behavior is intentional for the pinned espMqttClient 1.7.3 contract:
its ordinary disconnect queue cleanup retains unacknowledged QoS>0 PUBLISH packets,
so the library can retransmit the original packet. Clearing the adapter's packet ID
on disconnect would make the adapter forget that retained upstream ownership and
could cause an unnecessary second publish after reconnect.

Only a matching PUBACK clears the tracked slot. Mismatched/stale PUBACKs are ignored.
The result is at-least-once broker delivery: a broker-accepted PUBLISH whose PUBACK is
lost can still be delivered more than once at the MQTT protocol level, but the adapter
must not create an additional duplicate by re-enqueueing the same logical Telegram
message after every reconnect.

The later firmware integration can keep the existing eight-message application
queue for additional notifications. One message at a time can move from that queue
into the adapter's tracked slot; the application queue item may be removed once the
adapter has accepted its own fixed staged copy, while the adapter keeps that copy
until matching PUBACK.

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
