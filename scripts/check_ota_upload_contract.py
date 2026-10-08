#!/usr/bin/env python3
"""Freeze the deterministic reverse-TCP contract for balcony ArduinoOTA."""

from __future__ import annotations

from pathlib import Path

from ota_upload_existing import platformio_core_directory

ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"OTA upload contract FAILED: {message}")


def main() -> int:
    platformio = (ROOT / "platformio.ini").read_text(encoding="utf-8")
    helper = (ROOT / "scripts/ota_upload_existing.py").read_text(encoding="utf-8")
    readme = (ROOT / "README.md").read_text(encoding="utf-8")

    marker = "[env:esp32_ota]"
    require(marker in platformio, "esp32_ota environment missing")
    ota_section = platformio.split(marker, 1)[1].split("\n[", 1)[0]
    require("upload_protocol = espota" in ota_section, "espota protocol missing")
    require("upload_port = balkons-esp32.local" in ota_section, "OTA target drift")
    require("--host_port=3233" in ota_section, "fixed reverse TCP port missing")
    require(
        "--auth=${sysenv.BALCONY_OTA_AUTH}" in ota_section,
        "dedicated auth environment variable missing",
    )

    require("DEFAULT_HOST_PORT = 3233" in helper, "helper host port drift")
    require("--expected-source-sha" in helper, "source identity guard missing")
    require("--expected-firmware-sha256" in helper, "artifact hash guard missing")
    require("--untracked-files=no" in helper, "tracked-worktree guard missing")
    require("OTA_AUTH=loaded-not-displayed" in helper, "safe auth status missing")

    # Pin both PlatformIO JSON shapes. A malformed result must fail closed.
    core = "/opt/platformio-core"
    require(
        platformio_core_directory({"core_dir": core}) == Path(core),
        "legacy PlatformIO core_dir layout rejected",
    )
    require(
        platformio_core_directory({"core_dir": {"title": "Core Directory", "value": core}})
        == Path(core),
        "structured PlatformIO core_dir layout rejected",
    )
    for invalid in (None, "", "relative-path", {"title": "Core Directory"}, 42):
        try:
            platformio_core_directory({"core_dir": invalid})
        except SystemExit as exc:
            require(str(exc) == "PLATFORMIO_CORE_DIR_INVALID", "unsafe core_dir failure")
        else:
            require(False, "invalid PlatformIO core_dir unexpectedly accepted")

    require("PLATFORMIO_UPLOAD_FLAGS='--auth=" not in readme, "unsafe old auth guidance remains")
    require("3233/tcp" in readme, "firewall prerequisite not documented")
    require("ota_upload_existing.py" in readme, "canonical helper not documented")

    print("OTA upload contract PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
