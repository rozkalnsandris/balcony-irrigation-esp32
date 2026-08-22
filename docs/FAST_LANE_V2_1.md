# FAST-LANE v2.1 Hybrid — balcony irrigation ESP32

## FAST

Documentation, native tests, source refactors and firmware changes are FAST while they remain Git/CI-only. A FAST batch may include 2-5 related same-risk work items and up to two scope-preserving corrective commits after CI/review findings.

## STRICT

Separate explicit owner authorization is required for flashing or OTA, live MQTT commands/publishes, broker credential changes, Home Assistant mutation, provisioning, pump/relay activation or any physical/live-device state change.

## CI

The workflow always starts. Repository safety validation always runs. Pull requests are classified internally:

- docs-only: run safety + stable merge gate, skip PlatformIO firmware/native/OTA builds;
- any firmware/config/test/workflow change: run the complete existing native tests, ESP32 builds, Cppcheck and OTA build-only validation;
- pushes to `main`: run the complete validation.

The stable aggregate status is `FAST-LANE Merge Gate`.

## Evidence

Produce one Ready receipt with lane, related work, exact base/head, CI, reviewed diff, unresolved threads, firmware activation classification and next gate. Merge remains explicit and never authorizes flash/OTA/live MQTT or physical actuation.
