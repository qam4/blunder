"""Run-all pipeline — orchestrates full benchmarking pipeline.

Runs ``bench run --suite all --mode all`` followed by
``bench gauntlet --type sprt``, collects failures without aborting,
and prints a consolidated summary.
"""

from __future__ import annotations

import argparse
import sys

from bench.config import Config


def cmd_run_all(config: Config, args: argparse.Namespace) -> int:
    """Execute bench run-all subcommand. Returns exit code."""
    from bench.gauntlet import cmd_gauntlet
    from bench.run import cmd_run

    evaluator: str | None = getattr(args, "evaluator", None)
    verbose: bool = getattr(args, "verbose", False)
    dry_run: bool = getattr(args, "dry_run", False)
    show_cmd: bool = getattr(args, "show_cmd", False)

    failures: list[str] = []
    run_result: int | None = None
    gauntlet_result: int | None = None

    # ---- Step 1: bench run --suite all --mode all ----
    print("=" * 60)
    print("Step 1: Running benchmarks (suite=all, mode=all)")
    print("=" * 60)

    run_args = argparse.Namespace(
        suite="all",
        mode="all",
        evaluator=evaluator,
        verbose=verbose,
        dry_run=dry_run,
        show_cmd=show_cmd,
    )

    try:
        run_result = cmd_run(config, run_args)
    except Exception as exc:  # noqa: BLE001
        run_result = 1
        failures.append(f"bench run: {exc}")
        print(f"Error in bench run: {exc}", file=sys.stderr)

    if run_result != 0 and "bench run" not in " ".join(failures):
        failures.append(f"bench run: exited with code {run_result}")

    # ---- Step 2: bench gauntlet --type sprt ----
    print("=" * 60)
    print("Step 2: Running gauntlet (type=sprt)")
    print("=" * 60)

    gauntlet_args = argparse.Namespace(
        type="sprt",
        baseline=None,
        candidate=None,
        engines=None,
        tc=None,
        rounds=None,
        concurrency=None,
        book=None,
        book_depth=None,
        elo0=None,
        elo1=None,
        fast_chess_args=None,
        verbose=verbose,
        dry_run=dry_run,
        show_cmd=show_cmd,
    )

    try:
        gauntlet_result = cmd_gauntlet(config, gauntlet_args)
    except Exception as exc:  # noqa: BLE001
        gauntlet_result = 1
        failures.append(f"bench gauntlet: {exc}")
        print(f"Error in bench gauntlet: {exc}", file=sys.stderr)

    if gauntlet_result != 0 and "bench gauntlet" not in " ".join(failures):
        failures.append(f"bench gauntlet: exited with code {gauntlet_result}")

    # ---- Consolidated summary ----
    print()
    print("=" * 60)
    print("Consolidated Summary")
    print("=" * 60)

    print(f"  Benchmark run:  {'PASS' if run_result == 0 else 'FAIL'}")
    print(f"  Gauntlet SPRT:  {'PASS' if gauntlet_result == 0 else 'FAIL'}")

    if failures:
        print()
        print("Failures:")
        for f in failures:
            print(f"  - {f}")
        print()
        return 1

    print()
    print("All steps completed successfully.")
    return 0
