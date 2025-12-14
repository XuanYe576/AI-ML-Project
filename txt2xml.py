#!/usr/bin/env python3
"""
Convert the text summary produced by xml2txt.py back into a lightweight MusicXML file.

Usage:
    python txt2xml.py              # interactive picker for *.txt
    python txt2xml.py path/to/file.txt

This is a best-effort reconstruction; data not present in the text file (tie/slur,
articulations, exact durations) is filled with simple defaults.
"""

from __future__ import annotations

import os
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path
from typing import Dict, List, Optional, Tuple


KEY_TO_FIFTHS = {
    "C♭ Major": -7, "G♭ Major": -6, "D♭ Major": -5, "A♭ Major": -4, "E♭ Major": -3,
    "B♭ Major": -2, "F Major": -1, "C Major": 0, "G Major": 1, "D Major": 2,
    "A Major": 3, "E Major": 4, "B Major": 5, "F♯ Major": 6, "C♯ Major": 7,
}

CLEF_LINE = {
    "G": 2,
    "F": 4,
    "C": 3,
}

ACCIDENTAL_ALTER = {
    "#": 1,
    "b": -1,
    "♭": -1,
    "♯": 1,
    "♮": 0,
    "": None,
}


def choose_txt_file() -> Optional[Path]:
    txt_files = [Path(f) for f in os.listdir() if f.endswith(".txt")]
    if not txt_files:
        print("No .txt files found in the current directory.")
        return None

    print("Available .txt files:")
    for idx, file in enumerate(txt_files, 1):
        print(f"{idx}. {file}")
    try:
        choice = int(input("Enter the number to convert: ").strip())
    except ValueError:
        print("Invalid selection.")
        return None
    if 1 <= choice <= len(txt_files):
        return txt_files[choice - 1]
    print("Choice out of range.")
    return None


def parse_metadata(lines: List[str]) -> Tuple[str, str, str, str]:
    tempo = time_sig = key_sig = clef = ""
    for line in lines[:4]:
        if line.startswith("Tempo:"):
            tempo = line.partition(":")[2].strip().split()[0]
        elif line.startswith("Time Signature:"):
            time_sig = line.partition(":")[2].strip()
        elif line.startswith("Key Signature:"):
            key_sig = line.partition(":")[2].strip()
        elif line.startswith("Clef:"):
            clef = line.partition(":")[2].strip()
    return tempo, time_sig, key_sig, clef


def parse_measure_line(line: str) -> Tuple[int, List[Dict[str, object]]]:
    # Example: "Measure 1: C#4 (quarter, 1), [Chord] E4 (quarter, 1), Rest"
    header, _, notes_part = line.partition(":")
    measure_no = int(re.findall(r"\d+", header)[0]) if re.findall(r"\d+", header) else 0
    notes_raw = [n.strip() for n in notes_part.split(",") if n.strip()]
    notes: List[Dict[str, object]] = []

    for raw in notes_raw:
        is_chord = raw.startswith("[Chord]")
        if is_chord:
            raw = raw.replace("[Chord]", "", 1).strip()

        lyric = ""
        if " - Lyric:" in raw:
            raw, _, lyric = raw.partition(" - Lyric:")
            lyric = lyric.strip()

        # Extract type/duration inside parentheses.
        note_type = ""
        duration_val = 1
        m = re.search(r"\(([^,]+),\s*([^)]+)\)", raw)
        if m:
            note_type = m.group(1).strip()
            try:
                duration_val = int(m.group(2).strip())
            except ValueError:
                duration_val = 1
            raw = raw[: m.start()].strip()

        entry: Dict[str, object] = {
            "chord": is_chord,
            "lyric": lyric,
            "type": note_type,
            "duration": duration_val,
        }

        if raw.lower().startswith("rest"):
            entry["rest"] = True
        else:
            entry["rest"] = False
            pitch_match = re.match(r"([A-G])([#b♯♭♮]?)(\d)", raw)
            if pitch_match:
                entry["step"] = pitch_match.group(1)
                entry["accidental"] = pitch_match.group(2)
                entry["octave"] = int(pitch_match.group(3))
            else:
                entry["step"] = "C"
                entry["accidental"] = ""
                entry["octave"] = 4
        notes.append(entry)
    return measure_no, notes


def build_musicxml(measures: List[Tuple[int, List[Dict[str, object]]]], tempo: str,
                   time_sig: str, key_sig: str, clef: str) -> ET.Element:
    score = ET.Element("score-partwise", version="3.1")
    part_list = ET.SubElement(score, "part-list")
    score_part = ET.SubElement(part_list, "score-part", id="P1")
    ET.SubElement(score_part, "part-name").text = "Music"

    part = ET.SubElement(score, "part", id="P1")

    for idx, (measure_no, notes) in enumerate(measures, 1):
        measure = ET.SubElement(part, "measure", number=str(measure_no or idx))
        if idx == 1:
            attributes = ET.SubElement(measure, "attributes")
            ET.SubElement(attributes, "divisions").text = "1"

            fifths = KEY_TO_FIFTHS.get(key_sig, 0)
            key_el = ET.SubElement(attributes, "key")
            ET.SubElement(key_el, "fifths").text = str(fifths)

            if "/" in time_sig:
                beats, beat_type = time_sig.split("/", 1)
                time_el = ET.SubElement(attributes, "time")
                ET.SubElement(time_el, "beats").text = beats.strip() or "4"
                ET.SubElement(time_el, "beat-type").text = beat_type.strip() or "4"

            clef_sign = clef.split()[0] if clef else "G"
            clef_el = ET.SubElement(attributes, "clef")
            ET.SubElement(clef_el, "sign").text = clef_sign
            line = CLEF_LINE.get(clef_sign, 2)
            ET.SubElement(clef_el, "line").text = str(line)

            if tempo:
                sound = ET.SubElement(measure, "sound")
                sound.set("tempo", tempo)

        for note_info in notes:
            note_el = ET.SubElement(measure, "note")
            if note_info.get("chord"):
                ET.SubElement(note_el, "chord")

            if note_info.get("rest"):
                ET.SubElement(note_el, "rest")
            else:
                pitch = ET.SubElement(note_el, "pitch")
                ET.SubElement(pitch, "step").text = note_info.get("step", "C")
                acc = note_info.get("accidental", "")
                alter = ACCIDENTAL_ALTER.get(acc, None)
                if alter is not None:
                    ET.SubElement(pitch, "alter").text = str(alter)
                ET.SubElement(pitch, "octave").text = str(note_info.get("octave", 4))

            ET.SubElement(note_el, "duration").text = str(note_info.get("duration", 1))
            note_type = note_info.get("type")
            if note_type and note_type.lower() != "unknown":
                ET.SubElement(note_el, "type").text = note_type

            lyric_text = note_info.get("lyric", "")
            if lyric_text:
                lyric_el = ET.SubElement(note_el, "lyric")
                ET.SubElement(lyric_el, "text").text = lyric_text

    return score


def read_text_file(path: Path) -> Tuple[str, str, str, str, List[Tuple[int, List[Dict[str, object]]]]]:
    content = path.read_text(encoding="utf-8").splitlines()
    tempo, time_sig, key_sig, clef = parse_metadata(content)

    measures: List[Tuple[int, List[Dict[str, object]]]] = []
    for line in content:
        if line.startswith("Measure"):
            measures.append(parse_measure_line(line))
    return tempo, time_sig, key_sig, clef, measures


def main(argv: List[str]) -> int:
    if len(argv) > 1:
        txt_path = Path(argv[1])
    else:
        chosen = choose_txt_file()
        if not chosen:
            return 1
        txt_path = chosen

    if not txt_path.exists():
        print(f"File not found: {txt_path}")
        return 1

    tempo, time_sig, key_sig, clef, measures = read_text_file(txt_path)
    if not measures:
        print("No measure data found in text file; aborting.")
        return 1

    score = build_musicxml(measures, tempo, time_sig, key_sig, clef)
    output_path = txt_path.with_suffix(".musicxml")
    ET.ElementTree(score).write(output_path, encoding="utf-8", xml_declaration=True)
    print(f"Wrote MusicXML to {output_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
