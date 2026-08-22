## FAST-LANE v2.1

- **Lane:** FAST / STRICT
- **Related work:** #...
- **Runtime/device effect:** NONE / READ_ONLY / MUTATION
- **Firmware activation required:** YES / NO
- **Trust-boundary change:** YES / NO

## Scope

Describe one coherent acceptance story. FAST may batch 2-5 closely related same-risk firmware work items.

## Validation

List safety checks, native tests, firmware builds/static analysis as applicable.

## Ready receipt

- Base / current main:
- Exact head SHA:
- CI/checks:
- Unresolved review threads:
- Reviewed scope/diff:
- Device/firmware activation classification:
- Exact next gate:

Merge is not authorized by this PR and never authorizes flash/OTA, live MQTT, broker credentials, Home Assistant mutation, pump/relay activation or another live-device write.
