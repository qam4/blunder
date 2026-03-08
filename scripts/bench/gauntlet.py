"""Gauntlet runner — executes engine-vs-engine matches via fast-chess.

Handles fast-chess command construction, real-time stdout streaming with
simultaneous file logging, output parsing for W/L/D/Elo/SPRT, and saving
PGN + log to a timestamped output directory.
"""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
from datetime import datetime
from pathlib import Path

from bench.config import Config, EngineEntry, resolve_engine
from bench.parsers import GauntletResult, parse_fastchess_output


# ---------------------------------------------------------------------------
# Fast-chess command construction
# ---------------------------------------------------------------------------


def build_fastchess_cmd(
    fast_chess_path: str,
    engines: list[EngineEntry],
    tc: str,
    rounds: int,
    concurrency: int,
    book_path: str,
    book_depth: int,
    pgn_path: str,
    sprt_elo0: int | None = None,
    sprt_elo1: int | None = None,
    extra_args: str | None = None,
) -> list[str]:
    """Build the fast-chess command line from parameters.

    Returns a list of strings suitable for ``subprocess.Popen``.
    """
    cmd: list[str] = [fast_chess_path]

    # Engine definitions
    for eng in engines:
        engine_arg = f"cmd={eng.cmd} name={eng.name} proto={eng.protocol}"
        for opt_name, opt_value in eng.options.items():
            engine_arg += f" option.{opt_name}={opt_value}"
        for arg in eng.args:
            engine_arg += f" arg={arg}"
        cmd.extend(["-engine", engine_arg])

    # Time control
    cmd.extend(["-each", f"tc={tc}"])

    # Rounds and concurrency
    cmd.extend(["-rounds", str(rounds)])
    cmd.extend(["-concurrency", str(concurrency)])

    # Opening book
    cmd.extend([
        "-openings",
        f"file={book_path}",
        "format=pgn",
        "order=random",
        f"plies={book_depth}",
    ])

    # SPRT or roundrobin
    if sprt_elo0 is not None and sprt_elo1 is not None:
        cmd.extend([
            "-sprt",
            f"elo0={sprt_elo0}",
            f"elo1={sprt_elo1}",
            "alpha=0.05",
            "beta=0.05",
        ])
    else:
        cmd.extend(["-tournament", "roundrobin"])

    # PGN output
    cmd.extend(["-pgnout", pgn_path])

    # Extra args (split by spaces and appended as-is)
    if extra_args:
        cmd.extend(extra_args.split())

    return cmd


# ---------------------------------------------------------------------------
# Output directory helpers
# ---------------------------------------------------------------------------


def _create_output_dir(project_root: Path) -> Path:
    """Create a timestamped output directory for gauntlet results.

    Returns the path ``scripts/output/gauntlet_{YYYYMMDD_HHMMSS}/``.
    """
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    out_dir = project_root / "scripts" / "output" / f"gauntlet_{timestamp}"
    os.makedirs(out_dir, exist_ok=True)
    return out_dir


# ---------------------------------------------------------------------------
# Main gauntlet runner
# ---------------------------------------------------------------------------


def cmd_gauntlet(config: Config, args: argparse.Namespace) -> int:
    """Execute bench gauntlet subcommand. Returns exit code."""
    gauntlet_type: str = getattr(args, "type", "sprt")
    dry_run: bool = getattr(args, "dry_run", False)
    show_cmd: bool = getattr(args, "show_cmd", False)

    # Resolve engines from registry
    if gauntlet_type == "roundrobin":
        engine_names: list[str] = getattr(args, "engines", None) or []
        if not engine_names:
            print(
                "Error: --engines is required for roundrobin tournaments",
                file=sys.stderr,
            )
            return 1
        engine_entries = [resolve_engine(config, name) for name in engine_names]
    else:
        # SPRT: baseline vs candidate
        baseline_name = getattr(args, "baseline", None) or config.defaults.baseline_engine
        candidate_name = getattr(args, "candidate", None) or config.defaults.candidate_engine
        engine_entries = [
            resolve_engine(config, baseline_name),
            resolve_engine(config, candidate_name),
        ]

    # Resolve parameters: CLI flags override config defaults
    tc = getattr(args, "tc", None) or config.defaults.tc
    rounds = getattr(args, "rounds", None) or config.defaults.rounds
    concurrency = getattr(args, "concurrency", None) or config.defaults.concurrency
    book_path = getattr(args, "book", None) or str(config.paths.opening_book)
    book_depth = getattr(args, "book_depth", None) or config.defaults.book_depth

    # SPRT bounds (only for sprt type)
    sprt_elo0: int | None = None
    sprt_elo1: int | None = None
    if gauntlet_type == "sprt":
        sprt_elo0 = getattr(args, "elo0", None) or config.defaults.sprt_elo0
        sprt_elo1 = getattr(args, "elo1", None) or config.defaults.sprt_elo1

    extra_args: str | None = getattr(args, "fast_chess_args", None)

    # Check fast-chess binary exists (skip in dry-run mode)
    fast_chess_path = str(config.paths.fast_chess)
    if not dry_run and not Path(fast_chess_path).exists():
        print(
            f"Error: fast-chess not found at: {fast_chess_path}",
            file=sys.stderr,
        )
        return 1

    # Create timestamped output directory
    out_dir = _create_output_dir(config.project_root)
    pgn_path = str(out_dir / "games.pgn")
    log_path = out_dir / "fastchess.log"

    # Build the fast-chess command
    cmd = build_fastchess_cmd(
        fast_chess_path=fast_chess_path,
        engines=engine_entries,
        tc=tc,
        rounds=rounds,
        concurrency=concurrency,
        book_path=book_path,
        book_depth=book_depth,
        pgn_path=pgn_path,
        sprt_elo0=sprt_elo0,
        sprt_elo1=sprt_elo1,
        extra_args=extra_args,
    )

    if getattr(args, "verbose", False) or show_cmd:
        print(f"$ {' '.join(cmd)}", file=sys.stderr)

    if dry_run:
        print("DRY RUN — command that would be executed:")
        print(f"  $ {' '.join(cmd)}")
        print(f"  PGN would be saved to: {pgn_path}")
        print(f"  Log would be saved to: {log_path}")
        return 0

    # Run fast-chess with real-time streaming and simultaneous file logging
    accumulated_output: list[str] = []
    try:
        proc = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )
    except FileNotFoundError:
        print(
            f"Error: fast-chess not found at: {fast_chess_path}",
            file=sys.stderr,
        )
        return 1

    with open(log_path, "w", encoding="utf-8") as log_file:
        assert proc.stdout is not None
        for line in proc.stdout:
            # Print to stdout in real time
            sys.stdout.write(line)
            sys.stdout.flush()
            # Accumulate for parsing
            accumulated_output.append(line)
            # Write to log file
            log_file.write(line)
            log_file.flush()

    return_code = proc.wait()

    if return_code != 0:
        print(
            f"Warning: fast-chess exited with code {return_code}",
            file=sys.stderr,
        )

    # Parse output
    full_output = "".join(accumulated_output)
    try:
        result: GauntletResult = parse_fastchess_output(full_output)
    except ValueError:
        print(
            "Warning: Could not parse fast-chess output",
            file=sys.stderr,
        )
        print(f"PGN saved to: {pgn_path}")
        print(f"Log saved to: {log_path}")
        return 1 if return_code != 0 else 0

    # Print summary
    total = result.wins + result.losses + result.draws
    print()
    print(f"Results: +{result.wins} -{result.losses} ={result.draws} ({total} games)")
    print(f"Elo difference: {result.elo_diff:+.1f} +/- {result.elo_error:.1f}")
    if result.sprt_conclusion:
        print(f"SPRT: {result.sprt_conclusion} accepted")
    print(f"PGN saved to: {pgn_path}")
    print(f"Log saved to: {log_path}")

    return 1 if return_code != 0 else 0
