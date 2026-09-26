# FAST-LANE v2.3 adoption — balcony-irrigation-esp32

Status: source/governance consumer adapter for `ops-workflows#124`.

Canonical shared revision: `rozkalnsandris/ops-workflows@274d58f2d9d3cb86feded2751b8f9009a4501f6b`.

## Adopted capabilities

- `BOOTSTRAP_MANIFEST_V1` through `.github/agent-bootstrap.json`.
- START safe auto-continuation and compact terminal-response semantics through the existing Agent Work Cycle/FAST rules.
- `WRITE_PREFLIGHT_COMPACT_V1` through `.github/github-api-access-v1.json`; no second local preflight framework.

The bootstrap manifest is routing metadata only. It contains no mutable branch/PR/CI/review/runtime truth and grants no authority.

## AUTO-RUN FULL applicability

`balcony-irrigation-esp32` does not currently provide a repository-local AUTO-RUN FULL contract or controller. For this rollout:

```text
AUTO_RUN_FULL = NOT_APPLICABLE
```

No controller is created merely for fleet uniformity. A future FULL adoption requires its own explicit repository-local governance decision.

## Deployment profile and local stricter rules

The deployment profile is `source-only`: repository GitHub Actions validate firmware/source but do not authorize or perform device flash/OTA or other live activation as part of this adoption.

`AGENTS.md` remains authoritative, including:

- explicit owner merge authority;
- separate firmware flash/OTA authority;
- separate live MQTT/Home Assistant/device/physical-actuation authority;
- firmware safety and hard runtime maximums;
- credentials and secret boundaries;
- fail-closed behavior after a started mutation errors or becomes ambiguous.

Merge never implies firmware activation, LIVE, or physical/device mutation.

## Queue boundary

Existing GITHUB-ONLY/LIVE-ALL deploy-queue support remains unchanged. This adoption does not activate Queue vNext `#96` or create any new queue authority.

## Authority

This adoption does not widen source, merge, firmware/live/device, MQTT/Home Assistant, credentials, permissions, retry, rollback, cleanup, Queue or deployment authority.
