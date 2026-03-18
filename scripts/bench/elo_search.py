"""Elo search — iterative estimation of the engine's absolute Elo rating.

Two-phase approach:
1. Search phase: quick iterations (10 rounds each) to find the approximate
   Elo where win rate ≈ 50%.
2. Precision phase: one large match at the converged SF Elo to measure the
   Elo difference with a statistical confidence interval.

The confidence interval uses the formula: ±1.96 * 400 / (√N * win_rate * (1-win_rate))
simplified to approximately ±800/√N for win rates near 50%.
"""

from __future__ import annotations

import argparse
import csv
import math
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

from bench.config import Config, resolve_engine
from bench.git_meta import get_git_info
from bench.machine_info import get_machine_info
from bench.parsers import parse_fastchess_output


def _win_rate_to_elo(win_rate: float) -> float:
    """Convert win rate to Elo difference (positive = first player stronger)."""
    win_rate = max(0.01, min(0.99, win_rate))
    return -400.0 * math.log10(1.0 / win_rate - 1.0)


def _elo_confidence(win_rate: float, n_games: int) -> float:
    """95% confidence interval half-width for an Elo estimate."""
    if n_games <= 1:
        return 999.0
    win_rate = max(0.01, min(0.99, win_rate))
    # Standard error of win rate, then propagate through the Elo formula
    se = math.sqrt(win_rate * (1.0 - win_rate) / n_games)
    # Derivative of Elo w.r.t. win rate at the observed point
    d_elo = 400.0 / (math.log(10) * win_rate * (1.0 - win_rate))
    return 1.96 * se * d_elo


def _run_match(
    fast_chess_path: str,
    engine_cmd: str,
    engine_args: list[str],
    sf_cmd: str,
    sf_elo: int,
    rounds: int,
    tc: str,
    pgn_path: str,
    book_path: str | None = None,
    book_depth: int = 4,
) -> tuple[int, int, int]:
    """Run a fixed-round match. Returns (wins, losses, draws) for blunder."""
    cmd: list[str] = [
        fast_chess_path,
        "-engine", f"cmd={engine_cmd}", "name=blunder", "proto=uci",
    ]
    if engine_args:
        cmd.append(f"args={' '.join(engine_args)}")
    cmd.extend([
        "-engine", f"cmd={sf_cmd}", f"name=stockfish-{sf_elo}", "proto=uci",
        "option.Threads=1", "option.Hash=128",
        "option.UCI_LimitStrength=true", f"option.UCI_Elo={sf_elo}",
        "-each", f"tc={tc}",
        "-rounds", str(rounds),
        "-games", "2",
        "-concurrency", "1",
        "-pgnout", f"file={pgn_path}",
    ])

    if book_path:
        book_ext = Path(book_path).suffix.lower()
        if book_ext in (".epd", ".pgn"):
            book_format = "epd" if book_ext == ".epd" else "pgn"
            cmd.extend([
                "-openings",
                f"file={book_path}",
                f"format={book_format}",
                "order=random",
                f"plies={book_depth}",
            ])

    try:
        proc = subprocess.Popen(
            cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
        )
    except FileNotFoundError:
        print(f"Error: fast-chess not found at {fast_chess_path}", file=sys.stderr)
        return (0, 0, 0)

    output_lines: list[str] = []
    assert proc.stdout is not None
    for line in proc.stdout:
        sys.stdout.write(line)
        sys.stdout.flush()
        output_lines.append(line)

    proc.wait()
    full_output = "".join(output_lines)

    try:
        result = parse_fastchess_output(full_output)
        return (result.wins, result.losses, result.draws)
    except ValueError:
        print("Warning: could not parse match output", file=sys.stderr)
        return (0, 0, 0)


def cmd_elo(config: Config, args: argparse.Namespace) -> int:
    """Execute bench elo subcommand. Returns exit code."""
    lo: int = getattr(args, "lo", 1000)
    hi: int = getattr(args, "hi", 3000)
    search_rounds: int = getattr(args, "search_rounds", 10)
    precision_rounds: int = getattr(args, "precision_rounds", 125)
    tc: str = getattr(args, "tc", None) or "5+0.05"
    max_iterations: int = getattr(args, "max_iterations", 8)
    dry_run: bool = getattr(args, "dry_run", False)

    engine_entry = resolve_engine(config, "blunder-hce")
    fast_chess_path = str(config.paths.fast_chess)
    sf_cmd = "stockfish"
    output_dir = config.project_root / "scripts" / "output"
    book_path = str(config.paths.opening_book)
    book_depth = config.defaults.book_depth

    if not Path(fast_chess_path).exists():
        print(f"Error: fast-chess not found at {fast_chess_path}", file=sys.stderr)
        return 1

    sf_elo = (lo + hi) // 2

    print(f"Elo search: initial guess {sf_elo}")
    print(f"  Search phase:    {search_rounds} rounds/step, max {max_iterations} iterations")
    print(f"  Precision phase: {precision_rounds} rounds ({precision_rounds * 2} games)")
    print(f"  Time control:    {tc}")
    print()

    if dry_run:
        print("DRY RUN — would iteratively estimate Elo")
        return 0

    # === Phase 1: Search ===
    print("─" * 60)
    print("  PHASE 1: Search (finding approximate Elo)")
    print("─" * 60)
    print()

    total_games = 0
    search_converge_threshold = 80  # coarse — just find the neighborhood

    for iteration in range(1, max_iterations + 1):
        sf_elo = max(lo, min(hi, sf_elo))

        print(f"  [{iteration}/{max_iterations}] Testing vs SF Elo {sf_elo}")

        pgn_path = str(output_dir / f"elo_search_s{iteration}_sf{sf_elo}.pgn")

        wins, losses, draws = _run_match(
            fast_chess_path=fast_chess_path,
            engine_cmd=engine_entry.cmd,
            engine_args=engine_entry.args,
            sf_cmd=sf_cmd,
            sf_elo=sf_elo,
            rounds=search_rounds,
            tc=tc,
            pgn_path=pgn_path,
            book_path=book_path,
            book_depth=book_depth,
        )

        games = wins + losses + draws
        total_games += games
        if games == 0:
            print("  Error: no games completed", file=sys.stderr)
            return 1

        win_rate = (wins + draws * 0.5) / games
        elo_diff = _win_rate_to_elo(win_rate)

        print(f"    +{wins} -{losses} ={draws} ({win_rate:.1%}) → Elo diff {elo_diff:+.0f}")

        if abs(elo_diff) < search_converge_threshold:
            print(f"    Search converged (|{elo_diff:.0f}| < {search_converge_threshold})")
            break

        sf_elo = sf_elo + int(elo_diff)
        print()

    # === Phase 2: Precision ===
    print()
    print("─" * 60)
    print(f"  PHASE 2: Precision match vs SF Elo {sf_elo}")
    print(f"  Playing {precision_rounds * 2} games for confidence interval")
    print("─" * 60)
    print()

    pgn_path = str(output_dir / f"elo_search_precision_sf{sf_elo}.pgn")

    wins, losses, draws = _run_match(
        fast_chess_path=fast_chess_path,
        engine_cmd=engine_entry.cmd,
        engine_args=engine_entry.args,
        sf_cmd=sf_cmd,
        sf_elo=sf_elo,
        rounds=precision_rounds,
        tc=tc,
        pgn_path=pgn_path,
        book_path=book_path,
        book_depth=book_depth,
    )

    games = wins + losses + draws
    total_games += games
    if games == 0:
        print("  Error: no games completed in precision phase", file=sys.stderr)
        return 1

    win_rate = (wins + draws * 0.5) / games
    elo_diff = _win_rate_to_elo(win_rate)
    ci = _elo_confidence(win_rate, games)
    estimated_elo = sf_elo + int(elo_diff)

    print(f"  Result: +{wins} -{losses} ={draws} ({win_rate:.1%}, {games} games)")
    print(f"  Elo vs SF-{sf_elo}: {elo_diff:+.0f}")
    print()
    print("=" * 60)
    print(f"  Estimated Elo: {estimated_elo} ± {ci:.0f}")
    print(f"  95% CI: [{estimated_elo - int(ci)}, {estimated_elo + int(ci)}]")
    print(f"  Total games: {total_games} (search: {total_games - games}, precision: {games})")
    print("=" * 60)
    print()

    # Record result
    git_info = get_git_info(config.project_root)
    machine = get_machine_info()
    timestamp = datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M:%S")

    elo_csv = output_dir / "elo_results.csv"
    write_header = not elo_csv.exists()
    with open(elo_csv, "a", newline="") as f:
        writer = csv.writer(f)
        if write_header:
            writer.writerow([
                "timestamp", "commit", "branch", "evaluator",
                "estimated_elo", "ci_95", "total_games",
                "precision_games", "tc", "iterations",
                "cpu_model",
            ])
        writer.writerow([
            timestamp, git_info.commit, git_info.branch, "hce",
            estimated_elo, f"{ci:.0f}", total_games,
            games, tc, iteration,
            machine.cpu_model,
        ])

    print(f"  Result saved to {elo_csv}")
    return 0
