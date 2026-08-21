#!/usr/bin/env python3
"""Fail-closed source contract check for the pinned espMqttClient 1.7.3 package."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Iterable

EXPECTED_LIBRARY_NAME = "espMqttClient"
EXPECTED_VERSION = "1.7.3"
EXPECTED_GIT_BLOBS: dict[str, str] = {
    "library.json": "af7fc819d491ea39b0aea3a9a9cd65bb98cc6a3d",
    "src/MqttClient.cpp": "5a393ef647c2e1018718f4530c337a598b08707c",
    "src/Outbox.h": "1dc75b4f78a1a8f75d4026e1eb428d60501166c9",
    "src/TypeDefs.h": "0f153606944b28298de8a9a21da5f18fe311479c",
}


class ContractError(RuntimeError):
    """Raised when the installed dependency does not match the frozen contract."""


def git_blob_sha(data: bytes) -> str:
    header = f"blob {len(data)}\0".encode("ascii")
    return hashlib.sha1(header + data).hexdigest()


def discover_library_root(libdeps_root: Path) -> Path:
    if not libdeps_root.is_dir():
        raise ContractError(f"libdeps root does not exist: {libdeps_root}")

    matches: list[Path] = []
    for manifest in sorted(libdeps_root.glob("*/library.json")):
        try:
            metadata = json.loads(manifest.read_text(encoding="utf-8"))
        except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
            raise ContractError(f"cannot parse {manifest}: {exc}") from exc

        if metadata.get("name") == EXPECTED_LIBRARY_NAME:
            matches.append(manifest.parent)

    if len(matches) != 1:
        raise ContractError(
            "expected exactly one espMqttClient package under "
            f"{libdeps_root}, found {len(matches)}"
        )

    return matches[0]


def verify_manifest(library_root: Path) -> None:
    manifest = library_root / "library.json"
    try:
        metadata = json.loads(manifest.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise ContractError(f"cannot parse {manifest}: {exc}") from exc

    if metadata.get("name") != EXPECTED_LIBRARY_NAME:
        raise ContractError(
            f"library name mismatch: expected {EXPECTED_LIBRARY_NAME!r}, "
            f"got {metadata.get('name')!r}"
        )
    if metadata.get("version") != EXPECTED_VERSION:
        raise ContractError(
            f"library version mismatch: expected {EXPECTED_VERSION!r}, "
            f"got {metadata.get('version')!r}"
        )


def verify_blobs(library_root: Path) -> None:
    for relative_path, expected_sha in EXPECTED_GIT_BLOBS.items():
        path = library_root / relative_path
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


def verify_contract(libdeps_root: Path) -> Path:
    library_root = discover_library_root(libdeps_root)
    verify_manifest(library_root)
    verify_blobs(library_root)
    return library_root


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Verify the exact espMqttClient 1.7.3 source contract used by C3"
    )
    parser.add_argument(
        "--libdeps-root",
        required=True,
        type=Path,
        help="PlatformIO environment libdeps directory containing espMqttClient",
    )
    return parser


def main(argv: Iterable[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        library_root = verify_contract(args.libdeps_root)
    except ContractError as exc:
        print(f"ERROR: {exc}")
        return 2

    print(f"library_root={library_root}")
    print(f"library_name={EXPECTED_LIBRARY_NAME}")
    print(f"library_version={EXPECTED_VERSION}")
    for relative_path, expected_sha in EXPECTED_GIT_BLOBS.items():
        print(f"git_blob[{relative_path}]={expected_sha}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
