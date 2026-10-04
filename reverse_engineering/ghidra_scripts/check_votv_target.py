#!/usr/bin/env python3
"""Read-only identity check for the VotV executable copies and runtime log.

This does not invoke Ghidra, write to either game tree, or open the active
Ghidra project. It exits non-zero if copies are missing, hashes differ, or the
target is not an x86-64 PE32+ executable.
"""

from __future__ import annotations

import hashlib
import subprocess
from pathlib import Path


TARGETS = (
    Path("/home/matt/VOTV/WindowsNoEditor/VotV/Binaries/Win64/VotV-Win64-Shipping.exe"),
    Path("/home/matt/Desktop/a09n/WindowsNoEditor/VotV/Binaries/Win64/VotV-Win64-Shipping.exe"),
)
LOG = Path("/home/matt/Desktop/a09n/WindowsNoEditor/VotV/Binaries/Win64/multivoid.log")
EXPECTED_SHA256 = "ad478218ec5513cc4c1682937db3214cbf2694d1dcd0583eb423447c049dd3ae"


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def command(*args: str) -> str:
    result = subprocess.run(args, capture_output=True, text=True, check=False)
    return (result.stdout + result.stderr).strip()


def main() -> int:
    ok = True
    hashes: list[str] = []
    for target in TARGETS:
        print("== %s ==" % target)
        if not target.is_file():
            print("MISSING")
            ok = False
            continue
        digest = sha256_file(target)
        description = command("file", "-b", str(target))
        hashes.append(digest)
        print("size: %d" % target.stat().st_size)
        print("sha256: %s" % digest)
        print("file: %s" % description)
        if digest != EXPECTED_SHA256:
            print("ERROR: hash differs from the prepared target")
            ok = False
        if "PE32+" not in description or "x86-64" not in description:
            print("ERROR: expected PE32+ x86-64")
            ok = False

    if hashes and len(set(hashes)) != 1:
        print("ERROR: executable copies are not byte-identical")
        ok = False

    print("\n== runtime log ==")
    print(LOG)
    print("exists: %s" % LOG.is_file())
    if LOG.is_file():
        with LOG.open("r", encoding="utf-8", errors="replace") as stream:
            print("lines: %d" % sum(1 for _ in stream))
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
