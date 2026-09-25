#!/usr/bin/env python3
"""Deterministic checks for the shared GitHub API access rollout contract."""

from __future__ import annotations

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / ".github" / "github-api-access-v1.json"
STARTUP = ROOT / ".github" / "start-github-only.json"
EXPECTED_REVISION = "3bb0740b5f0a8ce631d2ff79f1acc4999ff6ed2c"


def load(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def main() -> int:
    manifest = load(MANIFEST)
    startup = load(STARTUP)

    assert manifest["contract"] == "GITHUB_API_ACCESS_V1"
    assert manifest["accepted_shared_revision"] == EXPECTED_REVISION
    assert manifest["read_behavior"]["serial_requests"] is True
    assert manifest["read_behavior"]["tight_polling"] is False
    assert manifest["read_behavior"]["changed_files_on_demand_only"] is True

    read_dispositions = set(manifest["read_behavior"]["dispositions"])
    assert {
        "PRIMARY_RATE_LIMIT_EXHAUSTED",
        "SECONDARY_RATE_LIMIT_SUSPECTED",
        "RETRY_AFTER_REQUIRED",
        "RESET_WAIT_REQUIRED",
        "READ_BACKOFF_REQUIRED",
        "TRANSPORT_RATE_LIMIT_METADATA_UNAVAILABLE",
    } <= read_dispositions

    mutation = manifest["mutation_behavior"]
    assert mutation["single_attempt_per_authorization"] is True
    assert mutation["ambiguous_outcome_requires_read_only_reconciliation"] is True
    assert mutation["stop_after_ambiguous_outcome"] is True
    assert mutation["automatic_retry"] is False
    assert mutation["automatic_rollback"] is False
    assert mutation["automatic_cleanup"] is False
    assert mutation["alternate_mutation_path"] is False

    mutation_dispositions = set(mutation["dispositions"])
    for outcome in (
        "MUTATION_OUTCOME_UNKNOWN_RATE_LIMIT",
        "MUTATION_OUTCOME_UNKNOWN_TIMEOUT",
        "MUTATION_OUTCOME_UNKNOWN_TRANSPORT",
    ):
        assert outcome in mutation_dispositions

    authority = manifest["authority_preservation"]
    assert authority["merge_remains_explicit_owner_authority"] is True
    assert authority["live_device_authority_not_granted"] is True
    assert authority["firmware_flash_or_ota_not_granted"] is True
    assert authority["mqtt_or_home_assistant_mutation_not_granted"] is True
    assert authority["physical_actuation_not_granted"] is True
    assert authority["credentials_permissions_repository_settings_not_granted"] is True

    binding = startup["github_api_access"]
    assert binding["manifest"] == ".github/github-api-access-v1.json"
    assert binding["accepted_shared_revision"] == EXPECTED_REVISION
    assert binding["start_budget"] == "BOOTSTRAP_MINIMAL"
    assert binding["sync_budget"] == "PR_REFRESH_COMPACT"
    assert binding["premerge_budget"] == "FINAL_PREMERGE_COMPACT"
    assert binding["postmerge_budget"] == "EXACT_MAIN_MINIMAL"
    assert binding["serial_requests"] is True
    assert binding["tight_polling"] is False
    assert binding["changed_files_on_demand_only"] is True
    assert binding["mutation_ambiguity_requires_read_only_reconciliation_and_stop"] is True
    assert binding["mutation_retry_widens_authority"] is False

    print("GitHub API access rollout contract PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
