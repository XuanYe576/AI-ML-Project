from __future__ import annotations

import sys
import wave
from pathlib import Path

def summarize(path: Path) -> str:
    if not path.exists():
        return f"File not found: {path}"

    size_kb = path.stat().st_size / 1024.0
    duration = ""
    try:
        with wave.open(str(path), "rb") as wav:
            frames = wav.getnframes()
            rate = wav.getframerate() or 1
            duration = f", {frames / float(rate):.2f}s"
    except Exception:
        duration = ""
    return f"{path.name} ({size_kb:.1f} KB{duration})"

def main(argv: list[str]) -> int:
    if len(argv) < 2:
        print("usage: audio_processor.py <path>", file=sys.stderr)
        return 1
    path = Path(argv[1])
    print(summarize(path))
    return 0

if __name__ == "__main__":
    raise SystemExit(main(sys.argv))