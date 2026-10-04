#!/usr/bin/env python3
"""Summarize a Multivoid log without changing it or either game tree."""

from __future__ import annotations

import argparse
import re
from collections import Counter
from pathlib import Path


DEFAULT_LOG = Path("/home/matt/Desktop/a09n/WindowsNoEditor/VotV/Binaries/Win64/multivoid.log")
LEVEL = re.compile(r"\[(INFO |WARN |ERROR|DEBUG)\]")
ANCHOR = re.compile(r"resolve: ([^=]+)=([0-9A-Fa-f]+) \(rva (0x[0-9A-Fa-f]+)\)")
DOMAIN = re.compile(
    r"transformer|generator|power|breaker|repair|fullFix|isBroken|cycle|turnedOn",
    re.IGNORECASE,
)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", nargs="?", type=Path, default=DEFAULT_LOG)
    args = parser.parse_args()
    if not args.log.is_file():
        parser.error("log does not exist: %s" % args.log)

    lines = args.log.read_text(encoding="utf-8", errors="replace").splitlines()
    levels = Counter(match.group(1).strip() for line in lines if (match := LEVEL.search(line)))
    anchors = [match.groups() for line in lines if (match := ANCHOR.search(line))]
    domain = [(number, line) for number, line in enumerate(lines, 1) if DOMAIN.search(line)]

    print("path: %s" % args.log)
    print("lines: %d" % len(lines))
    print("levels: %s" % ", ".join("%s=%d" % item for item in sorted(levels.items())))
    print("anchors:")
    for name, address, rva in anchors:
        print("  %-24s %s (%s)" % (name.strip(), address, rva))
    print("transformer-domain lines: %d" % len(domain))
    for number, line in domain:
        print("  %d:%s" % (number, line))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
