# HA bounded watering protocol — build-only candidate (#69)

Source proposal for RPi5_main#1555. No flash, live MQTT, pump command, or HA cutover is authorized by this file or its merge.

## Build isolation

Normal esp32_ci and esp32_ota environments retain the legacy behavior. Only esp32_ha_bounded_candidate sets BALCONY_HA_BOUNDED_PROTOCOL=1 and is built by CI; it has no OTA configuration. Never upload this build as a test. A later owner-approved deployment requires a frozen build/artifact and coordinated retirement of legacy start publishers and schedules. In candidate mode all legacy start/extend paths are denied: plain HA ON, laist, laist_N. Existing OFF/stop and read-only diagnostics remain. The existing HA discovered switch can still display an ON control; it is not an admissible start path in candidate mode and must be replaced/hidden in the later HA source outcome.

## Wire contract (candidate only)

The existing HA pump command subscription is used; no new inbound subscription. All outgoing protocol responses are best-effort QoS0, non-retained, on `balkons/ha45/reply`. HA must subscribe/establish its receive context before requesting an offer. Do not issue these messages to a live broker as a source test.

| Message | Meaning |
| --- | --- |
| ticket45: + 16 lowercase hex characters | One correlated request, not an actuator command |
| offer: + request ID + colon + 24 lowercase hex characters | One device ticket, 10 s lifetime from issuance |
| start45: + exact offered 24 lowercase hex characters | Single-use candidate start; 32 bytes total |
| on: + exact ticket | Device software accepted start after GPIO write |
| off: + exact ticket | Device software executed OFF; not physical feedback |

Publisher must use non-retained commands. The existing MQTT adapter rejects retained deliveries and unsupported QoS/envelope lengths; the bounded parser additionally requires exact topic, length and lowercase hex, no trimming or embedded NUL. MQTT 3.1.1 does not expose the publisher retain flag to every already-subscribed client; broker-side authorization and non-retained publisher behavior still matter. QoS1 delivery and DUP flag are not an exactly-once contract. The protocol's single-use ticket handles repeat delivery independent of DUP.

A ticket is 64 bits of per-boot entropy from ESP32 hardware RNG while Wi-Fi is connected, plus a 32-bit sequence that refuses wrap. It is opaque correlation, not a password or publisher authentication. It prevents reuse within a boot, and cross-boot collision is probabilistic (64-bit entropy assumption), not a durable replay database. A new offer revokes the old offer. The ticket expires after 10 s using unsigned monotonic elapsed time; expiry is serviced even when idle. A queued request is valid for at most 5 s and its STOP/session epoch must still match. Tickets are invalidated on urgent STOP/OFF, session disconnect/connect, OTA, normal stop or start failure. A duplicate start cannot re-arm, extend, or create another ticket.

Only issue/consume a ticket when the application MQTT session is READY, Wi-Fi connected, pump OFF, no pending urgent STOP, no OTA, no outstanding MQTT outbox or tracked publish. The ticket is consumed before pre-start network work. Recheck the claimed ticket and all admission conditions immediately before the existing GPIO ON. Candidate planned duration is the remaining part of 45 s measured from device offer issuance (therefore at most 45 s from HA dispatch; usually slightly shorter than 45 s of actual GPIO ON). No duration override or extension. The independent existing 180 s absolute session limit remains. This uses the existing cooperative timer/watchdog and network bounds: an absolute physical 45 s cutoff under stalls is **not proven** by the source or native tests. If that requirement cannot be demonstrated, use an independently enforced short device-side deadline before LIVE.

The legacy retained switch ON can precede GPIO ON; it is not a correlated acknowledgement. The candidate queues an on receipt only after GPIO ON and checks the pump state/time first; inability to enqueue invokes local OFF. A normal or urgent stop invalidates the ticket and queues the matching off receipt after GPIO OFF. The reply uses the existing enqueue-only publishMessage interface, not a nested MQTT loop while ON. Connection loss in candidate mode requests immediate OFF when detected and the service loop checks loss as well. If an ACK is lost, delayed or arrives after STOP, it describes a historical software event only; HA must fail closed and use its separately reviewed OFF recovery path. No automatic ON retry. OTA/reboot or disconnect may lose ACK entirely.

## HA obligations — not implemented here

Run weather/sensor checks first, then enter one admission lock and establish a new unique request ID and cancellation context. Accept only the first matching offer received in that context within the bounded wait; freeze it, verify/record the day reservation and recheck safety and clock, then dispatch at most one matching start if still in time. Never refresh a ticket or adopt a new offer for the same attempt. STOP cancels the pending request/admission even before an offer is received; a device can issue a new offer for a delayed request arriving *after* STOP, but that offer cannot revive a canceled HA admission. Prove this race at the HA integration gate; device-side epoch alone cannot distinguish such a delayed request. No real start until this host-side cancellation invariant and sole publisher constraints are established.

Use matching software on/off receipts with fresh session evidence, not a retained switch state alone. Bound ACK wait to <=5 s from start dispatch, and normal OFF deadline no later than 45 s from dispatch; ambiguous result must inhibit future admission and invoke the separately reviewed bounded OFF path. The current physical cutoff, restart-day reservation, device/HA clock validity, sensor freshness (retained sensor categories alone insufficient), MQTT publisher/ACL inventory, exclusive legacy scheduler retirement and physical NO wiring remain explicit open gates. This candidate does not implement date policy, weather/sensor gates, HA helper changes, scheduler, permissions, new credentials, or durable daily admission. No new service.

## Offline evidence

Native tests cover exact payload decoding, default disabled, wrong topic/retained/truncated/NUL/uppercase payload, claim/duplicate/replacement, cancellation/invalidation, expiry and millis wrap, queue age/epoch, and shortened duration. Existing native tests and source contracts remain in CI; CI additionally compiles the candidate. These tests exercise the policy, not the real broker, callbacks under all transport stalls, physical relay, or HA integration. Verify no-late-start, timing, ACK/STOP interleavings, boot/session entropy and physical isolation before activation. Keep legacy behavior unchanged in the default environments.

References:
- https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/random.html
- https://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html
- RPi5_main#1555 / f7ed1c407fe5c901be3197c0e0f03ec0d210b3ce
