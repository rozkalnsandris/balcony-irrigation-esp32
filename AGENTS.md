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

<!-- BEGIN GITHUB-ONLY-LIVE-ALL-V1-MANAGED -->
## GITHUB-ONLY / LIVE-ALL v1

Canonical shared contract: `rozkalnsandris/ops-workflows/docs/GITHUB_ONLY_LIVE_ALL.md` with machine invariants in `policy/github-only-live-all-v1.json`.

- `GITHUB-ONLY` (including `git hub only`) means fresh GitHub state, firmware/source/docs/test work, and preparation of a future activation up to but not including the first live device/broker/Home Assistant mutation.
- Persist deferred rollout state as public-safe `[DEPLOY-QUEUE]` issues in `rozkalnsandris/ops-workflows`; chat or memory is never the queue.
- Merge remains separately explicit. Neither `GITHUB-ONLY` nor `LIVE-ALL` authorizes merge.
- A GitHub write whose deterministic side effect flashes/activates firmware or changes live device/broker/Home Assistant state counts as live work and must not run under `GITHUB-ONLY`.
- Queue `READY` requires the final exact source/artifact identity, exact device/target alias, reviewed entrypoint, preflight, verification, allowed mutations/limits and no outstanding separate prerequisite owner gate.
- `LIVE-ALL` snapshots only open `READY` items present at command start and freshly revalidates exact source/artifact/target/baseline, but it may execute only ordinary predeclared mutations that the repository-local contract already permits inside that exact authorization envelope.
- Firmware flash/OTA, live MQTT publish/commands, Home Assistant mutation, pump/relay/physical actuation, device provisioning and credentials remain separately gated under this repository's STRICT rules and do not become ordinary implicit `LIVE-ALL` work merely because a queue item exists.
- After any selected live mutation starts, error/ambiguity requires public-safe evidence preservation and STOP of the remaining batch; no automatic retry/rollback/cleanup/alternate flash path unless explicitly pre-authorized.
- Firmware safety rules remain authoritative and stricter where applicable.
<!-- END GITHUB-ONLY-LIVE-ALL-V1-MANAGED -->

## Firmware safety

Preserve fail-closed pump timing/command safety, hard maximums and existing static/native tests. Never weaken runtime safety checks to make CI pass. Do not put Wi-Fi/MQTT credentials or other secrets in committed firmware source.

Any command that can energize the pump, write device state or alter live broker/Home Assistant state is STRICT even when described as a test.

<!-- BEGIN START-GITHUB-ONLY-V1-MANAGED -->
## START_GITHUB_ONLY_V1 deterministic bootstrap amendment

Startup contract: `rozkalnsandris/ops-workflows/docs/START_GITHUB_ONLY_V1.md`.
Repository manifest: `.github/start-github-only.json`.

- `START <repository> GITHUB-ONLY` refreshes local rules/handoff, the pinned shared policy and START contract, current default branch/governance capability, active PRs, active issues/dependencies, and relevant deploy-queue items before selecting the manifest-defined canonical lane.
- Revalidate mutable GitHub state immediately before every state-dependent write.
- The absence of an open issue alone is NOT a STOP condition. Do not invent speculative work.
- If declared tie-breakers cannot resolve equally authoritative lanes, report `AMBIGUOUS_CANONICAL_LANE` instead of choosing arbitrarily.
- Final routing is one of `READY_FOR_MERGE`, `PARKED`, `STOP_ERROR`, `NEW_SCOPE_OR_RISK`, `AMBIGUOUS_CANONICAL_LANE`, or `IDLE`.
- `PARKED` is session-only. **EXECUTOR** availability is session capability, not **READY** rollout eligibility.
- Executor unavailability alone must not change `READY` to `BLOCKED`; use `BLOCKED` only for rollout eligibility or contract failure.
- Repository-local stricter firmware, physical-safety and trust-boundary rules remain authoritative.
<!-- END START-GITHUB-ONLY-V1-MANAGED -->

<!-- BEGIN AGENT-WORK-CYCLE-V1-MANAGED -->
## Agent Work Cycle v1

Shared governance contract: `rozkalnsandris/ops-workflows/docs/AGENT_WORK_CYCLE_V1.md` with machine invariants in `policy/agent-work-cycle-v1.json`. Repository-local rules remain authoritative and may be stricter.

### Canonical state and minimum-sufficient retrieval

- GitHub is canonical for mutable source, branch, SHA, issue/PR, CI/review and authorization-continuity state. Never reuse mutable state from chat history without a fresh read.
- `START balcony-irrigation-esp32` uses the repository-local startup routing. Bootstrap only enough state to identify one current work item/lane/gate: current `AGENTS.md`/rules, canonical handoff or continuation when present, current default-branch SHA, and only the issue/PR state required by that lane.
- For a current PR, inspect only the current exact head, required checks, reviews and unresolved threads unless a failure or conflict requires deeper evidence.
- `SYNC balcony-irrigation-esp32` is incremental refresh of the current lane, not a repo-wide audit. Re-read a handoff only when continuation may have changed or is ambiguous.
- `turpini` resumes the same scope with incremental retrieval. It never creates MERGE, LIVE, retry, rollback, cleanup, credential, permission or runtime authority.
- Do not enumerate unrelated work or historical CI/log/comment/review history during normal START/SYNC. Broaden retrieval only demand-driven or under an explicit repository-local audit mode such as `AUDIT-HANDOFF`.

### Work execution and owner gates

- Prefer the smallest coherent fix and carry safe source/docs/tests/policy work through Draft PR, exact-head CI/review convergence and Ready when repository-local rules permit it.
- Technical intermediate steps such as CI polling, exact-head/diff checks, read-only preflight, evidence refresh and scope-preserving correction are not owner gates.
- MERGE remains an explicit owner decision unless a repository-local explicitly activated FULL mode already grants issue-scoped merge authority. Merge never implies LIVE/deploy authority.
- LIVE/deploy/runtime/credential/permission/production-data mutations require the separate exact authorization defined by repository-local rules.
- Authorization is consumed at the first authorized mutation. After mutation begins, any error, timeout, drift, ambiguity or authorization uncertainty is fail-closed: collect only necessary read-only evidence and STOP. No retry, rollback, cleanup or alternate mutation without fresh explicit authority unless it was pre-authorized.

### Terminal response — exact next command

Every user-visible work-cycle response that ends or pauses repository work must finish with exactly one copy-pasteable command as the final actionable content.

- Use `ACTION REQUIRED` only for a genuine owner authorization/decision gate; never manufacture a gate merely to satisfy this presentation rule.
- When a real owner gate exists, output the exact authorization command with current issue/PR identifiers and exact SHA/target bindings where applicable.
- When no owner gate exists and mutable GitHub/external state must be refreshed, output `SYNC balcony-irrigation-esp32`.
- When no owner gate exists and same-scope safe technical continuation is immediately available, output `turpini`.
- When the current outcome is complete and no same-scope continuation remains, output `START balcony-irrigation-esp32`.
- Give exactly one recommended command, not a menu. The response-format contract never grants authority by itself.
<!-- END AGENT-WORK-CYCLE-V1-MANAGED -->
