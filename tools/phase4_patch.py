from pathlib import Path
import subprocess

EXPECTED_MAIN_BLOB = "c7c3305a76da731ede38d93a51e2598c4220745b"
PATH = Path("src/main.cpp")

actual_blob = subprocess.check_output(
    ["git", "hash-object", str(PATH)], text=True
).strip()
if actual_blob != EXPECTED_MAIN_BLOB:
    raise SystemExit(
        f"main.cpp blob mismatch: expected {EXPECTED_MAIN_BLOB}, got {actual_blob}"
    )

text = PATH.read_text(encoding="utf-8")

replacements = [
    (
        '#include "command_payload_policy.h"\n#include "network_reconnect_policy.h"\n',
        '#include "command_payload_policy.h"\n#include "network_reconnect_policy.h"\n#include "pump_timing_policy.h"\n',
        "include pump timing policy",
    ),
    (
        '''  if (seconds == 0) {\n    seconds = DEFAULT_PUMP_SECONDS;\n  }\n\n  if (seconds > MAX_PUMP_SECONDS) {\n    seconds = MAX_PUMP_SECONDS;\n  }\n''',
        '''  seconds =\n      pump_timing_policy::normalizeRequestedSeconds(\n        seconds,\n        DEFAULT_PUMP_SECONDS,\n        MAX_PUMP_SECONDS\n      );\n''',
        "normalize start duration",
    ),
    (
        '''  uint32_t maxDurationMs =\n      MAX_PUMP_SECONDS * 1000UL;\n\n  uint32_t requestedAddMs =\n      additionalSeconds * 1000UL;\n\n  uint32_t oldDurationMs =\n      pumpPlannedDurationMs;\n\n  uint64_t requestedDuration =\n      static_cast<uint64_t>(\n        pumpPlannedDurationMs\n      ) +\n      requestedAddMs;\n\n  if (\n      requestedDuration >\n      maxDurationMs\n  ) {\n    pumpPlannedDurationMs =\n        maxDurationMs;\n  } else {\n    pumpPlannedDurationMs =\n        static_cast<uint32_t>(\n          requestedDuration\n        );\n  }\n\n  uint32_t actuallyAddedMs =\n      pumpPlannedDurationMs -\n      oldDurationMs;\n''',
        '''  const auto extension =\n      pump_timing_policy::extendPlannedDuration(\n        pumpPlannedDurationMs,\n        additionalSeconds,\n        MAX_PUMP_SECONDS\n      );\n\n  pumpPlannedDurationMs =\n      extension.plannedDurationMs;\n\n  const uint32_t actuallyAddedMs =\n      extension.addedMs;\n''',
        "use overflow-safe extension policy",
    ),
    (
        '''  uint32_t elapsedMs =\n      millis() -\n      pumpSessionStartMs;\n\n  constexpr uint32_t hardLimitMs =\n      MAX_PUMP_SECONDS * 1000UL;\n''',
        '''  const uint32_t now =\n      millis();\n\n  constexpr uint32_t hardLimitMs =\n      MAX_PUMP_SECONDS * 1000UL;\n''',
        "capture servicePump time once",
    ),
    (
        '''  if (elapsedMs >= hardLimitMs) {\n''',
        '''  if (\n      pump_timing_policy::hasElapsed(\n        now,\n        pumpSessionStartMs,\n        hardLimitMs\n      )\n  ) {\n''',
        "use wrap-safe hard-limit predicate",
    ),
    (
        '''      pumpPlannedDurationMs > 0 &&\n      elapsedMs >= pumpPlannedDurationMs\n''',
        '''      pumpPlannedDurationMs > 0 &&\n      pump_timing_policy::hasElapsed(\n        now,\n        pumpSessionStartMs,\n        pumpPlannedDurationMs\n      )\n''',
        "use wrap-safe planned-duration predicate",
    ),
]

for old, new, label in replacements:
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected exactly 1 match, found {count}")
    text = text.replace(old, new, 1)

PATH.write_text(text, encoding="utf-8")
print("Phase 4 main.cpp patch applied with all exact guards satisfied")
