# Requirements Document

## Introduction

Consolidate the existing ad-hoc benchmarking scripts (`run-benchmarks.py`, `run-cutechess.py`, `run-tournament.py`, `analyze-sts.py`) into a single unified Python CLI tool with subcommands. The tool replaces cutechess-cli with fast-chess (pure C++17, no Qt dependency, builds with `make -j` on any platform), introduces a JSON configuration file for cross-platform path resolution, provides an engine registry for named engine definitions, and tracks all results in structured CSV files with git metadata. The goal is a one-command benchmarking workflow that covers WAC/STS test suites, gauntlet matches, and cross-evaluator comparison across Linux (AL2) and Windows (MinGW) development environments.

## Glossary

- **Bench_CLI**: The unified Python 3 command-line tool (`scripts/bench.py`) that provides all benchmarking subcommands.
- **Config_File**: A JSON configuration file (`scripts/bench-config.json`) that defines cross-platform paths, engine registry entries, build presets, and default settings.
- **Engine_Registry**: A section of the Config_File that defines named engine entries, each specifying a binary path, protocol, and UCI/Xboard options.
- **Fast_Chess**: A modern chess engine match runner (pure C++17, no Qt dependency) used for SPRT and round-robin gauntlet testing. Replaces cutechess-cli.
- **WAC**: Win At Chess — a 300-position tactical test suite used to measure solve rate.
- **STS**: Strategic Test Suite — a 1500-position strategic test suite across 15 categories, scored by move quality.
- **STS_Category**: One of 15 strategic themes in STS (Undermining, Open Files and Diagonals, Knight Outposts, Square Vacancy, Bishop vs Knight, Recapturing, Offer of Simplification, Kingside Pawn Advance, Queenside Pawn Advance, Simplification, King Activity, Center Control, Pawn Play in Center, 7th Rank Play, Avoid Pointless Exchange).
- **Catch2_Test_Binary**: The `blunder_test` (or `blunder_test.exe`) executable built with Catch2 v3.4.0 that runs WAC and STS test suites in fixed-nodes, fixed-depth, and fixed-time modes.
- **Benchmark_Mode**: One of three search constraint modes: `nodes` (fixed 1M nodes per position), `depth` (fixed depth 8), or `time` (fixed 1 second per position).
- **SPRT**: Sequential Probability Ratio Test — a statistical method for determining whether one engine is stronger than another within specified Elo bounds.
- **Round_Robin**: A tournament format where every engine plays every other engine.
- **Gauntlet**: A tournament format where one engine plays against all others.
- **NPS**: Nodes Per Second — the number of search tree nodes evaluated per second, measuring engine speed.
- **Evaluator_Type**: One of the engine's evaluation backends: `hce` (HandCraftedEvaluator), `nnue` (NNUEEvaluator), or `alphazero` (AlphaZero/MCTS).
- **Build_Preset**: A named CMake preset (e.g., `dev`, `rel`, `dev-mingw`) that determines the build directory and compiler configuration.
- **Result_CSV**: Structured CSV files in `scripts/output/` that store benchmark results with git commit hash, branch, timestamp, evaluator type, and scores.

## Requirements

### Requirement 1: JSON Configuration File

**User Story:** As a chess engine developer, I want a single JSON configuration file that defines all paths and settings for both Linux and Windows, so that I can switch between development environments without editing scripts.

#### Acceptance Criteria

1. THE Bench_CLI SHALL load configuration from a Config_File located at `scripts/bench-config.json` relative to the project root.
2. THE Config_File SHALL contain a `platforms` object with `linux` and `windows` keys, each defining platform-specific paths for the engine binary, Catch2_Test_Binary, Fast_Chess binary, opening book, and test suite EPD files.
3. THE Config_File SHALL contain a `defaults` object specifying the default Build_Preset, Evaluator_Type, Benchmark_Mode, time control, concurrency, and SPRT Elo bounds.
4. WHEN the Bench_CLI starts, THE Bench_CLI SHALL detect the current operating system and select the corresponding platform paths from the Config_File.
5. IF the Config_File does not exist at the expected path, THEN THE Bench_CLI SHALL exit with a descriptive error message indicating the expected file location.
6. IF a required path in the Config_File points to a non-existent file, THEN THE Bench_CLI SHALL exit with a descriptive error message identifying the missing file.
7. THE Bench_CLI SHALL accept a `--config` command-line flag to override the default Config_File path.

### Requirement 2: Engine Registry

**User Story:** As a chess engine developer, I want to define named engine entries in the configuration, so that I can reference engines by name (e.g., "blunder-hce", "blunder-nnue", "stockfish") instead of specifying paths and options every time.

#### Acceptance Criteria

1. THE Config_File SHALL contain an `engines` object where each key is a unique engine name and each value defines the engine's binary path, protocol (`uci` or `xboard`), and optional UCI/Xboard options.
2. THE Engine_Registry SHALL support platform-specific binary paths by allowing the `cmd` field to reference a platform path variable (e.g., `"${engine_binary}"`) that resolves from the `platforms` section.
3. THE Engine_Registry SHALL support specifying additional command-line arguments for each engine entry.
4. WHEN a subcommand references an engine by name, THE Bench_CLI SHALL resolve the engine definition from the Engine_Registry in the Config_File.
5. IF a referenced engine name does not exist in the Engine_Registry, THEN THE Bench_CLI SHALL exit with a descriptive error message listing the available engine names.
6. THE Engine_Registry SHALL support defining external engines (e.g., Stockfish) with their own binary paths and UCI options for comparison purposes.

### Requirement 3: Unified Benchmark Runner (bench run)

**User Story:** As a chess engine developer, I want a single subcommand that runs WAC and STS benchmarks in any combination of modes, so that I can measure engine strength with one command instead of managing multiple scripts.

#### Acceptance Criteria

1. THE Bench_CLI SHALL provide a `run` subcommand that executes WAC and STS test suites via the Catch2_Test_Binary.
2. THE `run` subcommand SHALL accept a `--suite` flag with values `wac`, `sts`, or `all` (default: `all`).
3. THE `run` subcommand SHALL accept a `--mode` flag with values `nodes`, `depth`, `time`, or `all` (default: `all`).
4. WHEN `--mode all` is specified, THE `run` subcommand SHALL execute the selected suites in all three Benchmark_Modes sequentially (nodes, depth, time).
5. THE `run` subcommand SHALL accept an `--evaluator` flag with values `hce`, `nnue`, or `alphazero` to tag results with the Evaluator_Type.
6. WHEN a benchmark run completes, THE `run` subcommand SHALL parse the Catch2_Test_Binary output to extract score, maximum score, score percentage, ELO estimate, and NPS.
7. WHEN an STS benchmark run completes, THE `run` subcommand SHALL parse per-category scores for all 15 STS_Categories.
8. WHEN a benchmark run completes, THE `run` subcommand SHALL print a summary table to stdout showing suite, mode, score, percentage, ELO, and NPS.
9. IF the Catch2_Test_Binary exits with a non-zero return code, THEN THE `run` subcommand SHALL report the failure and continue with remaining suites and modes.

### Requirement 4: Result Tracking and CSV Logging

**User Story:** As a chess engine developer, I want all benchmark results automatically logged to CSV files with git metadata, so that I can track engine strength over time and detect regressions.

#### Acceptance Criteria

1. WHEN a benchmark run completes, THE Bench_CLI SHALL append a row to `scripts/output/benchmarks.csv` containing: timestamp, git commit hash, git branch name, Evaluator_Type, Benchmark_Mode, suite name, score percentage, raw score, maximum score, ELO estimate, NPS, total nodes, total time in seconds, and a pass/fail indicator.
2. WHEN an STS benchmark run completes, THE Bench_CLI SHALL append rows to `scripts/output/benchmarks_categories.csv` containing: timestamp, git commit hash, git branch name, Evaluator_Type, Benchmark_Mode, STS_Category name, score percentage, raw score, maximum score, and number of positions.
3. THE Bench_CLI SHALL auto-detect the current git commit hash and branch name using `git rev-parse` commands.
4. IF the output CSV files do not exist, THEN THE Bench_CLI SHALL create them with the appropriate header row.
5. THE Bench_CLI SHALL preserve backward compatibility with the existing `benchmarks.csv` column format.
6. WHEN a benchmark run completes, THE Bench_CLI SHALL compare the new score against the most recent previous result for the same suite, mode, and Evaluator_Type combination, and print a warning if the score has regressed.

### Requirement 5: Gauntlet Runner (bench gauntlet)

**User Story:** As a chess engine developer, I want a subcommand that runs SPRT and round-robin matches using fast-chess, so that I can validate engine strength changes against other engines without needing cutechess-cli or Qt.

#### Acceptance Criteria

1. THE Bench_CLI SHALL provide a `gauntlet` subcommand that runs engine-vs-engine matches using Fast_Chess.
2. THE `gauntlet` subcommand SHALL accept a `--type` flag with values `sprt` or `roundrobin` (default: `sprt`).
3. THE `gauntlet` subcommand SHALL accept `--baseline` and `--candidate` flags that reference engine names from the Engine_Registry.
4. THE `gauntlet` subcommand SHALL accept `--tc` (time control), `--rounds`, `--concurrency`, `--book`, and `--book-depth` flags, with defaults loaded from the Config_File.
5. WHEN `--type sprt` is specified, THE `gauntlet` subcommand SHALL accept `--elo0` and `--elo1` flags for SPRT hypothesis bounds, with defaults from the Config_File.
6. WHEN a gauntlet run completes, THE `gauntlet` subcommand SHALL parse the Fast_Chess output to extract win/loss/draw counts and Elo difference.
7. WHEN a gauntlet run completes, THE `gauntlet` subcommand SHALL save the full PGN output and match log to a timestamped directory under `scripts/output/`.
8. IF the Fast_Chess binary is not found at the configured path, THEN THE `gauntlet` subcommand SHALL exit with a descriptive error message.
9. THE `gauntlet` subcommand SHALL stream Fast_Chess output to stdout in real time while simultaneously logging to a file.
10. WHEN `--type roundrobin` is specified, THE `gauntlet` subcommand SHALL accept an `--engines` flag listing multiple engine names from the Engine_Registry to include in the tournament.

### Requirement 6: Engine Comparison (bench compare)

**User Story:** As a chess engine developer, I want to compare benchmark results across different evaluators and engine versions side by side, so that I can see which evaluator or commit performs best.

#### Acceptance Criteria

1. THE Bench_CLI SHALL provide a `compare` subcommand that reads Result_CSV files and displays comparison tables.
2. THE `compare` subcommand SHALL accept a `--by` flag with values `evaluator`, `commit`, or `version` to select the comparison axis.
3. WHEN `--by evaluator` is specified, THE `compare` subcommand SHALL display the most recent results for each Evaluator_Type side by side, showing score percentage, ELO, and NPS for each suite and mode.
4. WHEN `--by commit` is specified, THE `compare` subcommand SHALL accept `--commits` with two or more git commit hashes and display their results side by side.
5. THE `compare` subcommand SHALL display STS per-category breakdowns when STS results are available, highlighting categories where scores differ by more than 5 percentage points.
6. THE `compare` subcommand SHALL compute and display the delta (difference) between compared entries for score percentage, ELO, and NPS.
7. IF no results exist in the Result_CSV for a requested comparison, THEN THE `compare` subcommand SHALL print a descriptive message indicating which data is missing.

### Requirement 7: Run-All Mode (bench run-all)

**User Story:** As a chess engine developer, I want a single command that runs all benchmarks and gauntlet tests in sequence, so that I can kick off a full evaluation pipeline before stepping away.

#### Acceptance Criteria

1. THE Bench_CLI SHALL provide a `run-all` subcommand that executes a full benchmarking pipeline.
2. THE `run-all` subcommand SHALL execute the `run` subcommand with `--suite all --mode all` for the configured Evaluator_Type.
3. THE `run-all` subcommand SHALL execute the `gauntlet` subcommand with `--type sprt` using the default baseline and candidate engines from the Config_File.
4. WHEN all steps complete, THE `run-all` subcommand SHALL print a consolidated summary showing WAC scores, STS scores, NPS, and gauntlet results.
5. IF any step in the pipeline fails, THEN THE `run-all` subcommand SHALL log the failure, continue with remaining steps, and report all failures in the final summary.
6. THE `run-all` subcommand SHALL accept an `--evaluator` flag to override the default Evaluator_Type for the benchmark run step.

### Requirement 8: Cross-Platform Path Resolution

**User Story:** As a chess engine developer working on both Linux (AL2) and Windows (MinGW), I want the tool to automatically resolve the correct paths for my platform, so that I can use the same commands on either system.

#### Acceptance Criteria

1. WHEN the Bench_CLI starts, THE Bench_CLI SHALL detect the current platform using Python's `os.name` or `sys.platform`.
2. THE Bench_CLI SHALL resolve all binary and file paths from the platform-specific section of the Config_File based on the detected platform.
3. THE Config_File SHALL support Build_Preset references in paths, so that the engine binary path can include the preset name (e.g., `build/{preset}/blunder`).
4. THE Bench_CLI SHALL accept a `--preset` command-line flag to override the default Build_Preset, which affects path resolution for engine and test binaries.
5. WHEN resolving paths, THE Bench_CLI SHALL expand `~` to the user's home directory and resolve relative paths against the project root directory.
6. THE Bench_CLI SHALL append the correct platform-specific executable extension (`.exe` on Windows, none on Linux) when resolving binary paths.

### Requirement 9: Git Metadata Collection

**User Story:** As a chess engine developer, I want every benchmark result tagged with the git commit hash and branch, so that I can correlate performance changes with specific code changes.

#### Acceptance Criteria

1. WHEN logging results, THE Bench_CLI SHALL capture the current git commit hash (short form, 7 characters) by running `git rev-parse --short HEAD`.
2. WHEN logging results, THE Bench_CLI SHALL capture the current git branch name by running `git rev-parse --abbrev-ref HEAD`.
3. IF the project directory is not a git repository, THEN THE Bench_CLI SHALL use `"unknown"` as the commit hash and branch name.
4. IF the git working tree has uncommitted changes, THEN THE Bench_CLI SHALL append `"-dirty"` to the commit hash.

### Requirement 10: STS Per-Category Tracking

**User Story:** As a chess engine developer, I want individual scores for each of the 15 STS categories tracked over time, so that I can identify specific strategic strengths and weaknesses of each evaluator.

#### Acceptance Criteria

1. WHEN an STS benchmark completes, THE Bench_CLI SHALL parse and record scores for all 15 STS_Categories individually.
2. THE Bench_CLI SHALL store per-category results in `scripts/output/benchmarks_categories.csv` with one row per category per run.
3. THE `compare` subcommand SHALL display per-category STS scores when comparing evaluators or commits.
4. THE `run` subcommand SHALL print a per-category breakdown table to stdout after each STS run, showing category name, score, maximum score, and percentage.
5. THE per-category breakdown SHALL identify categories scoring more than 5 percentage points above or below the overall average as strengths or weaknesses respectively.

### Requirement 11: Regression Detection

**User Story:** As a chess engine developer, I want automatic warnings when benchmark scores regress compared to previous runs, so that I can catch strength regressions early.

#### Acceptance Criteria

1. WHEN a benchmark run completes, THE Bench_CLI SHALL compare the new score percentage against the most recent previous result for the same suite, mode, and Evaluator_Type.
2. IF the new WAC score percentage is more than 2 percentage points lower than the previous result, THEN THE Bench_CLI SHALL print a regression warning to stderr.
3. IF the new STS score is more than 50 points lower than the previous result, THEN THE Bench_CLI SHALL print a regression warning to stderr.
4. IF the new NPS is more than 10 percent lower than the previous NPS for the same configuration, THEN THE Bench_CLI SHALL print a speed regression warning to stderr.
5. THE regression warnings SHALL include the previous value, the new value, and the delta.

### Requirement 12: Fast-Chess Integration

**User Story:** As a chess engine developer working on Amazon Linux 2, I want to use fast-chess instead of cutechess-cli for gauntlet testing, so that I can run engine matches without installing Qt or dealing with complex dependencies.

#### Acceptance Criteria

1. THE `gauntlet` subcommand SHALL invoke Fast_Chess as the match runner for all SPRT and round-robin matches.
2. THE Bench_CLI SHALL construct Fast_Chess command-line arguments for engine definitions, time control, opening book, SPRT parameters, concurrency, and PGN output.
3. THE Bench_CLI SHALL parse Fast_Chess stdout output to extract match results, including win/loss/draw counts and Elo difference with error margins.
4. WHEN running an SPRT test, THE Bench_CLI SHALL detect and report the SPRT conclusion (H0 accepted, H1 accepted, or inconclusive).
5. THE Config_File SHALL specify the Fast_Chess binary path in the platform-specific paths section.
6. THE Bench_CLI SHALL support passing additional Fast_Chess arguments via a `--fast-chess-args` flag for advanced use cases.

### Requirement 13: Python Project Structure and Dependency Management

**User Story:** As a chess engine developer, I want the Python tooling to follow modern Python project conventions with declared dependencies and reproducible environments, so that setup on a new machine is a single `pip install` command and code quality is enforced via tox.

#### Acceptance Criteria

1. THE project SHALL contain a `pyproject.toml` at the repository root that declares the project name, Python version requirement (>=3.10), and all runtime dependencies.
2. THE project SHALL contain a `requirements.txt` at the repository root for quick dependency installation via `pip install -r requirements.txt`.
3. THE Bench_CLI SHALL be installable in development mode via `pip install -e .` from the repository root, which registers the `bench` console entry point.
4. THE project SHALL contain a `tox.ini` at the repository root that defines environments for linting (ruff), type checking (mypy), and optionally running the Python test suite.
5. THE `pyproject.toml` SHALL declare a `[project.scripts]` entry point that maps the `bench` command to the Bench_CLI main function.
6. THE runtime dependencies SHALL be kept minimal — only standard library modules and, if needed, a CLI framework (e.g., `click` or `argparse` with no external dependency).
7. THE `tox.ini` SHALL define a `lint` environment that runs `ruff check` and a `typecheck` environment that runs `mypy` with strict mode on the scripts directory.
