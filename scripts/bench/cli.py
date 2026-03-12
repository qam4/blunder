"""Entry point and argument parsing for the bench CLI."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from bench.compare import cmd_compare
from bench.config import load_config
from bench.elo_search import cmd_elo
from bench.gauntlet import cmd_gauntlet
from bench.run import cmd_run
from bench.run_all import cmd_run_all


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="bench",
        description="Unified benchmarking CLI for the Blunder chess engine.",
    )

    # Global flags
    parser.add_argument(
        "--config",
        metavar="PATH",
        default=None,
        help="Override config file location (default: scripts/bench-config.json)",
    )
    parser.add_argument(
        "--preset",
        metavar="NAME",
        default=None,
        help="Override build preset (affects binary path resolution)",
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        default=False,
        help="Enable debug-level output",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        default=False,
        help="Print commands that would be run without executing them",
    )
    parser.add_argument(
        "--show-cmd",
        action="store_true",
        default=False,
        help="Print each subprocess command before executing it",
    )

    subparsers = parser.add_subparsers(dest="command", help="Available subcommands")

    # --- run ---
    run_parser = subparsers.add_parser("run", help="Run WAC/STS benchmarks")
    run_parser.add_argument(
        "--suite",
        choices=["wac", "sts", "all"],
        default="all",
        help="Test suite to run (default: all)",
    )
    run_parser.add_argument(
        "--mode",
        choices=["nodes", "depth", "time", "all"],
        default="all",
        help="Benchmark mode (default: all)",
    )
    run_parser.add_argument(
        "--evaluator",
        choices=["hce", "nnue", "alphazero"],
        default=None,
        help="Evaluator type to tag results with (default from config)",
    )
    run_parser.set_defaults(func=cmd_run)

    # --- gauntlet ---
    gauntlet_parser = subparsers.add_parser(
        "gauntlet", help="Run engine-vs-engine matches via fast-chess"
    )
    gauntlet_parser.add_argument(
        "--type",
        choices=["sprt", "roundrobin"],
        default="sprt",
        help="Tournament type (default: sprt)",
    )
    gauntlet_parser.add_argument(
        "--baseline",
        metavar="NAME",
        default=None,
        help="Baseline engine name from registry (default from config)",
    )
    gauntlet_parser.add_argument(
        "--candidate",
        metavar="NAME",
        default=None,
        help="Candidate engine name from registry (default from config)",
    )
    gauntlet_parser.add_argument(
        "--engines",
        nargs="+",
        metavar="NAME",
        default=None,
        help="Engine names for roundrobin tournament",
    )
    gauntlet_parser.add_argument(
        "--tc",
        metavar="TC",
        default=None,
        help="Time control string, e.g. 10+0.1 (default from config)",
    )
    gauntlet_parser.add_argument(
        "--rounds",
        type=int,
        default=None,
        help="Number of rounds (default from config)",
    )
    gauntlet_parser.add_argument(
        "--concurrency",
        type=int,
        default=None,
        help="Number of concurrent games (default from config)",
    )
    gauntlet_parser.add_argument(
        "--book",
        metavar="PATH",
        default=None,
        help="Path to opening book (default from config)",
    )
    gauntlet_parser.add_argument(
        "--book-depth",
        type=int,
        default=None,
        help="Opening book depth in plies (default from config)",
    )
    gauntlet_parser.add_argument(
        "--elo0",
        type=int,
        default=None,
        help="SPRT lower Elo bound (default from config)",
    )
    gauntlet_parser.add_argument(
        "--elo1",
        type=int,
        default=None,
        help="SPRT upper Elo bound (default from config)",
    )
    gauntlet_parser.add_argument(
        "--fast-chess-args",
        metavar="ARGS",
        default=None,
        help="Extra arguments to pass through to fast-chess",
    )
    gauntlet_parser.set_defaults(func=cmd_gauntlet)

    # --- compare ---
    compare_parser = subparsers.add_parser(
        "compare", help="Compare benchmark results across evaluators or commits"
    )
    compare_parser.add_argument(
        "--by",
        choices=["evaluator", "commit", "version"],
        default="evaluator",
        help="Comparison axis (default: evaluator)",
    )
    compare_parser.add_argument(
        "--commits",
        nargs="+",
        metavar="HASH",
        default=None,
        help="Git commit hashes to compare (when --by commit)",
    )
    compare_parser.set_defaults(func=cmd_compare)

    # --- run-all ---
    run_all_parser = subparsers.add_parser(
        "run-all", help="Run full benchmarking pipeline"
    )
    run_all_parser.add_argument(
        "--evaluator",
        choices=["hce", "nnue", "alphazero"],
        default=None,
        help="Override evaluator type for the benchmark run step (default from config)",
    )
    run_all_parser.set_defaults(func=cmd_run_all)

    # --- elo ---
    elo_parser = subparsers.add_parser(
        "elo", help="Binary search for absolute Elo rating vs Stockfish"
    )
    elo_parser.add_argument(
        "--lo", type=int, default=1000,
        help="Lower bound of Elo search range (default: 1000)",
    )
    elo_parser.add_argument(
        "--hi", type=int, default=3000,
        help="Upper bound of Elo search range (default: 3000)",
    )
    elo_parser.add_argument(
        "--rounds", type=int, default=20,
        help="Games per binary search step (default: 20)",
    )
    elo_parser.add_argument(
        "--tc", metavar="TC", default=None,
        help="Time control string (default: 5+0.05)",
    )
    elo_parser.add_argument(
        "--threshold", type=int, default=50,
        help="Stop when Elo range narrows to this (default: 50)",
    )
    elo_parser.set_defaults(func=cmd_elo)

    return parser


def _find_project_root() -> Path:
    """Detect the project root directory.

    Walks up from this file's location looking for a directory that
    contains a ``scripts/`` subdirectory.  Falls back to the current
    working directory if nothing is found.
    """
    start = Path(__file__).resolve().parent  # scripts/bench/
    candidate = start.parent.parent  # should be project root
    if (candidate / "scripts").is_dir():
        return candidate
    # Fallback: walk up from cwd
    cwd = Path.cwd()
    for parent in [cwd, *cwd.parents]:
        if (parent / "scripts").is_dir():
            return parent
    return cwd


def main(argv: list[str] | None = None) -> int:
    """Entry point for the bench CLI. Returns exit code."""
    parser = _build_parser()
    args = parser.parse_args(argv)

    if args.command is None:
        parser.print_help()
        return 0

    # Resolve project root and config path
    project_root = _find_project_root()

    if args.config is not None:
        config_path = Path(args.config)
    else:
        config_path = project_root / "scripts" / "bench-config.json"

    config = load_config(config_path, preset_override=args.preset)

    func = args.func
    return func(config, args)


if __name__ == "__main__":
    sys.exit(main())
