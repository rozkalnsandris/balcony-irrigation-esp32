#!/usr/bin/env python3
"""Upload one already-built exact ESP32 OTA artifact without rebuilding it."""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import re
import shutil
import socket
import subprocess
import sys
from pathlib import Path

DEFAULT_TARGET = "balkons-esp32.local"
DEFAULT_ESP_PORT = 3232
DEFAULT_HOST_PORT = 3233
PLACEHOLDERS = {"", "CHANGE_ME", "REDACTED"}
OTA_PASSWORD_RE = re.compile(
    r'constexpr\s+char\s+OTA_PASSWORD\[\]\s*=\s*"([^"]*)"\s*;'
)


def fail(message: str) -> "NoReturn":
    raise SystemExit(message)


def git_output(root: Path, *args: str) -> str:
    return subprocess.check_output(
        ["git", "-C", str(root), *args], text=True
    ).strip()


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def read_ota_password(path: Path) -> str:
    try:
        text = path.read_text(encoding="utf-8")
    except OSError as exc:
        fail(f"OTA credential file unavailable: {exc}")
    matches = OTA_PASSWORD_RE.findall(text)
    if len(matches) != 1:
        fail("expected exactly one OTA_PASSWORD declaration")
    password = matches[0]
    if password in PLACEHOLDERS:
        fail("OTA_PASSWORD is missing or still a placeholder")
    return password


def find_pio() -> str:
    for name in ("pio-balcony", "pio"):
        candidate = shutil.which(name)
        if candidate:
            return candidate
    fallback = Path.home() / ".local/bin/pio-balcony"
    if fallback.is_file():
        return str(fallback)
    fail("PlatformIO executable not found (tried pio-balcony and pio)")


def find_espota(pio: str) -> Path:
    result = subprocess.run(
        [pio, "system", "info", "--json-output"],
        check=True,
        capture_output=True,
        text=True,
    )
    data = json.loads(result.stdout)
    core_dir = Path(data["core_dir"])
    espota = core_dir / "packages/framework-arduinoespressif32/tools/espota.py"
    if not espota.is_file():
        fail(f"espota.py not found under PlatformIO core: {core_dir}")
    return espota


def load_espota(path: Path):
    spec = importlib.util.spec_from_file_location("balcony_espota", path)
    if spec is None or spec.loader is None:
        fail("unable to load espota.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    if not callable(getattr(module, "main", None)):
        fail("espota.py does not expose main()")
    return module


def validate_hex(value: str, length: int, label: str) -> str:
    normalized = value.lower()
    if len(normalized) != length or re.fullmatch(r"[0-9a-f]+", normalized) is None:
        fail(f"invalid {label}")
    return normalized


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Upload the exact prebuilt balcony firmware via ArduinoOTA."
    )
    parser.add_argument("--expected-source-sha", required=True)
    parser.add_argument("--expected-firmware-sha256", required=True)
    parser.add_argument("--target", default=DEFAULT_TARGET)
    args = parser.parse_args()

    expected_source = validate_hex(args.expected_source_sha, 40, "source SHA")
    expected_firmware = validate_hex(
        args.expected_firmware_sha256, 64, "firmware SHA-256"
    )

    root = Path(__file__).resolve().parents[1]
    head = git_output(root, "rev-parse", "HEAD")
    if head != expected_source:
        fail(f"HEAD_MISMATCH: expected {expected_source}, got {head}")

    dirty = git_output(root, "status", "--porcelain", "--untracked-files=no")
    if dirty:
        fail("TRACKED_WORKTREE_DIRTY")

    artifact = root / ".pio/build/esp32_ota/firmware.bin"
    if not artifact.is_file():
        fail("OTA_ARTIFACT_MISSING")
    actual_firmware = sha256_file(artifact)
    if actual_firmware != expected_firmware:
        fail(
            "ARTIFACT_HASH_MISMATCH: "
            f"expected {expected_firmware}, got {actual_firmware}"
        )

    password = read_ota_password(root / "include/secrets.h")
    pio = find_pio()
    espota = find_espota(pio)
    uploader = load_espota(espota)

    try:
        target_ip = socket.gethostbyname(args.target)
    except OSError as exc:
        fail(f"OTA_TARGET_RESOLUTION_FAILED: {exc}")

    print(f"OTA_SOURCE_SHA={head}")
    print(f"OTA_ARTIFACT_SHA256={actual_firmware}")
    print(f"OTA_TARGET={args.target} ({target_ip})")
    print(f"OTA_ESP_PORT={DEFAULT_ESP_PORT}")
    print(f"OTA_HOST_PORT={DEFAULT_HOST_PORT}")
    print("OTA_AUTH=loaded-not-displayed")

    result = uploader.main(
        [
            "--ip", args.target,
            "--port", str(DEFAULT_ESP_PORT),
            "--host_port", str(DEFAULT_HOST_PORT),
            "--auth", password,
            "--file", str(artifact),
            "--progress",
        ]
    )
    return int(result or 0)


if __name__ == "__main__":
    raise SystemExit(main())
