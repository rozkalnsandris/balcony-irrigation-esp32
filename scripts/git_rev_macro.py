#!/usr/bin/env python3
"""Emit a PlatformIO build flag with the current Git revision.

Ignored files (for example include/secrets.h and .pio/) do not make the
revision dirty. Tracked modifications and non-ignored untracked files do.
"""

from pathlib import Path
import subprocess
import sys

PROJECT_ROOT = Path(__file__).resolve().parents[1]


def git_output(*args: str) -> str:
    return subprocess.check_output(
        ["git", *args],
        cwd=PROJECT_ROOT,
        text=True,
        stderr=subprocess.STDOUT,
    ).strip()


try:
    revision = git_output("rev-parse", "--verify", "HEAD")
    dirty = bool(git_output("status", "--porcelain", "--untracked-files=normal"))
except (subprocess.CalledProcessError, FileNotFoundError) as exc:
    print(f"Unable to determine firmware Git revision: {exc}", file=sys.stderr)
    raise SystemExit(2) from exc

if dirty:
    revision += "-dirty"

# PlatformIO parses the printed line as build_flags. The escaped quotes make
# FIRMWARE_GIT_REV a C/C++ string literal.
print(f"'-DFIRMWARE_GIT_REV=\\\"{revision}\\\"'")
