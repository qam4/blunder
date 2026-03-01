#!/usr/bin/env python3
"""
SPRT regression test using cutechess-cli.
Compares two engine builds to detect strength regressions.

Usage:
    python scripts/run-cutechess.py [options]
"""

import argparse
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

    # Windows fallback
    fallback = Path(r"C:\tools\Cute Chess\cutechess-cli.exe")
    if fallback.is_file():
        return str(fallback)

    sys.exit("ERROR: cutechess-cli not found. Add it to PATH or use --cutechess.")


def default_engine() -> str:
    """Return default engine path based on platform."""
    ext = ".exe" if os.name == "nt" else ""
    return str(Path("build", "dev-mingw", f"blunder{ext}"))


def parse_args() -> argparse.Namespace:
    project_dir = Path(__file__).resolve().parent.parent
    engine = str(project_dir / default_engine())
    book = str(project_dir / "books" / "i-gm1950.bin")

    p = argparse.ArgumentParser(description="SPRT regression test via cutechess-cli")
    p.add_argument("--baseline", default=engine, help="Baseline engine binary")
    p.add_argument("--candidate", default=engine, help="Candidate engine binary")
    p.add_argument("--tc", default="10+0.1", help="Time control (default: 10+0.1)")
    p.add_argument("--book", default=book, help="Opening book path")
    p.add_argument("--book-depth", type=int, default=4, help="Book depth (default: 4)")
    p.add_argument("--rounds", type=int, default=2500, help="Max rounds")
    p.add_argument("--concurrency", type=int, default=4, help="Parallel games")
    p.add_argument("--elo0", type=int, default=0, help="SPRT null hypothesis")
    p.add_argument("--elo1", type=int, default=10, help="SPRT alternative hypothesis")
    p.add_argument("--cutechess", default=None, help="Path to cutechess-cli")
    return p.parse_args()


def validate(args: argparse.Namespace) -> None:
    for label, path in [("Baseline", args.baseline), ("Candidate", args.candidate)]:
        if not Path(path).is_file():
            sys.exit(f"ERROR: {label} engine not found: {path}")
    if not Path(args.book).is_file():
        sys.exit(f"ERROR: Opening book not found: {args.book}")


def run_sprt(args: argparse.Namespace) -> int:
    cutechess = find_cutechess(args.cutechess)

    # Create timestamped output directory
    scripts_dir = Path(__file__).resolve().parent
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    output_dir = scripts_dir / "output" / timestamp
    output_dir.mkdir(parents=True, exist_ok=True)

    pgn_out = str(output_dir / "sprt.pgn")
    log_file = output_dir / "cutechess.log"

    cmd = [
        cutechess,
        "-engine", f"name=candidate", f"cmd={args.candidate}",
            "proto=xboard", "arg=--xboard",
            f"stderr={output_dir / 'candidate.err.log'}",
        "-engine", f"name=baseline", f"cmd={args.baseline}",
            "proto=xboard", "arg=--xboard",
            f"stderr={output_dir / 'baseline.err.log'}",
        "-each",
            f"tc={args.tc}",
            f"book={args.book}",
            f"bookdepth={args.book_depth}",
        "-games", "2",
        "-rounds", str(args.rounds),
        "-repeat", "2",
        "-maxmoves", "200",
        "-sprt", f"elo0={args.elo0}", f"elo1={args.elo1}",
            "alpha=0.05", "beta=0.05",
        "-concurrency", str(args.concurrency),
        "-ratinginterval", "10",
        "-pgnout", pgn_out,
        "-srand", "0",
    ]

    print("=== SPRT Regression Test ===")
    print(f"Cutechess:   {cutechess}")
    print(f"Baseline:    {args.baseline}")
    print(f"Candidate:   {args.candidate}")
    print(f"TC:          {args.tc}")
    print(f"Book:        {args.book}")
    print(f"Rounds:      {args.rounds}")
    print(f"Concurrency: {args.concurrency}")
    print(f"SPRT:        elo0={args.elo0} elo1={args.elo1}")
    print(f"Output:      {output_dir}")
    print("============================")
    print()

    # Run cutechess-cli, tee output to both console and log file
    with open(log_file, "w", encoding="utf-8") as lf:
        proc = subprocess.Popen(
            cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            text=True, encoding="utf-8", errors="replace",
        )
        assert proc.stdout is not None
        for line in proc.stdout:
            sys.stdout.write(line)
            sys.stdout.flush()
            lf.write(line)
        proc.wait()

    # Parse SPRT result from log
    log_text = log_file.read_text(encoding="utf-8")
    return parse_result(log_text)


def parse_result(log_text: str) -> int:
    """Parse the SPRT conclusion from cutechess-cli output. Returns exit code."""
    # Look for the final SPRT line
    sprt_lines = [l for l in log_text.splitlines() if "SPRT" in l.upper() or "Elo" in l]

    print()
    print("=== Result ===")

    # Find Elo difference line
    elo_match = re.search(
        r"Elo difference:\s*([-\d.]+)\s*\+/-\s*([\d.]+)", log_text
    )
    if elo_match:
        print(f"Elo difference: {elo_match.group(1)} +/- {elo_match.group(2)}")

    # Find SPRT conclusion
    if re.search(r"H0\b", log_text, re.IGNORECASE):
        print("REGRESSION DETECTED (H0 accepted)")
        return 1
    elif re.search(r"H1\b", log_text, re.IGNORECASE):
        print("IMPROVEMENT DETECTED (H1 accepted)")
        return 0
    else:
        # Print last few lines for context
        for line in sprt_lines[-3:]:
            print(line.strip())
        print("INCONCLUSIVE")
        return 0


def main() -> None:
    args = parse_args()
    validate(args)
    exit_code = run_sprt(args)
    sys.exit(exit_code)


if __name__ == "__main__":
    main()
