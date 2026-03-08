"""Comparison tool for benchmark results.

Reads CSV result files and displays side-by-side comparison tables
with deltas, STS per-category breakdowns, and missing-data messages.
"""

from __future__ import annotations

import argparse

from bench.config import Config
from bench.csv_store import read_category_results, read_main_results


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _safe_float(val: str | None) -> float | None:
    """Convert a string to float, returning None on failure."""
    if val is None or val == "":
        return None
    try:
        return float(val)
    except (ValueError, TypeError):
        return None


def _safe_int(val: str | None) -> int | None:
    """Convert a string to int, returning None on failure."""
    if val is None or val == "":
        return None
    try:
        return int(float(val))
    except (ValueError, TypeError):
        return None


def _fmt_delta(cur: float | None, ref: float | None, fmt: str = ".1f") -> str:
    """Format a delta string like '+3.2' or '-1.0'. Empty if either is None."""
    if cur is None or ref is None:
        return ""
    d = cur - ref
    sign = "+" if d >= 0 else ""
    return f"{sign}{d:{fmt}}"


def classify_category(cat_pct: float, avg_pct: float) -> str:
    """Classify an STS category as strength, weakness, or neutral.

    A category is a "strength" if its score_pct exceeds the average by
    more than 5 percentage points, and a "weakness" if more than 5pp
    below average.
    """
    if cat_pct > avg_pct + 5.0:
        return "strength"
    if cat_pct < avg_pct - 5.0:
        return "weakness"
    return ""


# ---------------------------------------------------------------------------
# Pivoting / grouping
# ---------------------------------------------------------------------------


def _latest_by_evaluator(rows: list[dict]) -> dict[str, list[dict]]:
    """Group rows by evaluator, keeping only the latest timestamp per key.

    Returns ``{evaluator: [rows with that evaluator at latest timestamp]}``.
    """
    # Find the latest timestamp per evaluator
    latest_ts: dict[str, str] = {}
    for r in rows:
        ev = r.get("evaluator", "")
        ts = r.get("timestamp", "")
        if ev not in latest_ts or ts > latest_ts[ev]:
            latest_ts[ev] = ts

    grouped: dict[str, list[dict]] = {}
    for r in rows:
        ev = r.get("evaluator", "")
        ts = r.get("timestamp", "")
        if ts == latest_ts.get(ev):
            grouped.setdefault(ev, []).append(r)
    return grouped


def _rows_by_commit(rows: list[dict], commits: list[str]) -> dict[str, list[dict]]:
    """Filter rows matching the given commit hashes."""
    grouped: dict[str, list[dict]] = {}
    for r in rows:
        c = r.get("commit", "")
        if c in commits:
            grouped.setdefault(c, []).append(r)
    return grouped


def _rows_by_version(rows: list[dict]) -> dict[str, list[dict]]:
    """Group rows by commit (simple version comparison)."""
    grouped: dict[str, list[dict]] = {}
    for r in rows:
        c = r.get("commit", "")
        grouped.setdefault(c, []).append(r)
    return grouped


# ---------------------------------------------------------------------------
# Display helpers
# ---------------------------------------------------------------------------


def _make_key(row: dict) -> str:
    """Create a suite/mode key for matching rows across groups."""
    return f"{row.get('suite', '')}/{row.get('mode', '')}"


def _print_side_by_side(
    grouped: dict[str, list[dict]], axis_label: str
) -> None:
    """Print a side-by-side comparison table with deltas."""
    labels = list(grouped.keys())
    if len(labels) < 2:
        if len(labels) == 1:
            print(f"\n  Only one {axis_label} found ({labels[0]}). "
                  f"Need at least two for comparison.")
        return

    # Build lookup: label -> {suite/mode -> row}
    lookup: dict[str, dict[str, dict]] = {}
    all_keys: list[str] = []
    for label in labels:
        lk: dict[str, dict] = {}
        for r in grouped[label]:
            k = _make_key(r)
            lk[k] = r
            if k not in all_keys:
                all_keys.append(k)
        lookup[label] = lk

    # Header
    col_w = 18
    hdr = f"  {'Suite/Mode':<16}"
    for label in labels:
        hdr += f"  {label:>{col_w}}"
    if len(labels) == 2:
        hdr += f"  {'Delta':>{col_w}}"
    print(f"\n  Comparison by {axis_label}:")
    print(hdr)
    print("  " + "-" * (len(hdr) - 2))

    # Rows — show score_pct, elo, nps for each key
    for key in all_keys:
        # score_pct row
        line = f"  {key:<16}"
        vals: list[float | None] = []
        for label in labels:
            row = lookup[label].get(key)
            v = _safe_float(row.get("score_pct")) if row else None
            vals.append(v)
            cell = f"{v:.1f}%" if v is not None else "—"
            line += f"  {cell:>{col_w}}"
        if len(labels) == 2:
            delta = _fmt_delta(vals[1], vals[0])
            line += f"  {delta:>{col_w}}"
        print(line)

        # elo row
        line = f"  {'  ELO':<16}"
        elo_vals: list[float | None] = []
        for label in labels:
            row = lookup[label].get(key)
            v = _safe_float(row.get("elo")) if row else None
            elo_vals.append(v)
            cell = f"{int(v)}" if v is not None else "—"
            line += f"  {cell:>{col_w}}"
        if len(labels) == 2:
            delta = _fmt_delta(elo_vals[1], elo_vals[0], ".0f")
            line += f"  {delta:>{col_w}}"
        print(line)

        # nps row
        line = f"  {'  NPS':<16}"
        nps_vals: list[float | None] = []
        for label in labels:
            row = lookup[label].get(key)
            v = _safe_float(row.get("nps")) if row else None
            nps_vals.append(v)
            cell = f"{int(v):,}" if v is not None else "—"
            line += f"  {cell:>{col_w}}"
        if len(labels) == 2:
            delta = _fmt_delta(nps_vals[1], nps_vals[0], ".0f")
            line += f"  {delta:>{col_w}}"
        print(line)

    print()

def _print_category_comparison(
    cat_rows: list[dict],
    grouped_keys: dict[str, list[dict]],
    axis_label: str,
) -> None:
    """Print STS per-category breakdown with >5pp highlighting."""
    labels = list(grouped_keys.keys())
    if len(labels) < 2:
        return

    # Build per-label category lookup: label -> {category -> score_pct}
    # Match category rows using the same criteria as the main rows for this label
    cat_by_label: dict[str, dict[str, float]] = {}
    for label in labels:
        main_rows = grouped_keys[label]
        # Collect individual field sets for flexible matching
        commits = {r.get("commit", "") for r in main_rows}
        evaluators = {r.get("evaluator", "") for r in main_rows}

        cat_map: dict[str, float] = {}
        for cr in cat_rows:
            cr_commit = cr.get("commit", "")
            cr_eval = cr.get("evaluator", "")
            # Match: both commit AND evaluator must belong to this label
            if cr_commit in commits and cr_eval in evaluators:
                cat_name = cr.get("category", "")
                pct = _safe_float(cr.get("score_pct"))
                if pct is not None:
                    cat_map[cat_name] = pct
        cat_by_label[label] = cat_map

    # Collect all category names
    all_cats: list[str] = []
    for cm in cat_by_label.values():
        for c in cm:
            if c not in all_cats:
                all_cats.append(c)

    if not all_cats:
        return

    col_w = 18
    print(f"  STS Per-Category Comparison ({axis_label}):")
    hdr = f"  {'Category':<35}"
    for label in labels:
        hdr += f"  {label:>{col_w}}"
    if len(labels) == 2:
        hdr += f"  {'Delta':>{col_w}}  {'Note'}"
    print(hdr)
    print("  " + "-" * (len(hdr) - 2))

    for cat in all_cats:
        line = f"  {cat:<35}"
        vals: list[float | None] = []
        for label in labels:
            v = cat_by_label[label].get(cat)
            vals.append(v)
            cell = f"{v:.1f}%" if v is not None else "—"
            line += f"  {cell:>{col_w}}"
        if len(labels) == 2:
            delta_str = _fmt_delta(vals[1], vals[0])
            line += f"  {delta_str:>{col_w}}"
            # Highlight >5pp difference
            if vals[0] is not None and vals[1] is not None:
                diff = abs(vals[1] - vals[0])
                if diff > 5.0:
                    line += "  ⚠ >5pp"
        print(line)

    # Show strength/weakness classification per label
    for label in labels:
        cm = cat_by_label[label]
        if not cm:
            continue
        avg = sum(cm.values()) / len(cm)
        strengths = [c for c, v in cm.items() if classify_category(v, avg) == "strength"]
        weaknesses = [c for c, v in cm.items() if classify_category(v, avg) == "weakness"]
        if strengths or weaknesses:
            print(f"\n  {label}:")
            if strengths:
                print(f"    Strengths (>5pp above avg {avg:.1f}%): {', '.join(strengths)}")
            if weaknesses:
                print(f"    Weaknesses (>5pp below avg {avg:.1f}%): {', '.join(weaknesses)}")

    print()


# ---------------------------------------------------------------------------
# Main command
# ---------------------------------------------------------------------------


def cmd_compare(config: Config, args: argparse.Namespace) -> int:
    """Execute bench compare subcommand. Returns exit code."""
    output_dir = config.project_root / "scripts" / "output"
    by = getattr(args, "by", "evaluator")
    commits_arg: list[str] | None = getattr(args, "commits", None)

    # Read CSV data
    main_rows = read_main_results(output_dir)
    cat_rows = read_category_results(output_dir)

    if not main_rows:
        print("No benchmark results found. Run 'bench run' first to generate data.")
        return 1

    if by == "evaluator":
        grouped = _latest_by_evaluator(main_rows)
        if not grouped:
            print("No results found for any evaluator.")
            return 1
        _print_side_by_side(grouped, "evaluator")
        if cat_rows:
            _print_category_comparison(cat_rows, grouped, "evaluator")

    elif by == "commit":
        if not commits_arg or len(commits_arg) < 2:
            print("Error: --by commit requires --commits with two or more hashes.")
            return 1
        grouped = _rows_by_commit(main_rows, commits_arg)
        # Check for missing commits
        missing = [c for c in commits_arg if c not in grouped]
        if missing:
            print(f"Warning: No results found for commit(s): {', '.join(missing)}")
        if len(grouped) < 2:
            found = list(grouped.keys())
            print(f"Cannot compare: need results for at least 2 commits, "
                  f"found {len(found)} ({', '.join(found) if found else 'none'}).")
            return 1
        _print_side_by_side(grouped, "commit")
        if cat_rows:
            _print_category_comparison(cat_rows, grouped, "commit")

    elif by == "version":
        grouped = _rows_by_version(main_rows)
        if len(grouped) < 2:
            print("Not enough distinct versions (commits) to compare. "
                  "Need at least 2 different commits in results.")
            return 1
        # Take the two most recent by timestamp
        sorted_keys = sorted(
            grouped.keys(),
            key=lambda k: max(r.get("timestamp", "") for r in grouped[k]),
            reverse=True,
        )
        selected = {k: grouped[k] for k in sorted_keys[:2]}
        _print_side_by_side(selected, "version")
        if cat_rows:
            _print_category_comparison(cat_rows, selected, "version")

    else:
        print(f"Error: Unknown comparison axis '{by}'. Use evaluator, commit, or version.")
        return 1

    return 0
