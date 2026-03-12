"""Elo search — binary search for the engine's absolute Elo rating.

Plays matches against Stockfish at varying UCI_Elo levels to find the
Elo where the engine scores ~50%. Uses binary search (dichotomy) to
converge efficiently.

Algorithm:
1. Start with a range [lo, hi] (default 1000-3000)
2. Set stockfish to midpoint Elo
3. Play N quick games
4. If win rate > 55% → search higher half
   If win rate < 45% → search lower half
   Otherwise → converged
5. Stop when range < threshold (default 50 Elo)
"""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

from bench.config import Config, resolve_engine
from bench.parsers import parse_fastchess_output


def _run_match(
    fast_chess_path: str,
    engine_cmd: str,
    engine_args: list[str],
    sf_cmd: str,
    sf_elo: int,
    rounds: int,
    tc: str,
    pgn_path: str,
) -> tuple[int, int, int]:
    """Run a match and return (wins, losses, draws) for the engine."""
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
        "-concurrency", "1",
        "-pgnout", f"file={pgn_path}",
    ])

    try:
        proc = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
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
    rounds: int = getattr(args, "rounds", 20)
    tc: str = getattr(args, "tc", None) or "5+0.05"
    threshold: int = getattr(args, "threshold", 50)
    dry_run: bool = getattr(args, "dry_run", False)

    engine_entry = resolve_engine(config, "blunder-hce")
    fast_chess_path = str(config.paths.fast_chess)
    sf_cmd = "stockfish"
    output_dir = config.project_root / "scripts" / "output"

    if not Path(fast_chess_path).exists():
        print(f"Error: fast-chess not found at {fast_chess_path}", file=sys.stderr)
        return 1

    print(f"Elo search: range [{lo}, {hi}], {rounds} games/step, tc={tc}")
    print(f"Threshold: {threshold} Elo")
    print()

    if dry_run:
        print("DRY RUN — would binary search through Elo range")
        return 0

    iteration = 0
    while hi - lo > threshold:
        mid = (lo + hi) // 2
        iteration += 1

        print("=" * 60)
        print(f"Iteration {iteration}: testing SF Elo {mid}  (range [{lo}, {hi}])")
        print("=" * 60)

        pgn_path = str(output_dir / f"elo_search_iter{iteration}_sf{mid}.pgn")

        wins, losses, draws = _run_match(
            fast_chess_path=fast_chess_path,
            engine_cmd=engine_entry.cmd,
            engine_args=engine_entry.args,
            sf_cmd=sf_cmd,
            sf_elo=mid,
            rounds=rounds,
            tc=tc,
            pgn_path=pgn_path,
        )

        total = wins + losses + draws
        if total == 0:
            print("Error: no games completed", file=sys.stderr)
            return 1

        win_rate = (wins + draws * 0.5) / total
        print()
        print(f"  Result vs SF-{mid}: +{wins} -{losses} ={draws}  "
              f"({win_rate:.1%})")

        if win_rate > 0.55:
            print(f"  Win rate > 55% → blunder is stronger, searching [{mid}, {hi}]")
            lo = mid
        elif win_rate < 0.45:
            print(f"  Win rate < 45% → blunder is weaker, searching [{lo}, {mid}]")
            hi = mid
        else:
            print(f"  Win rate ~50% → converged!")
            lo = mid - threshold // 2
            hi = mid + threshold // 2
            break

        print()

    estimated_elo = (lo + hi) // 2
    print()
    print("=" * 60)
    print(f"  Estimated Elo: {estimated_elo}  (range [{lo}, {hi}])")
    print("=" * 60)
    print()

    return 0
