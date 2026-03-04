#!/usr/bin/env python3
"""
STS (Strategic Test Suite) per-category analysis.

Runs each STS position through the engine via UCI protocol, scores the
engine's chosen move against the EPD's scored move list, and produces
a per-category breakdown showing strengths and weaknesses.

Usage:
    python scripts/analyze-sts.py [options]
"""

import argparse
import os
import re
import subprocess
import sys
import time
from collections import defaultdict
from pathlib import Path


STS_CATEGORIES = {
    "Undermine": 1,
    "Open Files and Diagonals": 2,
    "Knight Outposts": 3,
    "Square Vacancy": 4,
    "Bishop vs Knight": 5,
    "Recapturing": 6,
    "Offer of Simplification": 7,
    "AKPC": 8,           # Advancement of f/g/h Pawns (King-side Pawn Charge)
    "AT": 8,             # alternate tag
    "Advancement of a/b/c pawns": 9,
    "Simplification": 10,
    "King Activity": 11,
    "Center Control": 12,
    "Pawn Play in the Center": 13,
    "7th Rank": 14,      # Queens and Rooks to the 7th Rank
    "Knight Outposts/Centralization/Repositioning": 3,
    "Knight Outposts/Repositioning/Centralization": 3,
}

STS_NAMES = {
    1: "Undermining",
    2: "Open Files & Diagonals",
    3: "Knight Outposts",
    4: "Square Vacancy",
    5: "Bishop vs Knight",
    6: "Recapturing",
    7: "Offer of Simplification",
    8: "Kingside Pawn Advance",
    9: "Queenside Pawn Advance",
    10: "Simplification",
    11: "King Activity",
    12: "Center Control",
    13: "Pawn Play in Center",
    14: "7th Rank Play",
    15: "Avoid Pointless Exchange",
}


def parse_epd_line(line: str) -> dict | None:
    """Parse an STS EPD line into a structured dict."""
    line = line.strip()
    if not line:
        return None

    # Split FEN from opcodes: FEN is the first 4 fields
    parts = line.split()
    if len(parts) < 4:
        return None

    fen = " ".join(parts[:4])
    # Add default move counters if not present in FEN
    # EPD has 4 fields, FEN has 6 — add halfmove and fullmove
    fen += " 0 1"

    rest = " ".join(parts[4:])

    result = {"fen": fen}

    # Parse id to get category
    id_match = re.search(r'id\s+"STS\([^)]+\)\s+([^"]+)"', rest)
    if id_match:
        id_str = id_match.group(1)
        # Category is everything before the last .NNN
        cat_match = re.match(r"(.+)\.(\d+)$", id_str)
        if cat_match:
            result["category"] = cat_match.group(1)
            result["position_num"] = int(cat_match.group(2))

    # Parse c9 for LAN moves (what we'll match against UCI output)
    c9_match = re.search(r'c9\s+"([^"]+)"', rest)
    c8_match = re.search(r'c8\s+"([^"]+)"', rest)
    if c9_match and c8_match:
        moves_lan = c9_match.group(1).split()
        scores = c8_match.group(1).split()
        result["scored_moves"] = {}
        for m, s in zip(moves_lan, scores):
            result["scored_moves"][m] = int(s)

    return result


def categorize(cat_str: str) -> int:
    """Map a category string to its STS number."""
    if cat_str in STS_CATEGORIES:
        return STS_CATEGORIES[cat_str]
    # Fallback: try to match partial
    for key, num in STS_CATEGORIES.items():
        if key.lower() in cat_str.lower():
            return num
    return 15  # default to last category


class UCIEngine:
    """Minimal UCI engine wrapper for running test positions."""

    def __init__(self, cmd: str, args: list[str] | None = None):
        full_cmd = [cmd] + (args or [])
        self.proc = subprocess.Popen(
            full_cmd,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            bufsize=1,
        )
        self._send("uci")
        self._wait_for("uciok")

    def _send(self, cmd: str) -> None:
        assert self.proc.stdin is not None
        self.proc.stdin.write(cmd + "\n")
        self.proc.stdin.flush()

    def _wait_for(self, token: str, timeout: float = 10.0) -> list[str]:
        lines = []
        assert self.proc.stdout is not None
        start = time.time()
        while time.time() - start < timeout:
            line = self.proc.stdout.readline().strip()
            lines.append(line)
            if token in line:
                return lines
        return lines

    def analyze(self, fen: str, nodes: int = 1000000) -> str | None:
        """Analyze a position and return the best move in UCI notation."""
        self._send("ucinewgame")
        self._send("isready")
        self._wait_for("readyok")
        self._send(f"position fen {fen}")
        self._send(f"go nodes {nodes}")
        lines = self._wait_for("bestmove", timeout=60.0)
        for line in lines:
            if line.startswith("bestmove"):
                parts = line.split()
                if len(parts) >= 2:
                    return parts[1]
        return None

    def quit(self) -> None:
        try:
            self._send("quit")
            self.proc.wait(timeout=5)
        except Exception:
            self.proc.kill()


def default_engine() -> str:
    ext = ".exe" if os.name == "nt" else ""
    return str(Path("build", "dev", f"blunder{ext}"))


def parse_args() -> argparse.Namespace:
    project_dir = Path(__file__).resolve().parent.parent
    engine = str(project_dir / default_engine())
    epd = str(project_dir / "test" / "data" / "test-positions" / "STS1-STS15_LAN_v6.epd")

    p = argparse.ArgumentParser(description="STS per-category analysis")
    p.add_argument("--engine", default=engine, help="Engine binary path")
    p.add_argument("--epd", default=epd, help="STS EPD file path")
    p.add_argument("--nodes", type=int, default=1000000,
                   help="Nodes per position (default: 1000000)")
    return p.parse_args()


def main() -> None:
    args = parse_args()

    if not Path(args.engine).is_file():
        sys.exit(f"ERROR: Engine not found: {args.engine}")
    if not Path(args.epd).is_file():
        sys.exit(f"ERROR: EPD file not found: {args.epd}")

    # Parse all positions
    positions = []
    with open(args.epd, encoding="utf-8") as f:
        for line in f:
            pos = parse_epd_line(line)
            if pos and "scored_moves" in pos and "category" in pos:
                positions.append(pos)

    print(f"Loaded {len(positions)} positions from {Path(args.epd).name}")
    print(f"Engine: {args.engine}")
    print(f"Nodes per position: {args.nodes:,}")
    print()

    # Start engine
    engine = UCIEngine(args.engine, ["--uci"])

    # Run positions and collect per-category scores
    cat_scores: dict[int, int] = defaultdict(int)
    cat_max: dict[int, int] = defaultdict(int)
    cat_correct: dict[int, int] = defaultdict(int)
    cat_total: dict[int, int] = defaultdict(int)
    total_score = 0
    total_max = 0
    total_positions = 0

    for i, pos in enumerate(positions):
        cat_num = categorize(pos["category"])
        best_move = engine.analyze(pos["fen"], args.nodes)

        max_score = max(pos["scored_moves"].values()) if pos["scored_moves"] else 0
        move_score = pos["scored_moves"].get(best_move, 0) if best_move else 0

        cat_scores[cat_num] += move_score
        cat_max[cat_num] += max_score
        cat_total[cat_num] += 1
        if move_score == max_score and max_score > 0:
            cat_correct[cat_num] += 1

        total_score += move_score
        total_max += max_score
        total_positions += 1

        # Progress every 100 positions
        if (i + 1) % 100 == 0:
            pct = 100.0 * total_score / total_max if total_max > 0 else 0
            print(f"  [{i+1}/{len(positions)}] running score: {pct:.1f}%", flush=True)

    engine.quit()

    # Print results
    print()
    print("=" * 72)
    print(f"{'Category':<30} {'Score':>8} {'Max':>8} {'Pct':>7} {'Best':>6}")
    print("-" * 72)

    sorted_cats = sorted(cat_scores.keys())
    cat_pcts = {}
    for cat_num in sorted_cats:
        name = STS_NAMES.get(cat_num, f"Category {cat_num}")
        score = cat_scores[cat_num]
        mx = cat_max[cat_num]
        pct = 100.0 * score / mx if mx > 0 else 0
        correct = cat_correct[cat_num]
        total = cat_total[cat_num]
        cat_pcts[cat_num] = pct
        print(f"{name:<30} {score:>8} {mx:>8} {pct:>6.1f}% {correct:>3}/{total}")

    print("-" * 72)
    total_pct = 100.0 * total_score / total_max if total_max > 0 else 0
    elo = 44.523 * total_pct - 242.85
    print(f"{'TOTAL':<30} {total_score:>8} {total_max:>8} {total_pct:>6.1f}%")
    print(f"{'ELO estimate':<30} {int(elo):>8}")
    print("=" * 72)

    # Analysis: strengths and weaknesses
    if cat_pcts:
        print()
        avg_pct = total_pct
        strong = [(n, p) for n, p in cat_pcts.items() if p > avg_pct + 5]
        weak = [(n, p) for n, p in cat_pcts.items() if p < avg_pct - 5]

        strong.sort(key=lambda x: -x[1])
        weak.sort(key=lambda x: x[1])

        if strong:
            print("Strengths (>5% above average):")
            for cat_num, pct in strong:
                name = STS_NAMES.get(cat_num, f"Category {cat_num}")
                print(f"  + {name}: {pct:.1f}% (+{pct - avg_pct:.1f}%)")

        if weak:
            print("Weaknesses (>5% below average):")
            for cat_num, pct in weak:
                name = STS_NAMES.get(cat_num, f"Category {cat_num}")
                print(f"  - {name}: {pct:.1f}% ({pct - avg_pct:.1f}%)")

        if not strong and not weak:
            print("No significant strengths or weaknesses — balanced profile.")

    print()


if __name__ == "__main__":
    main()
