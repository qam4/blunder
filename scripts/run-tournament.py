#!/usr/bin/env python3
"""
Multi-engine tournament using cutechess-cli.

Runs a round-robin or gauntlet tournament to compute Elo ratings
for Blunder against other UCI/xboard engines.

Usage:
    # Round-robin with all engines
    python scripts/run-tournament.py --engines engines.json --games 100

    # Gauntlet: test Blunder against all opponents
    python scripts/run-tournament.py --engines engines.json --games 100 --gauntlet

    # Quick test against Stockfish at low skill
    python scripts/run-tournament.py --games 50 --tc "5+0.05"

Engine config file (engines.json):
    [
        {
            "name": "Blunder-NNUE",
            "cmd": "build/dev/blunder.exe",
            "proto": "xboard",
            "args": ["--xboard", "--nnue", "weights/nnue_v002.bin"]
        },
        {
            "name": "Blunder-HC",
            "cmd": "build/dev/blunder.exe",
            "proto": "xboard",
            "args": ["--xboard"]
        },
        {
            "name": "Stockfish-Skill0",
            "cmd": "stockfish.exe",
            "proto": "uci",
            "options": {"Skill Level": "0", "Threads": "1"}
        }
    ]
"""

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
from datetime import datetime
from pathlib import Path


def find_cutechess(override: str | None = None) -> str:
    """Locate cutechess-cli executable."""
    if override:
        if Path(override).is_file():
            return override
        sys.exit(f"ERROR: cutechess-cli not found at: {override}")

    found = shutil.which("cutechess-cli")
    if found:
        return found

    fallback = Path(r"C:\Program Files (x86)\Cute Chess\cutechess-cli.EXE")
    if fallback.is_file():
        return str(fallback)

    fallback2 = Path(r"C:\tools\Cute Chess\cutechess-cli.exe")
    if fallback2.is_file():
        return str(fallback2)

    sys.exit("ERROR: cutechess-cli not found. Add it to PATH or use --cutechess.")


def default_engines_config(project_dir: Path) -> list[dict]:
    """Return a default engine list when no config file is provided."""
    ext = ".exe" if os.name == "nt" else ""
    engine_path = str(project_dir / "build" / "dev" / f"blunder{ext}")
    nnue_path = str(project_dir / "weights" / "nnue_v002.bin")

    engines = [
        {
            "name": "Blunder-NNUE",
            "cmd": engine_path,
            "proto": "xboard",
            "args": ["--xboard", "--nnue", nnue_path],
        },
        {
            "name": "Blunder-HC",
            "cmd": engine_path,
            "proto": "xboard",
            "args": ["--xboard"],
        },
    ]

    # Auto-detect Stockfish
    sf = shutil.which("stockfish")
    if sf:
        engines.append(
            {
                "name": "Stockfish-Skill0",
                "cmd": sf,
                "proto": "uci",
                "options": {"Skill Level": "0", "Threads": "1", "Hash": "16"},
            }
        )
        engines.append(
            {
                "name": "Stockfish-Skill3",
                "cmd": sf,
                "proto": "uci",
                "options": {"Skill Level": "3", "Threads": "1", "Hash": "16"},
            }
        )

    return engines


def build_engine_args(engine: dict, output_dir: Path) -> list[str]:
    """Build cutechess-cli engine arguments from config dict."""
    parts = ["-engine", f"name={engine['name']}", f"cmd={engine['cmd']}"]
    parts.append(f"proto={engine.get('proto', 'uci')}")

    # Add command-line arguments
    for arg in engine.get("args", []):
        parts.append(f"arg={arg}")

    # Add UCI/xboard options
    for key, val in engine.get("options", {}).items():
        parts.append(f"option.{key}={val}")

    # Stderr log
    safe_name = engine["name"].replace(" ", "_").lower()
    parts.append(f"stderr={output_dir / f'{safe_name}.err.log'}")

    return parts


def parse_args() -> argparse.Namespace:
    project_dir = Path(__file__).resolve().parent.parent
    book = str(project_dir / "books" / "i-gm1950.bin")

    p = argparse.ArgumentParser(
        description="Multi-engine tournament via cutechess-cli"
    )
    p.add_argument(
        "--engines",
        default=None,
        help="JSON file with engine configs (default: auto-detect)",
    )
    p.add_argument("--tc", default="10+0.1", help="Time control (default: 10+0.1)")
    p.add_argument("--book", default=book, help="Opening book path")
    p.add_argument(
        "--book-depth", type=int, default=4, help="Book depth (default: 4)"
    )
    p.add_argument(
        "--games", type=int, default=100, help="Games per pairing (default: 100)"
    )
    p.add_argument(
        "--concurrency", type=int, default=4, help="Parallel games (default: 4)"
    )
    p.add_argument(
        "--gauntlet",
        action="store_true",
        help="Gauntlet mode: first engine vs all others",
    )
    p.add_argument("--debug", action="store_true", help="Show engine I/O")
    p.add_argument("--cutechess", default=None, help="Path to cutechess-cli")
    return p.parse_args()


def run_tournament(args: argparse.Namespace) -> int:
    cutechess = find_cutechess(args.cutechess)
    project_dir = Path(__file__).resolve().parent.parent

    # Load engine configs
    if args.engines:
        with open(args.engines) as f:
            engines = json.load(f)
    else:
        engines = default_engines_config(project_dir)

    if len(engines) < 2:
        sys.exit("ERROR: Need at least 2 engines for a tournament.")

    # Validate engine binaries exist
    for eng in engines:
        if not Path(eng["cmd"]).is_file() and not shutil.which(eng["cmd"]):
            print(f"WARNING: Engine not found: {eng['cmd']} ({eng['name']})")

    # Create output directory
    scripts_dir = Path(__file__).resolve().parent
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    mode = "gauntlet" if args.gauntlet else "roundrobin"
    output_dir = scripts_dir / "output" / f"tournament_{mode}_{timestamp}"
    output_dir.mkdir(parents=True, exist_ok=True)

    pgn_out = str(output_dir / "tournament.pgn")
    log_file = output_dir / "cutechess.log"

    # Save engine config for reproducibility
    with open(output_dir / "engines.json", "w") as f:
        json.dump(engines, f, indent=2)

    # Build command
    cmd = [cutechess]

    for eng in engines:
        cmd.extend(build_engine_args(eng, output_dir))

    cmd.extend(
        [
            "-each",
            f"tc={args.tc}",
            f"book={args.book}",
            f"bookdepth={args.book_depth}",
            "-games",
            "2",
            "-rounds",
            str(args.games // 2),
            "-repeat",
            "2",
            "-maxmoves",
            "200",
            "-concurrency",
            str(args.concurrency),
            "-ratinginterval",
            "10",
            "-pgnout",
            pgn_out,
            "-srand",
            "0",
        ]
    )

    if args.gauntlet:
        cmd.append("-tournament")
        cmd.append("gauntlet")

    if args.debug:
        cmd.insert(1, "-debug")

    # Print header
    print("=" * 80)
    print(f"{'GAUNTLET' if args.gauntlet else 'ROUND-ROBIN'} TOURNAMENT")
    print("=" * 80)
    print(f"Cutechess:   {cutechess}")
    print(f"Engines:     {len(engines)}")
    for eng in engines:
        print(f"  - {eng['name']} ({eng.get('proto', 'uci')})")
    print(f"TC:          {args.tc}")
    print(f"Games/pair:  {args.games}")
    print(f"Concurrency: {args.concurrency}")
    print(f"Output:      {output_dir}")
    print("=" * 80)
    print()

    # Run
    with open(log_file, "w", encoding="utf-8") as lf:
        proc = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
        )
        assert proc.stdout is not None
        for line in proc.stdout:
            sys.stdout.write(line)
            sys.stdout.flush()
            lf.write(line)
        proc.wait()

    log_text = log_file.read_text(encoding="utf-8")
    return parse_results(log_text, engines)


def parse_results(log_text: str, engines: list[dict]) -> int:
    """Parse tournament results and print Elo ratings."""
    print()
    print("=" * 80)
    print("TOURNAMENT RESULTS")
    print("=" * 80)

    # Parse all score lines
    score_pattern = re.compile(
        r"Score of (\S+) vs (\S+):\s*(\d+)\s*-\s*(\d+)\s*-\s*(\d+)"
    )
    elo_pattern = re.compile(
        r"Elo difference:\s*([-\d.]+)\s*\+/-\s*([\d.]+).*?LOS:\s*([\d.]+)"
    )

    results: dict[str, dict] = {}
    for eng in engines:
        results[eng["name"]] = {"wins": 0, "losses": 0, "draws": 0}

    for match in score_pattern.finditer(log_text):
        e1, e2, w, l, d = match.groups()
        # Only take the last occurrence per pairing (final result)
        pass

    # Find the final ranking block if present
    rank_pattern = re.compile(r"Rank\s+Name.*?(?:\n.*?){1,20}", re.MULTILINE)

    # Print raw Elo lines from cutechess
    elo_lines = [
        l
        for l in log_text.splitlines()
        if "Elo" in l or "Score of" in l or "Rank" in l or "---" in l
    ]
    # Print last block of results
    last_score_idx = -1
    lines = log_text.splitlines()
    for i, line in enumerate(lines):
        if "Score of" in line:
            last_score_idx = i

    if last_score_idx >= 0:
        # Print from last score block to end
        start = max(0, last_score_idx - 1)
        for line in lines[start:]:
            stripped = line.strip()
            if stripped:
                print(stripped)

    print()
    print("=" * 80)
    print(f"Full PGN and logs saved to the output directory.")
    print("=" * 80)
    return 0


def main() -> None:
    args = parse_args()
    exit_code = run_tournament(args)
    sys.exit(exit_code)


if __name__ == "__main__":
    main()
