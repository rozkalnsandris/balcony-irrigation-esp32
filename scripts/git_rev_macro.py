#!/usr/bin/env python3
"""Inject the current Git revision into the PlatformIO build environment.

This file is loaded by PlatformIO as a PRE extra script, so it runs inside
PlatformIO's own Python interpreter and does not depend on a host shell
command named `python`.

Ignored files (for example include/secrets.h and .pio/) do not make the
revision dirty. Tracked modifications and non-ignored untracked files do.
"""

from pathlib import Path
import subprocess
import sys

Import("env")

if env.IsIntegrationDump():
    Return()

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

env.Append(
    CPPDEFINES=[
        ("FIRMWARE_GIT_REV", env.StringifyMacro(revision)),
    ]
)
