#!/usr/bin/env python3
"""
Blunder Benchmark Runner
========================

Runs WAC and STS test suites, tracks results over time.

How it works:
  1. Runs the Catch2 test binary with the selected test suite(s) and mode
  2. Parses score, ELO, NPS, and STS per-category breakdowns from output
  3. Auto-grabs git commit hash and branch name
  4. Appends results to scripts/output/benchmarks.csv
  5. Appends STS category breakdowns to scripts/output/benchmarks_categories.csv
  6. Warns if score regresses vs. the previous run for the same suite/mode/evaluator

Quick start (one command does everything):
    python3 scripts/run-benchmarks.py --mode all

This runs WAC + STS in all three modes (nodes, depth8, time1s) and logs everything.

Individual modes:
    --mode nodes    Fixed 1M nodes per position (default, original behavior)
    --mode depth8   Fixed depth 8 — isolates eval quality independent of speed
    --mode time1s   Fixed 1 second per position — real-world playing strength

Evaluator tracking:
    --evaluator hce        Hand-crafted eval (default)
    --evaluator nnue       NNUE neural network eval
    --evaluator alphazero  AlphaZero/MCTS eval

Output files:
    scripts/output/benchmarks.csv            — main results (one row per suite/mode run)
    scripts/output/benchmarks_categories.csv — STS per-category breakdown

For cutechess-cli SPRT gauntlet testing, see scripts/run-cutechess.py.
"""

import argparse
import csv
import os
import re
import subprocess
import sys
from datetime import datetime
from pathlib import Path

# Catch2 test name mapping
# mode -> suffix appended to test name
MODE_SUFFIXES = {
    "nodes": "",
    "depth8": "_depth8",
    "time1s": "_time1s",
}

# suite key -> Catch2 test name base (nodes mode uses original names)
SUITE_TEST_NAMES = {
    ("WAC", "nodes"): "test_positions_WAC",
    ("WAC", "depth8"): "test_positions_WAC_depth8",
    ("WAC", "time1s"): "test_positions_WAC_time1s",
    ("STS", "nodes"): "test_positions_STS-Rating",
    ("STS", "depth8"): "test_positions_STS_depth8",
    ("STS", "time1s"): "test_positions_STS_time1s",
}

MAIN_FIELDS = [
    "timestamp", "commit", "branch", "evaluator", "mode", "suite",
    "score_pct", "score", "max_score", "elo", "nps", "nodes",
    "time_secs", "passed",
]

CAT_FIELDS = [
    "timestamp", "commit", "branch", "evaluator", "mode",
    "category", "score_pct", "score", "max_score", "positions",
]
