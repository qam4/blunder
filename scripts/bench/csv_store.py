"""CSV read/write helpers for benchmark result tracking.

Manages two CSV files in the output directory:
- ``benchmarks.csv`` — one row per (suite, mode) run
- ``benchmarks_categories.csv`` — one row per STS category per run

Auto-creates the output directory and CSV files with headers if missing.
Write failures are logged to stderr but never abort execution.
"""

from __future__ import annotations

import csv
import os
import sys
from pathlib import Path

# Backward-compatible column order for the main results file.
MAIN_COLUMNS = [
    "timestamp",
    "commit",
    "branch",
    "evaluator",
    "mode",
    "suite",
    "score_pct",
    "score",
    "max_score",
    "elo",
    "nps",
    "nodes",
    "time_secs",
    "passed",
]

# Column order for the per-category results file.
CATEGORY_COLUMNS = [
    "timestamp",
    "commit",
    "branch",
    "evaluator",
    "mode",
    "category",
    "score_pct",
    "score",
    "max_score",
    "positions",
]

MAIN_FILENAME = "benchmarks.csv"
CATEGORY_FILENAME = "benchmarks_categories.csv"


def _ensure_csv(path: Path, columns: list[str]) -> None:
    """Create the CSV file with a header row if it does not exist."""
    os.makedirs(path.parent, exist_ok=True)
    if not path.exists():
        with open(path, "w", newline="", encoding="utf-8") as fh:
            writer = csv.DictWriter(fh, fieldnames=columns)
            writer.writeheader()


def append_main_result(output_dir: Path, row: dict) -> None:
    """Append a single benchmark result row to ``benchmarks.csv``."""
    path = output_dir / MAIN_FILENAME
    try:
        _ensure_csv(path, MAIN_COLUMNS)
        with open(path, "a", newline="", encoding="utf-8") as fh:
            writer = csv.DictWriter(fh, fieldnames=MAIN_COLUMNS)
            writer.writerow(row)
    except OSError as exc:
        print(f"Warning: could not write to {path}: {exc}", file=sys.stderr)


def append_category_results(output_dir: Path, rows: list[dict]) -> None:
    """Append STS per-category rows to ``benchmarks_categories.csv``."""
    path = output_dir / CATEGORY_FILENAME
    try:
        _ensure_csv(path, CATEGORY_COLUMNS)
        with open(path, "a", newline="", encoding="utf-8") as fh:
            writer = csv.DictWriter(fh, fieldnames=CATEGORY_COLUMNS)
            for row in rows:
                writer.writerow(row)
    except OSError as exc:
        print(f"Warning: could not write to {path}: {exc}", file=sys.stderr)


def read_main_results(output_dir: Path) -> list[dict]:
    """Read all rows from ``benchmarks.csv`` as a list of dicts."""
    path = output_dir / MAIN_FILENAME
    if not path.exists():
        return []
    with open(path, newline="", encoding="utf-8") as fh:
        reader = csv.DictReader(fh)
        return list(reader)


def get_previous_result(
    output_dir: Path, suite: str, mode: str, evaluator: str
) -> dict | None:
    """Return the most recent row matching *suite*, *mode*, and *evaluator*.

    Scans ``benchmarks.csv`` and returns the last matching row (most recent
    by file order / append order).  Returns ``None`` if no match is found.
    """
    rows = read_main_results(output_dir)
    match: dict | None = None
    for row in rows:
        if (
            row.get("suite") == suite
            and row.get("mode") == mode
            and row.get("evaluator") == evaluator
        ):
            match = row
    return match

def read_category_results(output_dir: Path) -> list[dict]:
    """Read all rows from ``benchmarks_categories.csv`` as a list of dicts."""
    path = output_dir / CATEGORY_FILENAME
    if not path.exists():
        return []
    with open(path, newline="", encoding="utf-8") as fh:
        reader = csv.DictReader(fh)
        return list(reader)

