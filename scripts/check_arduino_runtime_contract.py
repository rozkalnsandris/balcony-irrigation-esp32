#!/usr/bin/env python3
"""Fail-closed checks for Arduino-ESP32 runtime behavior relied on by firmware."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
from typing import Iterable

EXPECTED_SOURCE_GIT_BLOBS: dict[str, str] = {
    "libraries/Network/src/NetworkClient.cpp": "aa9792a40227688e1f14d810b92250101dcc2671",
    "libraries/ArduinoOTA/src/ArduinoOTA.cpp": "2afe9de2bd80246d37f7a9a41959274f96fb384e",
    "libraries/ArduinoOTA/src/ArduinoOTA.h": "e291f1b9abed4bf200c1a2c974a65b94a9074afa",
}


class ContractError(RuntimeError):
    """Raised when the installed framework no longer matches the reviewed contract."""


def git_blob_sha(data: bytes) -> str:
    header = f"blob {len(data)}\0".encode("ascii")
    return hashlib.sha1(header + data).hexdigest()


def discover_framework_root(packages_root: Path) -> Path:
    if not packages_root.is_dir():
        raise ContractError(f"packages root does not exist: {packages_root}")

    candidates: list[Path] = []
    for entry in sorted(packages_root.iterdir()):
        if not entry.is_dir():
            continue
        if all((entry / relative_path).is_file() for relative_path in EXPECTED_SOURCE_GIT_BLOBS):
            candidates.append(entry)

    if len(candidates) != 1:
        raise ContractError(
            "expected exactly one Arduino-ESP32 framework package under "
            f"{packages_root}, found {len(candidates)}"
        )

    return candidates[0]


def read_text(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8")
    except (OSError, UnicodeDecodeError) as exc:
        raise ContractError(f"cannot read {path}: {exc}") from exc


def verify_source_blobs(framework_root: Path) -> None:
    for relative_path, expected_sha in EXPECTED_SOURCE_GIT_BLOBS.items():
        path = framework_root / relative_path
        try:
            data = path.read_bytes()
        except OSError as exc:
            raise ContractError(f"cannot read {path}: {exc}") from exc

        actual_sha = git_blob_sha(data)
        if actual_sha != expected_sha:
            raise ContractError(
                f"Git blob mismatch for {relative_path}: "
                f"expected {expected_sha}, got {actual_sha}"
            )


def require_markers(text: str, markers: Iterable[str], source: str) -> None:
    for marker in markers:
        if marker not in text:
            raise ContractError(f"missing reviewed marker in {source}: {marker!r}")


def verify_network_client_contract(framework_root: Path) -> None:
    relative_path = "libraries/Network/src/NetworkClient.cpp"
    text = read_text(framework_root / relative_path)

    require_markers(
        text,
        (
            "#define WIFI_CLIENT_MAX_WRITE_RETRY     (10)",
            "#define WIFI_CLIENT_SELECT_TIMEOUT_US   (1000000)",
            "void NetworkClient::setConnectionTimeout(uint32_t milliseconds)",
            "_timeout = milliseconds;",
            "while (retry)",
            "select(socketFileDescriptor + 1, NULL, &set, NULL, &tv)",
        ),
        relative_path,
    )


def verify_arduino_ota_contract(framework_root: Path) -> None:
    cpp_path = "libraries/ArduinoOTA/src/ArduinoOTA.cpp"
    header_path = "libraries/ArduinoOTA/src/ArduinoOTA.h"
    cpp = read_text(framework_root / cpp_path)
    header = read_text(framework_root / header_path)

    require_markers(
        header,
        (
            "OTA_AUTH_ERROR",
            "OTA_BEGIN_ERROR",
            "void begin();",
        ),
        header_path,
    )
    require_markers(
        cpp,
        (
            "if (!_udp_ota.begin(_port))",
            "_error_callback(OTA_AUTH_ERROR);",
            "_start_callback();",
            "void ArduinoOTAClass::handle()",
            "if (!_initialized)",
        ),
        cpp_path,
    )

    auth_error = cpp.find("_error_callback(OTA_AUTH_ERROR);")
    start_callback = cpp.find("_start_callback();")
    if auth_error < 0 or start_callback < 0 or auth_error >= start_callback:
        raise ContractError(
            "reviewed ArduinoOTA ordering changed: OTA_AUTH_ERROR must remain "
            "reachable before the transfer start callback"
        )


def verify_contract(packages_root: Path) -> Path:
    framework_root = discover_framework_root(packages_root)
    verify_source_blobs(framework_root)
    verify_network_client_contract(framework_root)
    verify_arduino_ota_contract(framework_root)
    return framework_root


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Verify exact Arduino-ESP32 transport/OTA source behavior"
    )
    parser.add_argument(
        "--packages-root",
        required=True,
        type=Path,
        help="PlatformIO packages directory containing framework-arduinoespressif32",
    )
    return parser


def main(argv: Iterable[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        framework_root = verify_contract(args.packages_root)
    except ContractError as exc:
        print(f"ERROR: {exc}")
        return 2

    print(f"framework_root={framework_root}")
    for relative_path, expected_sha in EXPECTED_SOURCE_GIT_BLOBS.items():
        print(f"git_blob[{relative_path}]={expected_sha}")
    print("networkclient_write_retry_contract=KNOWN_UNBOUNDED_SERVICE_LATENCY")
    print("arduinoota_prestart_error_contract=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
