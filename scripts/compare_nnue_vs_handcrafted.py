#!/usr/bin/env python3
"""
Compare NNUE evaluator vs HandCrafted evaluator using cutechess-cli.

Plays a match between two instances of the same engine:
- One using NNUE evaluation (--nnue weights/nnue_v001.bin)
- One using HandCrafted evaluation (default)

Usage:
    python scripts/compare_nnue_vs_handcrafted.py [options]
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
    return str(Path("build", "dev", f"blunder{ext}"))


def parse_args() -> argparse.Namespace:
    project_dir = Path(__file__).resolve().parent.parent
    engine = str(project_dir / default_engine())
    book = str(project_dir / "books" / "i-gm1950.bin")
    nnue_weights = str(project_dir / "weights" / "nnue_v001.bin")

    p = argparse.ArgumentParser(
        description="Compare NNUE vs HandCrafted evaluator via cutechess-cli"
    )
    p.add_argument("--engine", default=engine, help="Engine binary path")
    p.add_argument("--nnue", default=nnue_weights, help="NNUE weights file")
    p.add_argument("--tc", default="10+0.1", help="Time control (default: 10+0.1)")
    p.add_argument("--book", default=book, help="Opening book path")
    p.add_argument("--book-depth", type=int, default=4, help="Book depth (default: 4)")
    p.add_argument("--games", type=int, default=100, help="Number of games (default: 100)")
    p.add_argument("--concurrency", type=int, default=4, help="Parallel games")
    p.add_argument("--alphazero", action="store_true",
                   help="Use AlphaZero mode (MCTS + dual-head network)")
    p.add_argument("--mcts-simulations", type=int, default=800,
                   help="MCTS simulations per move in AlphaZero mode (default: 800)")
    p.add_argument("--protocol", default="xboard", choices=["xboard", "uci"],
                   help="Engine protocol (default: xboard)")
    p.add_argument("--ponder", action="store_true", help="Enable pondering")
    p.add_argument("--debug", action="store_true", help="Show all engine I/O")
    p.add_argument("--cutechess", default=None, help="Path to cutechess-cli")
    return p.parse_args()


def validate(args: argparse.Namespace) -> None:
    if not Path(args.engine).is_file():
        sys.exit(f"ERROR: Engine not found: {args.engine}")
    if not Path(args.nnue).is_file():
        sys.exit(f"ERROR: NNUE weights not found: {args.nnue}")
    if not Path(args.book).is_file():
        sys.exit(f"ERROR: Opening book not found: {args.book}")


def run_match(args: argparse.Namespace) -> int:
    cutechess = find_cutechess(args.cutechess)

    # Create timestamped output directory
    scripts_dir = Path(__file__).resolve().parent
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    output_dir = scripts_dir / "output" / f"nnue_vs_handcrafted_{timestamp}"
    output_dir.mkdir(parents=True, exist_ok=True)

    pgn_out = str(output_dir / "games.pgn")
    log_file = output_dir / "cutechess.log"

    # Build engine options
    proto = args.protocol
    proto_flag = f"--{proto}"
    if args.alphazero:
        engine_name = "AlphaZero"
        engine_opts_nnue = [
            "-engine", f"name={engine_name}", f"cmd={args.engine}",
            f"proto={proto}", f"arg={proto_flag}",
            "arg=--alphazero",
            f"arg=--nnue", f"arg={args.nnue}",
            f"arg=--mcts-simulations", f"arg={args.mcts_simulations}",
            f"stderr={output_dir / 'alphazero.err.log'}",
        ]
    else:
        engine_name = "NNUE"
        engine_opts_nnue = [
            "-engine", f"name={engine_name}", f"cmd={args.engine}",
            f"proto={proto}", f"arg={proto_flag}",
            f"arg=--nnue", f"arg={args.nnue}",
            f"stderr={output_dir / 'nnue.err.log'}",
        ]

    # HandCrafted engine (no NNUE argument)
    engine_opts_hc = [
        "-engine", "name=HandCrafted", f"cmd={args.engine}",
        f"proto={proto}", f"arg={proto_flag}",
        f"stderr={output_dir / 'handcrafted.err.log'}",
    ]

    if args.ponder:
        engine_opts_nnue.append("ponder")
        engine_opts_hc.append("ponder")

    cmd = [
        cutechess,
        *engine_opts_nnue,
        *engine_opts_hc,
        "-each",
            f"tc={args.tc}",
            f"book={args.book}",
            f"bookdepth={args.book_depth}",
        "-games", "2",  # Play each opening from both sides
        "-rounds", str(args.games // 2),  # Total games = rounds * 2
        "-repeat", "2",
        "-maxmoves", "200",
        "-concurrency", str(args.concurrency),
        "-ratinginterval", "10",
        "-pgnout", pgn_out,
        "-srand", "0",
    ]
    
    if args.debug:
        cmd.insert(1, "-debug")

    print("=" * 80)
    print(f"{engine_name} vs HandCrafted Evaluator Match")
    print("=" * 80)
    print(f"Cutechess:   {cutechess}")
    print(f"Engine:      {args.engine}")
    print(f"Mode:        {'AlphaZero (MCTS + dual-head)' if args.alphazero else 'NNUE'}")
    print(f"Weights:     {args.nnue}")
    if args.alphazero:
        print(f"MCTS sims:   {args.mcts_simulations}")
    print(f"TC:          {args.tc}")
    print(f"Book:        {args.book}")
    print(f"Games:       {args.games}")
    print(f"Concurrency: {args.concurrency}")
    print(f"Protocol:    {args.protocol}")
    print(f"Ponder:      {'ON' if args.ponder else 'OFF'}")
    print(f"Output:      {output_dir}")
    print("=" * 80)
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

    # Parse result from log
    log_text = log_file.read_text(encoding="utf-8")
    return parse_result(log_text)


def parse_result(log_text: str) -> int:
    """Parse the match result from cutechess-cli output."""
    
    print()
    print("=" * 80)
    print("MATCH RESULT")
    print("=" * 80)

    # Find score line (format: "Score of NNUE vs HandCrafted: 45 - 50 - 5  [0.475]")
    score_match = re.search(
        r"Score of (\w+) vs (\w+):\s*(\d+)\s*-\s*(\d+)\s*-\s*(\d+)\s*\[([0-9.]+)\]",
        log_text
    )
    
    if score_match:
        engine1, engine2, wins, losses, draws, score = score_match.groups()
        wins, losses, draws = int(wins), int(losses), int(draws)
        score = float(score)
        total = wins + losses + draws
        
        print(f"{engine1} vs {engine2}:")
        print(f"  Wins:   {wins}")
        print(f"  Losses: {losses}")
        print(f"  Draws:  {draws}")
        print(f"  Total:  {total} games")
        print(f"  Score:  {score:.3f} ({score*100:.1f}%)")
        print()
        
        # Find Elo difference if available
        elo_match = re.search(
            r"Elo difference:\s*([-\d.]+)\s*\+/-\s*([\d.]+)", log_text
        )
        if elo_match:
            elo_diff = float(elo_match.group(1))
            elo_error = float(elo_match.group(2))
            print(f"Elo difference: {elo_diff:+.1f} +/- {elo_error:.1f}")
            
            if abs(elo_diff) < elo_error:
                print("Result: No significant difference")
            elif elo_diff > 0:
                print(f"Result: {engine1} is stronger")
            else:
                print(f"Result: {engine2} is stronger")
        
        print()
        
        return 0
    else:
        print("Could not parse match result from log")
        return 1


def main() -> None:
    args = parse_args()
    validate(args)
    exit_code = run_match(args)
    sys.exit(exit_code)


if __name__ == "__main__":
    main()
