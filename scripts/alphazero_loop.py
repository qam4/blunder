#!/usr/bin/env python3
"""
Iterative AlphaZero Training Loop for Blunder Chess Engine

Orchestrates the full AlphaZero self-improvement cycle:
  1. Generate MCTS self-play data using current weights
  2. Train dual-head network on the generated data
  3. (Optional) Evaluate new weights vs HandCrafted via cutechess
  4. Save new weights and repeat

Usage:
    python scripts/alphazero_loop.py --iterations 5 --games 100

First iteration uses MCTS with uniform priors (no weights). Subsequent
iterations use --alphazero mode so the dual-head network's policy head
guides MCTS exploration.
"""

import argparse
import os
import shutil
import subprocess
import sys
from datetime import datetime
from pathlib import Path


def default_engine() -> str:
    """Return default engine path based on platform."""
    ext = ".exe" if os.name == "nt" else ""
    return str(Path("build", "dev", f"blunder{ext}"))


def run_cmd(cmd: list[str], description: str) -> int:
    """Run a command, printing its output in real time."""
    print(f"\n{'='*70}")
    print(f"  {description}")
    print(f"{'='*70}")
    print(f"  Command: {' '.join(cmd)}\n")

    result = subprocess.run(cmd, text=True)
    if result.returncode != 0:
        print(f"\n  ERROR: {description} failed (exit code {result.returncode})")
    return result.returncode


def generate_selfplay_data(
    engine: str,
    output_path: str,
    games: int,
    simulations: int,
    temperature: float,
    temp_drop: int,
    weights_path: str | None = None,
) -> int:
    """Generate MCTS self-play training data.

    If weights_path is provided and the file exists, uses --alphazero mode
    so the dual-head network guides MCTS. Otherwise uses plain --mcts with
    uniform priors.
    """
    cmd = [engine, "--selfplay", "--mcts"]

    # Use AlphaZero mode when we have trained weights
    if weights_path and Path(weights_path).is_file():
        cmd += ["--alphazero", "--nnue", weights_path]

    cmd += [
        "--selfplay-games", str(games),
        "--mcts-simulations", str(simulations),
        "--selfplay-temperature", str(temperature),
        "--selfplay-temp-drop", str(temp_drop),
        "--selfplay-output", output_path,
    ]

    iteration_label = "with network" if weights_path and Path(weights_path).is_file() else "uniform priors"
    return run_cmd(cmd, f"Self-play data generation ({iteration_label})")


def train_network(
    data_path: str,
    output_path: str,
    epochs: int,
    batch_size: int = 256,
    learning_rate: float = 0.001,
) -> int:
    """Train dual-head network on MCTS self-play data."""
    scripts_dir = Path(__file__).resolve().parent
    train_script = str(scripts_dir / "train_alphazero.py")

    cmd = [
        sys.executable, train_script,
        "--input", data_path,
        "--output", output_path,
        "--epochs", str(epochs),
        "--batch-size", str(batch_size),
        "--learning-rate", str(learning_rate),
    ]

    return run_cmd(cmd, "Training dual-head network")


def evaluate_weights(
    engine: str,
    weights_path: str,
    eval_games: int,
    tc: str = "5+0.1",
) -> int:
    """Evaluate new weights vs HandCrafted using cutechess.

    Uses scripts/compare_nnue_vs_handcrafted.py which handles cutechess
    setup and result parsing.
    """
    scripts_dir = Path(__file__).resolve().parent
    compare_script = str(scripts_dir / "compare_nnue_vs_handcrafted.py")

    cmd = [
        sys.executable, compare_script,
        "--engine", engine,
        "--nnue", weights_path,
        "--games", str(eval_games),
        "--tc", tc,
    ]

    return run_cmd(cmd, f"Evaluating weights vs HandCrafted ({eval_games} games)")


def main():
    parser = argparse.ArgumentParser(
        description="Iterative AlphaZero training loop for Blunder"
    )
    parser.add_argument(
        "--engine", type=str, default=default_engine(),
        help=f"Path to blunder executable (default: {default_engine()})"
    )
    parser.add_argument(
        "--iterations", type=int, default=5,
        help="Number of training iterations (default: 5)"
    )
    parser.add_argument(
        "--games", type=int, default=100,
        help="Self-play games per iteration (default: 100)"
    )
    parser.add_argument(
        "--simulations", type=int, default=400,
        help="MCTS simulations per move (default: 400)"
    )
    parser.add_argument(
        "--epochs", type=int, default=10,
        help="Training epochs per iteration (default: 10)"
    )
    parser.add_argument(
        "--batch-size", type=int, default=256,
        help="Training batch size (default: 256)"
    )
    parser.add_argument(
        "--learning-rate", type=float, default=0.001,
        help="Training learning rate (default: 0.001)"
    )
    parser.add_argument(
        "--eval-games", type=int, default=20,
        help="Cutechess evaluation games per iteration (0 to skip, default: 20)"
    )
    parser.add_argument(
        "--eval-tc", type=str, default="5+0.1",
        help="Time control for evaluation games (default: 5+0.1)"
    )
    parser.add_argument(
        "--temperature", type=float, default=1.0,
        help="MCTS temperature for self-play (default: 1.0)"
    )
    parser.add_argument(
        "--temp-drop", type=int, default=30,
        help="Ply at which temperature drops (default: 30)"
    )
    parser.add_argument(
        "--output-dir", type=str, default="weights",
        help="Directory for weights and data (default: weights/)"
    )

    args = parser.parse_args()

    # Validate engine exists
    if not Path(args.engine).is_file():
        sys.exit(f"ERROR: Engine not found: {args.engine}")

    # Create output directory
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    # Create a timestamped run directory for training data
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    run_dir = output_dir / f"alphazero_run_{timestamp}"
    run_dir.mkdir(parents=True, exist_ok=True)

    print("=" * 70)
    print("  AlphaZero Iterative Training Loop")
    print("=" * 70)
    print(f"  Engine:       {args.engine}")
    print(f"  Iterations:   {args.iterations}")
    print(f"  Games/iter:   {args.games}")
    print(f"  Simulations:  {args.simulations}")
    print(f"  Epochs/iter:  {args.epochs}")
    print(f"  Eval games:   {args.eval_games} ({'skip' if args.eval_games == 0 else args.eval_tc})")
    print(f"  Output dir:   {run_dir}")
    print("=" * 70)

    current_weights: str | None = None

    for iteration in range(1, args.iterations + 1):
        print(f"\n{'#'*70}")
        print(f"  ITERATION {iteration} / {args.iterations}")
        print(f"{'#'*70}")

        # Paths for this iteration
        data_path = str(run_dir / f"selfplay_iter{iteration:03d}.bin")
        weights_path = str(run_dir / f"alphazero_iter{iteration:03d}.bin")

        # Step 1: Generate self-play data
        rc = generate_selfplay_data(
            engine=args.engine,
            output_path=data_path,
            games=args.games,
            simulations=args.simulations,
            temperature=args.temperature,
            temp_drop=args.temp_drop,
            weights_path=current_weights,
        )
        if rc != 0:
            print(f"\nSelf-play failed at iteration {iteration}. Stopping.")
            sys.exit(1)

        if not Path(data_path).is_file():
            print(f"\nExpected data file not found: {data_path}. Stopping.")
            sys.exit(1)

        # Step 2: Train dual-head network
        rc = train_network(
            data_path=data_path,
            output_path=weights_path,
            epochs=args.epochs,
            batch_size=args.batch_size,
            learning_rate=args.learning_rate,
        )
        if rc != 0:
            print(f"\nTraining failed at iteration {iteration}. Stopping.")
            sys.exit(1)

        if not Path(weights_path).is_file():
            print(f"\nExpected weights file not found: {weights_path}. Stopping.")
            sys.exit(1)

        # Update current weights for next iteration
        previous_weights = current_weights
        current_weights = weights_path

        # Also copy to a stable "latest" path
        latest_path = str(output_dir / "alphazero_latest.bin")
        shutil.copy2(weights_path, latest_path)
        print(f"\n  Weights saved: {weights_path}")
        print(f"  Latest copy:   {latest_path}")

        # Step 3: Optional evaluation
        if args.eval_games > 0:
            evaluate_weights(
                engine=args.engine,
                weights_path=weights_path,
                eval_games=args.eval_games,
                tc=args.eval_tc,
            )
            # Evaluation failures are non-fatal — continue training

    print(f"\n{'='*70}")
    print(f"  AlphaZero training complete!")
    print(f"  Final weights: {current_weights}")
    print(f"  Latest copy:   {output_dir / 'alphazero_latest.bin'}")
    print(f"{'='*70}")


if __name__ == "__main__":
    main()
