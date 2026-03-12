"""Benchmark runner — executes WAC/STS test suites via the Catch2 test binary.

Handles suite/mode expansion, subprocess invocation, output parsing,
summary table printing, CSV logging, and regression detection.

Rich integration provides:
- Live progress bar with ETA for multi-step runs
- Per-step elapsed time and status
- Color-coded regression warnings
- Summary table with conditional formatting
"""

from __future__ import annotations

import argparse
import subprocess
import sys
import time
from datetime import datetime
from itertools import product
from pathlib import Path

from bench.config import Config
from bench.csv_store import append_category_results, append_main_result, get_previous_result
from bench.git_meta import get_git_info
from bench.parsers import BenchmarkResult, parse_benchmark_output

# ---------------------------------------------------------------------------
# Rich logging (optional — falls back to plain if not installed)
# ---------------------------------------------------------------------------

try:
    from rich.console import Console
    from rich.progress import (
        BarColumn,
        MofNCompleteColumn,
        Progress,
        SpinnerColumn,
        TextColumn,
        TimeElapsedColumn,
        TimeRemainingColumn,
    )
    from rich.table import Table
    from rich.text import Text

    _RICH = True
    _console = Console(stderr=True)
except ImportError:
    _RICH = False
    _console = None  # type: ignore[assignment]


def _log(msg: str, style: str = "") -> None:
    """Log a message to stderr, using rich if available."""
    if _RICH:
        _console.print(msg, style=style)
    else:
        print(msg, file=sys.stderr)


def _log_step(idx: int, total: int, suite: str, mode: str) -> None:
    """Log the start of a benchmark step (non-progress-bar mode)."""
    tag = f"[{idx}/{total}]"
    if _RICH:
        _console.print(
            f"  [bold cyan]{tag}[/] Running [bold]{suite.upper()}[/] / "
            f"[bold]{mode}[/] …"
        )
    else:
        print(f"  {tag} Running {suite.upper()} / {mode} …", file=sys.stderr)


def _log_done(suite: str, mode: str, elapsed: float, result: BenchmarkResult) -> None:
    """Log completion of a benchmark step with timing."""
    pct = f"{result.score_pct:.1f}%"
    if _RICH:
        _console.print(
            f"       ✓ {suite.upper()}/{mode}: "
            f"[bold green]{result.score}/{result.max_score}[/] "
            f"({pct})  ELO {result.elo}  "
            f"[dim]{elapsed:.0f}s[/]"
        )
    else:
        print(
            f"       ✓ {suite.upper()}/{mode}: "
            f"{result.score}/{result.max_score} ({pct})  "
            f"ELO {result.elo}  {elapsed:.0f}s",
            file=sys.stderr,
        )


def _log_skip(suite: str, mode: str, reason: str) -> None:
    """Log a skipped benchmark step."""
    if _RICH:
        _console.print(
            f"       [yellow]⚠ {suite.upper()}/{mode}: {reason}[/]"
        )
    else:
        print(f"       ⚠ {suite.upper()}/{mode}: {reason}", file=sys.stderr)


def _log_warning(msg: str) -> None:
    """Log a warning."""
    if _RICH:
        _console.print(f"  [bold yellow]WARNING:[/] {msg}")
    else:
        print(f"  WARNING: {msg}", file=sys.stderr)


def _log_cmd(cmd: list[str]) -> None:
    """Log a command that is about to be executed."""
    cmd_str = " ".join(cmd)
    if _RICH:
        _console.print(f"  [dim]$ {cmd_str}[/]")
    else:
        print(f"  $ {cmd_str}", file=sys.stderr)


def _log_output_paths(output_dir: Path) -> None:
    """Print the paths to output files at the end of a run."""
    main_csv = output_dir / "benchmarks.csv"
    cat_csv = output_dir / "benchmarks_categories.csv"
    if _RICH:
        _console.print()
        _console.print("  [bold]Output files:[/]")
        _console.print(f"    Results:    [link=file://{main_csv}]{main_csv}[/]")
        if cat_csv.exists():
            _console.print(f"    Categories: [link=file://{cat_csv}]{cat_csv}[/]")
    else:
        print(file=sys.stderr)
        print("  Output files:", file=sys.stderr)
        print(f"    Results:    {main_csv}", file=sys.stderr)
        if cat_csv.exists():
            print(f"    Categories: {cat_csv}", file=sys.stderr)


# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

ALL_SUITES = ["wac", "sts"]
ALL_MODES = ["nodes", "depth", "time"]

# Catch2 test name mapping: (suite, mode) -> test name
_TEST_NAME_MAP: dict[tuple[str, str], str] = {
    ("wac", "nodes"): "test_positions_WAC",
    ("wac", "depth"): "test_positions_WAC_depth8",
    ("wac", "time"):  "test_positions_WAC_time1s",
    ("sts", "nodes"): "test_positions_STS-Rating",
    ("sts", "depth"): "test_positions_STS_depth8",
    ("sts", "time"):  "test_positions_STS_time1s",
}

# Mode strings for CSV (matching existing format)
_CSV_MODE_MAP: dict[str, str] = {
    "nodes": "nodes",
    "depth": "depth8",
    "time":  "time1s",
}

# Suite names for CSV (uppercase, matching existing format)
_CSV_SUITE_MAP: dict[str, str] = {
    "wac": "WAC",
    "sts": "STS",
}

# Regression thresholds
_WAC_REGRESSION_PP = 2.0    # percentage points
_STS_REGRESSION_PTS = 50    # raw score points
_NPS_REGRESSION_PCT = 0.10  # 10% drop


# ---------------------------------------------------------------------------
# Suite/mode expansion
# ---------------------------------------------------------------------------

def expand_suites_modes(suite: str, mode: str) -> list[tuple[str, str]]:
    """Expand suite/mode flags into a list of (suite, mode) pairs.

    ``"all"`` expands to all valid values for that dimension.
    Returns the Cartesian product of expanded suites × expanded modes.
    """
    suites = ALL_SUITES if suite == "all" else [suite]
    modes = ALL_MODES if mode == "all" else [mode]
    return list(product(suites, modes))


# ---------------------------------------------------------------------------
# Streaming subprocess runner
# ---------------------------------------------------------------------------

def _run_test_streaming(
    cmd: list[str], verbose: bool, timeout: int = 600
) -> tuple[int, str]:
    """Run a Catch2 test binary, streaming output line-by-line.

    Returns (return_code, accumulated_stdout).
    Streams each line to stderr so the user sees progress in real time.
    """
    try:
        proc = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )
    except FileNotFoundError:
        return (-1, "")

    lines: list[str] = []
    assert proc.stdout is not None
    start = time.monotonic()
    for line in proc.stdout:
        lines.append(line)
        elapsed = time.monotonic() - start
        # Always write raw output to stdout so external monitors see activity
        sys.stdout.write(line)
        sys.stdout.flush()
        if verbose:
            sys.stderr.write(f"       │ {line.rstrip()}\n")
            sys.stderr.flush()
        # Timeout check (approximate — only between lines)
        if elapsed > timeout:
            proc.kill()
            proc.wait()
            return (-2, "".join(lines))

    rc = proc.wait()
    return (rc, "".join(lines))


# ---------------------------------------------------------------------------
# Regression detection
# ---------------------------------------------------------------------------

def _check_regression(
    suite: str,
    mode: str,
    evaluator: str,
    result: BenchmarkResult,
    output_dir: Path,
) -> list[str]:
    """Compare against previous result and warn on regressions.

    Returns a list of warning strings (empty if no regressions).
    """
    warnings: list[str] = []
    prev = get_previous_result(
        output_dir, _CSV_SUITE_MAP[suite], _CSV_MODE_MAP[mode], evaluator
    )
    if prev is None:
        return warnings

    # WAC: warn if score_pct drops by more than 2 percentage points
    if suite == "wac":
        try:
            prev_pct = float(prev["score_pct"])
            delta = result.score_pct - prev_pct
            if delta < -_WAC_REGRESSION_PP:
                msg = (
                    f"WAC regression! "
                    f"Previous: {prev_pct:.1f}%, New: {result.score_pct:.1f}%, "
                    f"Delta: {delta:+.1f}pp"
                )
                warnings.append(msg)
                _log_warning(msg)
        except (KeyError, ValueError):
            pass

    # STS: warn if raw score drops by more than 50 points
    if suite == "sts":
        try:
            prev_score = int(prev["score"])
            delta = result.score - prev_score
            if delta < -_STS_REGRESSION_PTS:
                msg = (
                    f"STS regression! "
                    f"Previous: {prev_score}, New: {result.score}, "
                    f"Delta: {delta:+d}"
                )
                warnings.append(msg)
                _log_warning(msg)
        except (KeyError, ValueError):
            pass

    # NPS: warn if NPS drops by more than 10%
    try:
        prev_nps = int(prev["nps"])
        if prev_nps > 0 and result.nps > 0:
            drop_pct = (prev_nps - result.nps) / prev_nps
            if drop_pct > _NPS_REGRESSION_PCT:
                msg = (
                    f"NPS regression! "
                    f"Previous: {prev_nps}, New: {result.nps}, "
                    f"Delta: {result.nps - prev_nps:+d} ({-drop_pct * 100:.1f}%)"
                )
                warnings.append(msg)
                _log_warning(msg)
    except (KeyError, ValueError):
        pass

    return warnings


# ---------------------------------------------------------------------------
# Summary printing
# ---------------------------------------------------------------------------

def _print_summary_table(results: list[dict]) -> None:
    """Print a summary table to stdout."""
    if not results:
        return

    if _RICH:
        table = Table(title="Benchmark Results", show_lines=False)
        table.add_column("Suite", style="bold")
        table.add_column("Mode", style="bold")
        table.add_column("Score", justify="right")
        table.add_column("Pct", justify="right")
        table.add_column("ELO", justify="right")
        table.add_column("NPS", justify="right")
        table.add_column("Time", justify="right")
        for r in results:
            pct = f"{r['score_pct']:.1f}%"
            pct_style = "green" if r["score_pct"] >= 60 else ("yellow" if r["score_pct"] >= 50 else "red")
            nps_str = f"{r['nps']:,}" if r["nps"] else "—"
            time_str = f"{r['elapsed']:.0f}s" if r.get("elapsed") else "—"
            table.add_row(
                r["suite"],
                r["mode"],
                f"{r['score']}/{r['max_score']}",
                Text(pct, style=pct_style),
                str(r["elo"]),
                nps_str,
                time_str,
            )
        console_out = Console()
        console_out.print()
        console_out.print(table)
        console_out.print()
    else:
        header = f"{'Suite':<8} {'Mode':<8} {'Score':<12} {'Pct':>7} {'ELO':>6} {'NPS':>12} {'Time':>8}"
        print()
        print(header)
        print("-" * len(header))
        for r in results:
            score_str = f"{r['score']}/{r['max_score']}"
            time_str = f"{r['elapsed']:.0f}s" if r.get("elapsed") else "—"
            print(
                f"{r['suite']:<8} {r['mode']:<8} {score_str:<12} "
                f"{r['score_pct']:>6.1f}% {r['elo']:>6} {r['nps']:>12} {time_str:>8}"
            )
        print()


def _print_category_breakdown(result: BenchmarkResult) -> None:
    """Print STS per-category breakdown with strength/weakness markers."""
    if not result.categories:
        return

    # Compute overall average percentage
    total_score = sum(c.score for c in result.categories.values())
    total_max = sum(c.max_score for c in result.categories.values())
    avg_pct = (total_score / total_max * 100) if total_max > 0 else 0.0

    if _RICH:
        table = Table(title="STS Per-Category Breakdown", show_lines=False)
        table.add_column("Category", style="bold", min_width=30)
        table.add_column("Score", justify="right")
        table.add_column("Pct", justify="right")
        table.add_column("Note")
        for name, cat in result.categories.items():
            cat_pct = (cat.score / cat.max_score * 100) if cat.max_score > 0 else 0.0
            note = ""
            note_style = ""
            if cat_pct > avg_pct + 5.0:
                note = "★ strength"
                note_style = "green"
            elif cat_pct < avg_pct - 5.0:
                note = "▼ weakness"
                note_style = "red"
            table.add_row(
                name,
                f"{cat.score}/{cat.max_score}",
                f"{cat_pct:.1f}%",
                Text(note, style=note_style),
            )
        table.add_row("Average", "", f"{avg_pct:.1f}%", "", style="dim")
        Console().print(table)
    else:
        header = f"  {'Category':<35} {'Score':<12} {'Pct':>7}  {'Note'}"
        print("  STS Per-Category Breakdown:")
        print(header)
        print("  " + "-" * (len(header) - 2))
        for name, cat in result.categories.items():
            cat_pct = (cat.score / cat.max_score * 100) if cat.max_score > 0 else 0.0
            score_str = f"{cat.score}/{cat.max_score}"
            note = ""
            if cat_pct > avg_pct + 5.0:
                note = "★ strength"
            elif cat_pct < avg_pct - 5.0:
                note = "▼ weakness"
            print(f"  {name:<35} {score_str:<12} {cat_pct:>6.1f}%  {note}")
        print(f"  {'Average':<35} {'':<12} {avg_pct:>6.1f}%")
        print()


# ---------------------------------------------------------------------------
# CSV logging
# ---------------------------------------------------------------------------

def _log_main_result(
    output_dir: Path,
    suite: str,
    mode: str,
    evaluator: str,
    result: BenchmarkResult,
    git_commit: str,
    git_branch: str,
    timestamp: str,
) -> None:
    """Append a main result row to benchmarks.csv."""
    row = {
        "timestamp": timestamp,
        "commit": git_commit,
        "branch": git_branch,
        "evaluator": evaluator,
        "mode": _CSV_MODE_MAP[mode],
        "suite": _CSV_SUITE_MAP[suite],
        "score_pct": f"{result.score_pct:.1f}",
        "score": str(result.score),
        "max_score": str(result.max_score),
        "elo": str(result.elo),
        "nps": str(result.nps),
        "nodes": str(result.nodes),
        "time_secs": f"{result.time_secs:.1f}",
        "passed": "True",
    }
    append_main_result(output_dir, row)


def _log_category_results(
    output_dir: Path,
    mode: str,
    evaluator: str,
    result: BenchmarkResult,
    git_commit: str,
    git_branch: str,
    timestamp: str,
) -> None:
    """Append STS per-category rows to benchmarks_categories.csv."""
    if not result.categories:
        return

    rows = []
    for name, cat in result.categories.items():
        cat_pct = (cat.score / cat.max_score * 100) if cat.max_score > 0 else 0.0
        rows.append({
            "timestamp": timestamp,
            "commit": git_commit,
            "branch": git_branch,
            "evaluator": evaluator,
            "mode": _CSV_MODE_MAP[mode],
            "category": name,
            "score_pct": f"{cat_pct:.1f}",
            "score": str(cat.score),
            "max_score": str(cat.max_score),
            "positions": str(cat.positions),
        })
    append_category_results(output_dir, rows)


# ---------------------------------------------------------------------------
# Human-readable report writer
# ---------------------------------------------------------------------------

def _write_report(
    output_dir: Path,
    evaluator: str,
    git_commit: str,
    git_branch: str,
    timestamp: str,
    total_elapsed: float,
    summary_rows: list[dict],
    category_results: list[tuple[str, str, BenchmarkResult]],
    regression_warnings: list[str],
) -> Path:
    """Write a human-readable results.txt report file.

    Returns the path to the written file.
    """
    report_path = output_dir / "results.txt"
    lines: list[str] = []

    lines.append("=" * 70)
    lines.append(f"  Blunder Benchmark Report")
    lines.append(f"  {timestamp}")
    lines.append("=" * 70)
    lines.append("")
    lines.append(f"  Evaluator:  {evaluator}")
    lines.append(f"  Commit:     {git_commit}")
    lines.append(f"  Branch:     {git_branch}")
    lines.append(f"  Total time: {total_elapsed:.0f}s")
    lines.append("")

    # Summary table
    if summary_rows:
        lines.append("-" * 70)
        lines.append(f"  {'Suite':<8} {'Mode':<8} {'Score':<12} {'Pct':>7} {'ELO':>6} {'NPS':>12} {'Time':>8}")
        lines.append("-" * 70)
        for r in summary_rows:
            score_str = f"{r['score']}/{r['max_score']}"
            nps_str = f"{r['nps']:,}" if r["nps"] else "—"
            time_str = f"{r['elapsed']:.0f}s" if r.get("elapsed") else "—"
            lines.append(
                f"  {r['suite']:<8} {r['mode']:<8} {score_str:<12} "
                f"{r['score_pct']:>6.1f}% {r['elo']:>6} {nps_str:>12} {time_str:>8}"
            )
        lines.append("")

    # STS category breakdowns
    for suite, mode, result in category_results:
        if not result.categories:
            continue
        total_score = sum(c.score for c in result.categories.values())
        total_max = sum(c.max_score for c in result.categories.values())
        avg_pct = (total_score / total_max * 100) if total_max > 0 else 0.0

        lines.append(f"  STS Categories ({mode}):")
        lines.append(f"  {'Category':<35} {'Score':<12} {'Pct':>7}  {'Note'}")
        lines.append("  " + "-" * 65)
        for name, cat in result.categories.items():
            cat_pct = (cat.score / cat.max_score * 100) if cat.max_score > 0 else 0.0
            score_str = f"{cat.score}/{cat.max_score}"
            note = ""
            if cat_pct > avg_pct + 5.0:
                note = "★ strength"
            elif cat_pct < avg_pct - 5.0:
                note = "▼ weakness"
            lines.append(f"  {name:<35} {score_str:<12} {cat_pct:>6.1f}%  {note}")
        lines.append(f"  {'Average':<35} {'':<12} {avg_pct:>6.1f}%")
        lines.append("")

    # Regression warnings
    if regression_warnings:
        lines.append("-" * 70)
        lines.append("  REGRESSION WARNINGS:")
        for w in regression_warnings:
            lines.append(f"    ⚠ {w}")
        lines.append("")

    lines.append("=" * 70)

    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return report_path


# ---------------------------------------------------------------------------
# Progress bar helper
# ---------------------------------------------------------------------------

def _make_progress() -> Progress | None:
    """Create a rich Progress bar, or return None if rich is unavailable."""
    if not _RICH:
        return None
    return Progress(
        SpinnerColumn(),
        TextColumn("[bold blue]{task.description}"),
        BarColumn(bar_width=30),
        MofNCompleteColumn(),
        TextColumn("•"),
        TimeElapsedColumn(),
        TextColumn("•"),
        TimeRemainingColumn(),
        console=_console,
        transient=False,
    )


# ---------------------------------------------------------------------------
# Main runner
# ---------------------------------------------------------------------------

def cmd_run(config: Config, args: argparse.Namespace) -> int:
    """Execute bench run subcommand. Returns exit code (0=success)."""
    suite_flag: str = getattr(args, "suite", "all")
    mode_flag: str = getattr(args, "mode", "all")
    evaluator: str = getattr(args, "evaluator", None) or config.defaults.evaluator
    verbose: bool = getattr(args, "verbose", False)
    dry_run: bool = getattr(args, "dry_run", False)
    show_cmd: bool = getattr(args, "show_cmd", False)

    pairs = expand_suites_modes(suite_flag, mode_flag)
    test_binary = str(config.paths.test_binary)
    output_dir = config.project_root / "scripts" / "output"

    # Per-suite timeouts from config
    timeout_map = {
        "wac": config.defaults.timeout_wac,
        "sts": config.defaults.timeout_sts,
    }

    # Collect git metadata once
    git_info = get_git_info(config.project_root)
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

    total = len(pairs)

    # Header
    _log("")
    if _RICH:
        _console.rule("[bold]Benchmark Run[/]", style="cyan")
    _log(
        f"  Evaluator:   [bold]{evaluator}[/]" if _RICH else f"  Evaluator:   {evaluator}"
    )
    _log(
        f"  Commit:      [dim]{git_info.commit}[/] ({git_info.branch})"
        if _RICH else
        f"  Commit:      {git_info.commit} ({git_info.branch})"
    )
    _log(f"  Steps:       {total}")
    _log(f"  Test binary: {test_binary}")
    _log("")

    # Dry-run mode: just print commands and exit
    if dry_run:
        if _RICH:
            _console.print("  [bold yellow]DRY RUN[/] — commands that would be executed:")
        else:
            print("  DRY RUN — commands that would be executed:", file=sys.stderr)
        _log("")
        for suite, mode in pairs:
            test_name = _TEST_NAME_MAP.get((suite, mode))
            if test_name is None:
                continue
            cmd = [test_binary, test_name, "--reporter", "compact"]
            cmd_str = " ".join(cmd)
            if _RICH:
                _console.print(f"  [dim]$[/] {cmd_str}")
            else:
                print(f"  $ {cmd_str}", file=sys.stderr)
        _log("")
        return 0

    summary_rows: list[dict] = []
    category_results: list[tuple[str, str, BenchmarkResult]] = []
    regression_warnings: list[str] = []
    any_success = False
    overall_start = time.monotonic()

    # Use rich progress bar if available
    progress = _make_progress()

    if progress is not None:
        with progress:
            task_id = progress.add_task("Benchmarking", total=total)
            for idx, (suite, mode) in enumerate(pairs, 1):
                _run_single_step(
                    idx, total, suite, mode, test_binary, evaluator,
                    verbose, show_cmd, timeout_map, output_dir,
                    git_info, timestamp, summary_rows, category_results,
                    regression_warnings, progress, task_id,
                )
                if summary_rows and summary_rows[-1].get("_ok"):
                    any_success = True
                progress.update(task_id, advance=1,
                                description=f"Done {suite.upper()}/{mode}")
    else:
        # Plain fallback without progress bar
        for idx, (suite, mode) in enumerate(pairs, 1):
            _run_single_step(
                idx, total, suite, mode, test_binary, evaluator,
                verbose, show_cmd, timeout_map, output_dir,
                git_info, timestamp, summary_rows, category_results,
                regression_warnings, None, None,
            )
            if summary_rows and summary_rows[-1].get("_ok"):
                any_success = True

    total_elapsed = time.monotonic() - overall_start

    # Done message
    _log("")
    if _RICH:
        _console.rule(f"[bold]Done in {total_elapsed:.0f}s[/]", style="green")
    else:
        _log(f"  Done in {total_elapsed:.0f}s")

    # Clean internal flags from summary rows before display
    for r in summary_rows:
        r.pop("_ok", None)

    # Print summary table
    _print_summary_table(summary_rows)

    # Print STS category breakdowns
    for suite, mode, result in category_results:
        _print_category_breakdown(result)

    # Write human-readable report
    report_path = _write_report(
        output_dir, evaluator, git_info.commit, git_info.branch,
        timestamp, total_elapsed, summary_rows, category_results,
        regression_warnings,
    )

    # Print output file locations
    _log_output_paths(output_dir)
    if _RICH:
        _console.print(f"    Report:     [link=file://{report_path}]{report_path}[/]")
    else:
        print(f"    Report:     {report_path}", file=sys.stderr)
    _log("")

    return 0 if any_success else 1


def _run_single_step(
    idx: int,
    total: int,
    suite: str,
    mode: str,
    test_binary: str,
    evaluator: str,
    verbose: bool,
    show_cmd: bool,
    timeout_map: dict[str, int],
    output_dir: Path,
    git_info: object,
    timestamp: str,
    summary_rows: list[dict],
    category_results: list[tuple[str, str, BenchmarkResult]],
    regression_warnings: list[str],
    progress: Progress | None,
    task_id: object | None,
) -> None:
    """Run a single benchmark step (one suite/mode pair)."""
    test_name = _TEST_NAME_MAP.get((suite, mode))
    if test_name is None:
        _log_skip(suite, mode, "no test name mapping")
        return

    cmd = [test_binary, test_name, "--reporter", "compact"]

    # Update progress bar description
    if progress is not None and task_id is not None:
        progress.update(task_id, description=f"Running {suite.upper()}/{mode}")

    if show_cmd:
        _log_cmd(cmd)

    if progress is None:
        _log_step(idx, total, suite, mode)

    step_start = time.monotonic()
    timeout = timeout_map.get(suite, 600)
    rc, stdout = _run_test_streaming(cmd, verbose, timeout=timeout)
    elapsed = time.monotonic() - step_start

    if rc == -1:
        _log_skip(suite, mode, f"binary not found: {test_binary}")
        return
    if rc == -2:
        _log_skip(suite, mode, f"timed out after {timeout}s")
        return
    if rc != 0:
        _log_warning(f"{test_name} exited with code {rc}")

    # Parse output
    try:
        result = parse_benchmark_output(stdout)
    except ValueError:
        _log_skip(suite, mode, "could not parse output")
        return

    _log_done(suite, mode, elapsed, result)

    # Regression detection (before logging, so we compare against old data)
    step_warnings = _check_regression(suite, mode, evaluator, result, output_dir)
    regression_warnings.extend(step_warnings)

    # CSV logging
    _log_main_result(
        output_dir, suite, mode, evaluator, result,
        git_info.commit, git_info.branch, timestamp,  # type: ignore[attr-defined]
    )

    if suite == "sts":
        _log_category_results(
            output_dir, mode, evaluator, result,
            git_info.commit, git_info.branch, timestamp,  # type: ignore[attr-defined]
        )
        category_results.append((suite, mode, result))

    # Collect for summary table (with internal _ok flag)
    summary_rows.append({
        "suite": _CSV_SUITE_MAP[suite],
        "mode": _CSV_MODE_MAP[mode],
        "score": result.score,
        "max_score": result.max_score,
        "score_pct": result.score_pct,
        "elo": result.elo,
        "nps": result.nps,
        "elapsed": elapsed,
        "_ok": True,
    })
