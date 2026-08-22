# Repository operating rules

This repository contains ESP32 irrigation firmware and supporting tests/configuration. Git source work and live device activation are separate authority boundaries.

<!-- BEGIN FAST-LANE-V2.2-MANAGED -->
## FAST-LANE v2.2 Composite

Read `docs/FAST_LANE_V2_2.md` as the active local startup contract.

**Primary rule:** the human approves the **RISK / DECISION**; automation executes the **TECHNICAL STEPS**.

- `START`, `turpini`, or equivalent continuation may carry documentation, tests and firmware/source changes through Ready when nothing is flashed/OTA-applied and no live MQTT/Home Assistant/device state is mutated.
- FAST may batch **2-5 closely related same-risk work items** and use up to **two scope-preserving corrective commits** for CI/review findings.
- Normal delivery has at most two owner gates: explicit **MERGE**, then one bounded **COMPOSITE LIVE** only when device/live mutation is required.
- Read-only validation, evidence refresh, CI/review inspection, firmware candidate verification and reconciliation are technical steps, not owner gates.
- Composite Live must bind exact firmware/source SHA, exact device/target, allowed mutation categories, practical limits and explicit exclusions. Pin the toolchain, build once, verify the exact artifact and deploy/flash that exact artifact when applicable.
- Authorization is consumed at the first authorized mutation. Any later error, ambiguity or drift requires evidence preservation and STOP; no automatic retry, rollback, cleanup, alternate flash path or other mutation unless explicitly pre-authorized.
- **STRICT** includes firmware flash/OTA, live MQTT publish/commands, broker credentials, Home Assistant mutation, pump/relay/physical actuation, device provisioning and equivalent live authority.
- Put any remaining owner decision visibly at the end under `ACTION REQUIRED` and provide exact copyable input when needed.
- Merge remains explicit owner authority and never authorizes firmware activation or physical/device mutation.
<!-- END FAST-LANE-V2.2-MANAGED -->

## Firmware safety

Preserve fail-closed pump timing/command safety, hard maximums and existing static/native tests. Never weaken runtime safety checks to make CI pass. Do not put Wi-Fi/MQTT credentials or other secrets in committed firmware source.

Any command that can energize the pump, write device state or alter live broker/Home Assistant state is STRICT even when described as a test.
