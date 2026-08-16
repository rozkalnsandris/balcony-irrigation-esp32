#!/usr/bin/env python3
"""Fail CI if public-repo unsafe files or obvious credentials are tracked."""

from __future__ import annotations

import re
import subprocess
from pathlib import Path

FORBIDDEN_EXACT = {
    "include/secrets.h",
}

FORBIDDEN_PREFIXES = (
    ".pio/",
    ".vscode/",
    "claude-export/",
    "FULL_PRIVATE/",
)

FORBIDDEN_SUFFIXES = (
    ".bin",
    ".elf",
    ".map",
    ".dump",
    ".bak",
    ".backup",
    ".tar",
    ".tar.gz",
    ".tgz",
    ".zip",
)

SECRET_DECLARATION = re.compile(
    r'constexpr\s+char\s+'
    r'(WIFI_SSID|WIFI_PASSWORD|MQTT_USERNAME|MQTT_PASSWORD|OTA_PASSWORD)'
    r'\[\]\s*=\s*"([^"]*)"\s*;'
)

TOKEN_PATTERNS = (
    ("private key", re.compile(r"-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----")),
    ("GitHub token", re.compile(r"\bgh[pousr]_[A-Za-z0-9]{20,}\b")),
    ("Telegram bot token", re.compile(r"\b\d{8,12}:[A-Za-z0-9_-]{30,}\b")),
)

ALLOWED_PLACEHOLDERS = {"CHANGE_ME", "REDACTED", ""}


def tracked_files() -> list[str]:
    raw = subprocess.check_output(["git", "ls-files", "-z"])
    return [item.decode("utf-8") for item in raw.split(b"\0") if item]


def main() -> int:
    errors: list[str] = []

    for name in tracked_files():
        if name in FORBIDDEN_EXACT:
            errors.append(f"forbidden tracked file: {name}")
            continue

        if name.startswith(FORBIDDEN_PREFIXES):
            errors.append(f"forbidden tracked path: {name}")
            continue

        if name.endswith(FORBIDDEN_SUFFIXES):
            errors.append(f"forbidden tracked artefact/archive: {name}")
            continue

        path = Path(name)
        try:
            if path.stat().st_size > 2_000_000:
                continue
            text = path.read_text(encoding="utf-8")
        except (UnicodeDecodeError, OSError):
            continue

        for variable, value in SECRET_DECLARATION.findall(text):
            if value not in ALLOWED_PLACEHOLDERS:
                errors.append(f"non-placeholder {variable} literal in {name}")

        for label, pattern in TOKEN_PATTERNS:
            if pattern.search(text):
                errors.append(f"possible {label} in {name}")

    if errors:
        print("Repository safety check FAILED:")
        for error in errors:
            print(f"- {error}")
        return 1

    print("Repository safety check PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
