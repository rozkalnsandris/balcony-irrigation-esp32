# Repository operating rules

This repository contains ESP32 irrigation firmware and supporting tests/configuration. Git source work and live device activation are separate authority boundaries.

## FAST-LANE v2.1 Hybrid

Read `docs/FAST_LANE_V2_1.md` before implementation.

- **FAST** covers documentation, tests and firmware/source changes through Ready when nothing is flashed/OTA-applied and no live MQTT/Home Assistant/device state is mutated.
- FAST may batch **2-5 closely related same-risk work items** when they form one coherent firmware acceptance story.
- After initial publication, at most **two scope-preserving corrective commits** may address CI/review findings. A third correction or material scope/risk expansion requires STOP.
- Use one Ready receipt and refresh mutable merge evidence immediately before merge.
- **STRICT** includes firmware flash/OTA, live MQTT publish/commands, broker credentials, Home Assistant mutation, pump/relay/physical actuation, device provisioning and equivalent live authority.
- Merge remains explicit owner authority and never authorizes firmware activation or physical/device mutation.

## Firmware safety

Preserve fail-closed pump timing/command safety, hard maximums and existing static/native tests. Never weaken runtime safety checks to make CI pass. Do not put Wi-Fi/MQTT credentials or other secrets in committed firmware source.

Any command that can energize the pump, write device state or alter live broker/Home Assistant state is STRICT even when described as a test.
