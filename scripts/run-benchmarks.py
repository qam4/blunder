#!/usr/bin/env python3
"""
WAC/STS solve rate benchmark runner.

Runs the engine's built-in WAC and STS test suites via the Catch2 test binary,
parses the score and ELO estimate, and logs results with the current git commit
hash for historical tracking.

Usage:
    python scripts/run-benchmarks.py [options]

Results are appended to scripts/output/benchmarks.csv for trend tracking.
"""

import argparse
import csv
import os
import re
import subprocess
import sys
from datetime import datetime
from pathlib import Path


def default_test_binary() -> str:
    """Return default test binary path based on platform."""
    ext = ".exe" if os.name == "nt" else ""
    return str(Path("build", "dev", "test", f"blunder_test{ext}"))


def get_git_info() -> tuple[str, str]:
    """Return (short commit hash, branch name)."""
    try:
        commit = subprocess.check_output(
            ["git", "rev-parse", "--short", "HEAD"],
            text=True, stderr=subprocess.DEVNULL,
        ).strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        commit = "unknown"

    try:
        branch = subprocess.check_output(
            ["git", "rev-parse", "--abbrev-ref", "HEAD"],
            text=True, stderr=subprocess.DEVNULL,
        ).strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        branch = "unknown"

    return commit, branch


def run_test_suite(binary: str, suite: str) -> dict:
    """Run a single test suite and parse the output.

    Returns dict with keys: suite, score_pct, score, max_score, elo, passed.
    """
    result = {
        "suite": suite,
        "score_pct": 0.0,
        "score": 0,
        "max_score": 0,
        "elo": 0,
        "passed": False,
        "error": None,
    }

    tag = f"test_positions_{suite}"
    try:
        proc = subprocess.run(
            [binary, tag, "--reporter", "compact"],
            capture_output=True, text=True, timeout=1800,  # 30 min max
        )
        output = proc.stdout + proc.stderr
    except subprocess.TimeoutExpired:
        result["error"] = "timeout (30 min)"
        return result
    except FileNotFoundError:
        result["error"] = f"binary not found: {binary}"
        return result

    # Parse "Score=XX.XX% (NNN/MMM)"
    score_match = re.search(
        r"Score=([\d.]+)%\s*\((\d+)/(\d+)\)", output
    )
    if score_match:
        result["score_pct"] = float(score_match.group(1))
        result["score"] = int(score_match.group(2))
        result["max_score"] = int(score_match.group(3))

    # Parse "ELO=NNNN"
    elo_match = re.search(r"ELO=(-?\d+)", output)
    if elo_match:
        result["elo"] = int(elo_match.group(1))

    # Check if the test passed
    result["passed"] = proc.returncode == 0

    return result


def load_previous(csv_path: Path, suite: str) -> dict | None:
    """Load the most recent result for a suite from the CSV log."""
    if not csv_path.is_file():
        return None

    last = None
    with open(csv_path, newline="", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            if row["suite"] == suite:
                last = row
    return last


def check_regression(current: dict, previous: dict | None) -> str | None:
    """Check for significant solve rate drops. Returns warning string or None."""
    if previous is None:
        return None

    prev_pct = float(previous["score_pct"])
    curr_pct = current["score_pct"]
    drop = prev_pct - curr_pct

    suite = current["suite"]
    if suite == "WAC" and drop > 5.0:
        return f"WAC regression: {prev_pct:.1f}% -> {curr_pct:.1f}% (drop: {drop:.1f}%)"
    elif suite == "STS-Rating" and drop > 4.2:
        # 4.2% of 118800 ≈ 50 points on the STS scale
        return f"STS regression: {prev_pct:.1f}% -> {curr_pct:.1f}% (drop: {drop:.1f}%)"

    return None


def append_csv(csv_path: Path, row: dict) -> None:
    """Append a result row to the CSV log."""
    csv_path.parent.mkdir(parents=True, exist_ok=True)
    write_header = not csv_path.is_file()

    fields = ["timestamp", "commit", "branch", "suite",
              "score_pct", "score", "max_score", "elo", "passed"]
    with open(csv_path, "a", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        if write_header:
            writer.writeheader()
        writer.writerow({k: row[k] for k in fields})


def parse_args() -> argparse.Namespace:
    project_dir = Path(__file__).resolve().parent.parent
    binary = str(project_dir / default_test_binary())

    p = argparse.ArgumentParser(description="WAC/STS solve rate benchmarks")
    p.add_argument("--binary", default=binary, help="Test binary path")
    p.add_argument("--suite", default="all", choices=["all", "WAC", "STS"],
                   help="Which suite to run (default: all)")
    p.add_argument("--no-log", action="store_true",
                   help="Don't append results to CSV log")
    return p.parse_args()


def main() -> None:
    args = parse_args()

    if not Path(args.binary).is_file():
        sys.exit(f"ERROR: Test binary not found: {args.binary}")

    commit, branch = get_git_info()
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    csv_path = Path(__file__).resolve().parent / "output" / "benchmarks.csv"

    suites = []
    if args.suite in ("all", "WAC"):
        suites.append("WAC")
    if args.suite in ("all", "STS"):
        suites.append("STS-Rating")

    print("=" * 60)
    print(f"Solve Rate Benchmarks — {commit} ({branch})")
    print("=" * 60)
    print()

    warnings = []
    exit_code = 0

    for suite in suites:
        print(f"Running {suite}...", flush=True)
        result = run_test_suite(args.binary, suite)

        if result["error"]:
            print(f"  ERROR: {result['error']}")
            exit_code = 1
            continue

        print(f"  Score: {result['score_pct']:.2f}% ({result['score']}/{result['max_score']})")
        print(f"  ELO:   {result['elo']}")
        print(f"  Pass:  {'YES' if result['passed'] else 'NO'}")
        print()

        # Check for regression
        if not args.no_log:
            previous = load_previous(csv_path, suite)
            warning = check_regression(result, previous)
            if warning:
                warnings.append(warning)

            # Log result
            row = {
                "timestamp": timestamp,
                "commit": commit,
                "branch": branch,
                **result,
            }
            append_csv(csv_path, row)

    if warnings:
        print("=" * 60)
        print("WARNINGS:")
        for w in warnings:
            print(f"  ⚠ {w}")
        print("=" * 60)
        exit_code = 1

    if not args.no_log:
        print(f"Results logged to {csv_path}")

    sys.exit(exit_code)


if __name__ == "__main__":
    main()
