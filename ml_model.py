#!/usr/bin/env python3
"""
Placeholder ML inference script. Replace with a real model later.
Usage: ml_model.py <path-to-audio>
Prints a one-line summary to stdout.
"""

from __future__ import annotations

import sys
from pathlib import Path


def run_inference(path: Path) -> str:
    if not path.exists():
        return f"File not found: {path}"
    return f"stub inference on {path.name}"


def main(argv: list[str]) -> int:
    if len(argv) < 2:
        print("usage: ml_model.py <path>", file=sys.stderr)
        return 1
    print(run_inference(Path(argv[1])))
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
