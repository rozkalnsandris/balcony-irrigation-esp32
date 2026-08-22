# FAST-LANE v2.2 Composite — balcony irrigation ESP32

> Compatibility path: `AGENTS.md` already points to this v2.1 filename; these are the authoritative v2.2 rules.

## Core rule

**The human approves the RISK / DECISION. Automation executes the TECHNICAL STEPS.** Read-only checks never create owner gates. STRICT describes live/device risk, not approval-per-command.

## FAST

Documentation, native tests, source refactors and firmware source changes may proceed from fresh GitHub state through Ready in one coherent batch, including branch, PR, CI/review and up to two scope-preserving corrections. Batch 2-5 closely related same-risk items when coherent. Merge remains explicit.

## Human gate budget and Composite STRICT

Normal delivery has at most two owner gates: **MERGE**, then **COMPOSITE LIVE** only when device/live mutation is required. Before the live gate, automation gathers all read-only evidence. One bounded authorization binds exact firmware/source SHA, exact device/target, allowed mutation categories, operation limits and explicit exclusions. Preflight and verification belong inside the same fail-closed one-shot.

Where a firmware artifact is produced: pin the toolchain, build once, verify the exact artifact, re-check target/baseline and flash/deploy that exact artifact only.

## Local STRICT boundaries

Flashing/OTA, live MQTT commands/publishes, broker credential changes, Home Assistant mutation, provisioning, pump/relay activation or any physical/live-device state change require Composite Live authorization.

## Failure and evidence

Authorization is consumed at the first authorized mutation. Any later error/ambiguity requires evidence preservation and STOP; no automatic retry, rollback, cleanup, alternate flash path or other mutation unless explicitly pre-authorized.

Use one Ready receipt and one final live receipt. Put any remaining owner decision at the **end** under `ACTION REQUIRED`; when the owner must enter/run something, provide the exact copyable instruction in a fenced `bash` block.

Merge never authorizes flash/OTA/live MQTT or physical actuation.
