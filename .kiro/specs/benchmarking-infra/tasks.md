# Implementation Plan: Benchmarking Infrastructure

## Overview

Build the `bench` CLI tool as a Python package under `scripts/bench/`, backed by a JSON config file, with CSV result tracking and git metadata. Each task incrementally adds a module, wires it into the CLI, and validates with tests. The existing scripts are left in place until the new tool is fully working.

## Tasks

- [x] 1. Set up project scaffolding and package structure
  - [x] 1.1 Create `pyproject.toml` at repo root with project name `blunder-bench`, `requires-python = ">=3.10"`, zero runtime dependencies, and `[project.scripts] bench = "bench.cli:main"` entry point. Create `requirements.txt` with dev dependencies (ruff, mypy, tox, pytest, hypothesis). Create `tox.ini` with `lint`, `typecheck`, and `test` environments.
    - _Requirements: 13.1, 13.2, 13.3, 13.4, 13.5, 13.6, 13.7_
  - [x] 1.2 Create the `scripts/bench/` package with `__init__.py` and `__main__.py` (for `python -m bench` support). The `__main__.py` should import and call `cli.main()`.
    - _Requirements: 13.3, 13.5_
  - [x] 1.3 Create `scripts/bench/cli.py` with the `main()` entry point, top-level argparse parser with `--config`, `--preset`, `--verbose` global flags, and stub subcommand parsers for `run`, `gauntlet`, `compare`, `run-all`. Each subcommand should print a "not yet implemented" message and return 0.
    - _Requirements: 1.7, 8.4_

- [x] 2. Implement configuration loader (`config.py`)
  - [x] 2.1 Create `scripts/bench/config.py` with dataclasses (`PlatformPaths`, `EngineEntry`, `Defaults`, `Config`) and the `load_config()` function. Implement JSON loading, schema validation (required fields check), platform detection (`os.name`), variable interpolation (`${preset}`, `${engine_binary}`), path expansion (`~`, relative-to-project-root), and `.exe` appending on Windows.
    - _Requirements: 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 2.1, 2.2, 2.3, 8.1, 8.2, 8.3, 8.5, 8.6_
  - [x] 2.2 Implement engine registry resolution: lookup by name, error with available names on miss.
    - _Requirements: 2.4, 2.5, 2.6_
  - [x] 2.3 Wire `load_config()` into `cli.py` so it runs before subcommand dispatch, passing the `Config` object to each handler.
    - _Requirements: 1.1, 1.4, 1.7_
  - [ ]* 2.4 Write property tests for config validation (Property 1)
    - **Property 1: Config validation accepts valid configs and rejects invalid ones**
    - **Validates: Requirements 1.2, 1.3, 2.1, 12.5**
  - [ ]* 2.5 Write property tests for platform path resolution (Property 2)
    - **Property 2: Platform path resolution selects the correct platform section**
    - **Validates: Requirements 1.4, 8.1, 8.2**
  - [ ]* 2.6 Write property tests for variable interpolation (Property 3)
    - **Property 3: Variable interpolation in paths**
    - **Validates: Requirements 2.2, 8.3**
  - [ ]* 2.7 Write property tests for path expansion and platform extensions (Property 4)
    - **Property 4: Path expansion and platform extensions**
    - **Validates: Requirements 8.5, 8.6**
  - [ ]* 2.8 Write property tests for engine registry lookup (Property 5)
    - **Property 5: Engine registry lookup**
    - **Validates: Requirements 2.4, 2.5**

- [x] 3. Create the JSON config file
  - [x] 3.1 Create `scripts/bench-config.json` with `platforms` (linux/windows paths), `engines` (blunder-hce, blunder-nnue, stockfish), and `defaults` sections matching the design schema.
    - _Requirements: 1.2, 1.3, 2.1, 2.6, 5.4, 5.5, 12.5_

- [x] 4. Checkpoint — Verify scaffolding
  - Ensure `tox` runs lint + typecheck + tests cleanly. Ensure `pip install -e .` registers the `bench` command and `bench --help` shows subcommands. Ask the user if questions arise.

- [x] 5. Implement git metadata module (`git_meta.py`)
  - [x] 5.1 Create `scripts/bench/git_meta.py` with `GitInfo` dataclass and `get_git_info()` function. Run `git rev-parse --short HEAD` and `git rev-parse --abbrev-ref HEAD`, detect dirty state with `git diff --quiet`, fall back to `"unknown"` if not a git repo.
    - _Requirements: 9.1, 9.2, 9.3, 9.4_
  - [ ]* 5.2 Write property test for git hash format (Property 14)
    - **Property 14: Git hash format**
    - **Validates: Requirements 9.1**

- [x] 6. Implement CSV store module (`csv_store.py`)
  - [x] 6.1 Create `scripts/bench/csv_store.py` with `append_main_result()`, `append_category_results()`, `read_main_results()`, and `get_previous_result()`. Auto-create output directory and CSV files with headers if missing. Use the backward-compatible column format from the design.
    - _Requirements: 4.1, 4.2, 4.4, 4.5, 10.2_
  - [ ]* 6.2 Write property test for CSV round-trip (Property 8)
    - **Property 8: CSV result round-trip**
    - **Validates: Requirements 4.1, 4.2, 4.5, 10.2**

- [x] 7. Implement output parsers (`parsers.py`)
  - [x] 7.1 Create `scripts/bench/parsers.py` with `BenchmarkResult`, `CategoryResult`, `GauntletResult` dataclasses, `parse_benchmark_output()` for Catch2 stdout, and `parse_fastchess_output()` for fast-chess stdout. Use the regex patterns from the design.
    - _Requirements: 3.6, 3.7, 5.6, 10.1, 12.3, 12.4_
  - [ ]* 7.2 Write property test for benchmark output parse round-trip (Property 7)
    - **Property 7: Benchmark output parsing round-trip**
    - **Validates: Requirements 3.6, 3.7, 10.1**
  - [ ]* 7.3 Write property test for fast-chess output parsing (Property 11)
    - **Property 11: Fast-chess output parsing**
    - **Validates: Requirements 5.6, 12.3, 12.4**

- [x] 8. Checkpoint — Verify core modules
  - Ensure all tests pass, ask the user if questions arise.

- [x] 9. Implement benchmark runner (`run.py`)
  - [x] 9.1 Create `scripts/bench/run.py` with `cmd_run()`. Implement suite/mode expansion (Cartesian product of `--suite` × `--mode`), Catch2 test name mapping, subprocess invocation of the test binary, output parsing, summary table printing, CSV logging via `csv_store`, and regression detection (WAC >2pp, STS >50pts, NPS >10% drop).
    - _Requirements: 3.1, 3.2, 3.3, 3.4, 3.5, 3.6, 3.7, 3.8, 3.9, 4.1, 4.2, 4.3, 4.6, 10.1, 10.4, 10.5, 11.1, 11.2, 11.3, 11.4, 11.5_
  - [x] 9.2 Wire `cmd_run` into the `run` subparser in `cli.py` with `--suite`, `--mode`, and `--evaluator` flags.
    - _Requirements: 3.2, 3.3, 3.5_
  - [ ]* 9.3 Write property test for suite/mode expansion (Property 6)
    - **Property 6: Suite and mode expansion**
    - **Validates: Requirements 3.2, 3.3, 3.4, 3.5**
  - [ ]* 9.4 Write property test for regression detection thresholds (Property 9)
    - **Property 9: Regression detection thresholds**
    - **Validates: Requirements 4.6, 11.1, 11.2, 11.3, 11.4, 11.5**

- [x] 10. Implement gauntlet runner (`gauntlet.py`)
  - [x] 10.1 Create `scripts/bench/gauntlet.py` with `cmd_gauntlet()`. Implement fast-chess command construction from engine entries + CLI flags + config defaults, real-time stdout streaming with simultaneous file logging, output parsing for W/L/D/Elo/SPRT, and saving PGN + log to timestamped output directory.
    - _Requirements: 5.1, 5.2, 5.3, 5.4, 5.5, 5.6, 5.7, 5.8, 5.9, 5.10, 12.1, 12.2, 12.3, 12.4, 12.5, 12.6_
  - [x] 10.2 Wire `cmd_gauntlet` into the `gauntlet` subparser in `cli.py` with `--type`, `--baseline`, `--candidate`, `--engines`, `--tc`, `--rounds`, `--concurrency`, `--book`, `--book-depth`, `--elo0`, `--elo1`, and `--fast-chess-args` flags.
    - _Requirements: 5.2, 5.3, 5.4, 5.5, 5.10, 12.6_
  - [ ]* 10.3 Write property test for fast-chess command construction (Property 10)
    - **Property 10: Fast-chess command construction**
    - **Validates: Requirements 12.2**
  - [ ]* 10.4 Write property test for gauntlet defaults from config (Property 12)
    - **Property 12: Gauntlet defaults from config**
    - **Validates: Requirements 5.4**

- [x] 11. Implement comparison tool (`compare.py`)
  - [x] 11.1 Create `scripts/bench/compare.py` with `cmd_compare()`. Implement CSV reading and pivoting by evaluator or commit, side-by-side table display with deltas, STS per-category breakdown with >5pp highlighting, and missing-data messages.
    - _Requirements: 6.1, 6.2, 6.3, 6.4, 6.5, 6.6, 6.7, 10.3_
  - [x] 11.2 Wire `cmd_compare` into the `compare` subparser in `cli.py` with `--by` and `--commits` flags.
    - _Requirements: 6.2, 6.4_
  - [ ]* 11.3 Write property test for STS category strength/weakness classification (Property 13)
    - **Property 13: STS category strength/weakness classification**
    - **Validates: Requirements 6.5, 10.5**

- [x] 12. Implement run-all pipeline (`run_all.py`)
  - [x] 12.1 Create `scripts/bench/run_all.py` with `cmd_run_all()`. Orchestrate `cmd_run(--suite all --mode all)` then `cmd_gauntlet(--type sprt)`, collect failures without aborting, print consolidated summary.
    - _Requirements: 7.1, 7.2, 7.3, 7.4, 7.5, 7.6_
  - [x] 12.2 Wire `cmd_run_all` into the `run-all` subparser in `cli.py` with `--evaluator` flag.
    - _Requirements: 7.6_

- [x] 13. Final checkpoint — Full validation
  - Ensure `tox` passes all environments (lint, typecheck, test). Ensure `bench run --help`, `bench gauntlet --help`, `bench compare --help`, and `bench run-all --help` all display correct flags. Ask the user if questions arise.

## Notes

- Tasks marked with `*` are optional and can be skipped for faster MVP
- Each task references specific requirements for traceability
- Checkpoints ensure incremental validation
- Property tests validate universal correctness properties from the design document
- The existing scripts (`run-benchmarks.py`, `run-cutechess.py`, `run-tournament.py`) are not deleted — they stay until the new tool is verified working
- Zero runtime dependencies: only stdlib modules used in production code
- Dev dependencies (ruff, mypy, pytest, hypothesis, tox) are declared in `requirements.txt` and `tox.ini`
