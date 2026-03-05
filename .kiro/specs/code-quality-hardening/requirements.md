# Requirements Document

## Introduction

This spec covers the Code Quality and Tech Debt Hardening track identified in the Blunder engine code review (docs/CODE_REVIEW.md). The goal is to reduce tech debt, improve maintainability, fix latent bugs, and apply modern C++17 idioms — without changing engine strength. Engine strength improvements (search, eval, NNUE) are covered by a separate spec.

## Glossary

- **Engine**: The Blunder chess engine application
- **Board**: The `Board` class that holds game state (bitboards, mailbox, Zobrist hash, move stack)
- **Search**: The `Search` class that owns alpha-beta, quiescence, and iterative deepening algorithms
- **TranspositionTable**: The `TranspositionTable` class that stores previously searched positions
- **TimeManager**: The `TimeManager` class that manages search time budgets
- **NNUEEvaluator**: The `NNUEEvaluator` class implementing the NNUE neural network evaluator
- **UCI_Handler**: The `UCI` class implementing the Universal Chess Interface protocol
- **Xboard_Handler**: The `Xboard` class implementing the Xboard/WinBoard protocol
- **CLI_Parser**: The command-line argument parsing and mode dispatch logic in `main.cpp`
- **MoveGenerator**: The `MoveGenerator` class containing static methods for legal move generation
- **History_Table**: The `int history_[2][64][64]` array in Search used for move ordering
- **Accumulator_Stack**: The `std::vector<AccumulatorEntry> acc_stack_` in NNUEEvaluator

## Requirements

### Requirement 1: Extract Shared CLI Configuration Parsing

**User Story:** As a developer, I want the CLI argument parsing for MCTS/AlphaZero/NNUE configuration to be defined once, so that adding a new mode or flag does not require copy-pasting across xboard, UCI, and selfplay blocks.

#### Acceptance Criteria

1. THE CLI_Parser SHALL parse MCTS simulation count, MCTS exploration constant, AlphaZero mode, and dual-head network loading through a single shared function or configuration struct.
2. WHEN a new protocol mode (xboard, UCI, or selfplay) is added, THE CLI_Parser SHALL reuse the shared configuration without duplicating parsing logic.
3. WHEN the shared parser is used, THE Engine SHALL produce identical runtime behavior to the current implementation for all existing command-line flag combinations.
4. THE CLI_Parser SHALL reduce the line count of `main.cpp` by removing the triplicated MCTS/AlphaZero/NNUE parsing blocks.

### Requirement 2: Board Dependency Injection

**User Story:** As a developer, I want Board to receive its TranspositionTable and evaluators as injected dependencies, so that Board does not own unrelated subsystems and callers can control evaluator selection.

#### Acceptance Criteria

1. THE Board SHALL accept a TranspositionTable reference or pointer through its constructor or a setter, instead of owning a `shared_ptr<TranspositionTable>`.
2. THE Board SHALL accept evaluator dependencies (HandCraftedEvaluator, NNUEEvaluator) through injection rather than owning them internally.
3. THE Board SHALL remove the `get_evaluator()` method that contains NNUE-fallback logic; the caller SHALL select which evaluator to use.
4. WHEN Board is constructed without an injected TranspositionTable, THE Engine SHALL still compile and function correctly using a default-constructed table.
5. WHEN dependency injection is applied, THE Engine SHALL pass all existing tests without regressions.

### Requirement 3: Modernize Constants.h

**User Story:** As a developer, I want compile-time constants to use `constexpr` and non-trivial constants to use `inline const`, so that each translation unit does not get its own copy and ODR violations are avoided.

#### Acceptance Criteria

1. THE Engine SHALL declare all integral and bitboard constants in Constants.h as `constexpr` instead of `const`.
2. THE Engine SHALL declare `PIECE_CHARS` as `inline const std::string` or a `constexpr std::string_view`.
3. THE Engine SHALL declare `PIECE_UNICODE` as `inline const` or `constexpr` to avoid duplicate storage across translation units.
4. WHEN Constants.h is included in multiple translation units, THE linker SHALL produce no duplicate-symbol or ODR-violation warnings.

### Requirement 4: Modernize Common.h

**User Story:** As a developer, I want Common.h to use standard C++ headers and avoid pulling unnecessary transitive includes, so that compile times are reduced and the codebase follows C++ conventions.

#### Acceptance Criteria

1. THE Engine SHALL include `<cassert>` instead of `<assert.h>` in Common.h.
2. THE Engine SHALL remove the `<iostream>` include from Common.h.
3. WHEN `<iostream>` is removed from Common.h, each source file that uses `std::cout` or `std::cerr` SHALL include `<iostream>` directly.
4. WHEN Common.h is updated, THE Engine SHALL compile and pass all tests without regressions.

### Requirement 5: Replace Preprocessor Defines in Search.h

**User Story:** As a developer, I want the PV and null-move flag constants to be scoped and type-safe, so that they do not pollute the global preprocessor namespace.

#### Acceptance Criteria

1. THE Engine SHALL replace `#define NO_PV`, `IS_PV`, `NO_NULL`, and `DO_NULL` in Search.h with `constexpr int` constants or an `enum`/`enum class`.
2. WHEN the defines are replaced, THE Engine SHALL compile and pass all tests without regressions.

### Requirement 6: Remove Redundant NDEBUG Guards Around assert()

**User Story:** As a developer, I want assert() calls to stand alone without redundant `#ifndef NDEBUG` guards, so that the code is cleaner and the intent is clear.

#### Acceptance Criteria

1. THE Engine SHALL remove all `#ifndef NDEBUG` / `#endif` blocks that wrap only `assert()` calls in Search.cpp.
2. WHEN the guards are removed, THE Engine SHALL produce identical behavior in both debug and release builds.

### Requirement 7: Declare Variables at Point of Use in Search.cpp

**User Story:** As a developer, I want variables declared at their point of first use rather than at function top, so that scope is minimized and readability is improved.

#### Acceptance Criteria

1. THE Engine SHALL move C-style variable declarations (e.g., `int i, n, value;`) in Search.cpp to the point of first use.
2. WHEN variables are moved, THE Engine SHALL compile and pass all tests without regressions.

### Requirement 8: Relocate TestPositions to Test Directory

**User Story:** As a developer, I want test infrastructure code to live in the test directory, so that production binaries do not include test-only code.

#### Acceptance Criteria

1. THE Engine SHALL move `TestPositions.cpp` and `TestPositions.h` from `source/` to `test/source/` or conditionally compile them out of `blunder_lib`.
2. WHEN TestPositions is relocated, THE `--test-positions` CLI command SHALL still function correctly.
3. WHEN TestPositions is relocated, THE production library (`blunder_lib`) SHALL not contain `test_positions_benchmark` or `perft_benchmark` symbols.

### Requirement 9: UCI go ponder Support

**User Story:** As a developer, I want the UCI handler to support the `go ponder` command, so that the engine can think on the opponent's time in UCI mode.

#### Acceptance Criteria

1. WHEN the UCI_Handler receives a `go ponder` command, THE UCI_Handler SHALL start a search in pondering mode.
2. WHEN the UCI_Handler receives a `ponderhit` command while pondering, THE UCI_Handler SHALL switch the search from pondering to normal time-managed mode.
3. WHEN the UCI_Handler receives a `stop` command while pondering, THE UCI_Handler SHALL abort the search and return the best move found so far.

### Requirement 10: UCI Thread Safety

**User Story:** As a developer, I want shared state between the UCI search thread and the main command thread to be protected by a mutex, so that data races cannot occur even with misbehaving GUIs.

#### Acceptance Criteria

1. THE UCI_Handler SHALL protect access to `board_` with a mutex when the search thread is running.
2. WHEN the search thread is active, THE UCI_Handler SHALL prevent concurrent modification of `board_` from the main thread.
3. WHEN thread safety is added, THE Engine SHALL not deadlock during normal UCI command sequences (position, go, stop, quit).

### Requirement 11: Wire TT Resize to UCI Hash and Xboard memory Commands

**User Story:** As a developer, I want the `setoption Hash` (UCI) and `memory` (Xboard) commands to actually resize the transposition table, so that users can configure TT size.

#### Acceptance Criteria

1. WHEN the UCI_Handler receives `setoption name Hash value N`, THE TranspositionTable SHALL be resized to hold approximately N megabytes of entries.
2. WHEN the Xboard_Handler receives a `memory N` command, THE TranspositionTable SHALL be resized to hold approximately N megabytes of entries.
3. THE TranspositionTable SHALL provide a `resize(int size)` method that reallocates the internal storage.
4. WHEN the table is resized, THE TranspositionTable SHALL clear all existing entries.

### Requirement 12: Use Compiler Intrinsic for pop_count

**User Story:** As a developer, I want `pop_count` to use a compiler intrinsic, so that it compiles to a single hardware instruction on supported platforms.

#### Acceptance Criteria

1. THE Engine SHALL implement `pop_count` using `__builtin_popcountll` on GCC/Clang or `__popcnt64` / `_mm_popcnt_u64` on MSVC.
2. IF the compiler does not support the intrinsic, THEN THE Engine SHALL fall back to the current manual loop implementation.
3. WHEN the intrinsic is used, THE Engine SHALL produce identical results to the manual implementation for all 64-bit inputs.

### Requirement 13: Pre-reserve NNUE Accumulator Stack

**User Story:** As a developer, I want the NNUE accumulator stack to be pre-allocated, so that no heap allocations occur during search.

#### Acceptance Criteria

1. THE NNUEEvaluator SHALL reserve `acc_stack_` capacity for at least `MAX_SEARCH_PLY` entries in its constructor.
2. WHEN `push()` is called during search, THE Accumulator_Stack SHALL not trigger a heap allocation (assuming search depth does not exceed `MAX_SEARCH_PLY`).

### Requirement 14: TT Index via Bitmask Instead of Modulo

**User Story:** As a developer, I want the transposition table to use a bitmask for index computation, so that the probe and record operations avoid an expensive integer division.

#### Acceptance Criteria

1. THE TranspositionTable SHALL compute the entry index using `hash & (size - 1)` instead of `hash % size`.
2. THE TranspositionTable SHALL enforce that its size is always a power of two.
3. WHEN the bitmask is used, THE TranspositionTable SHALL produce identical probe and record behavior for all hash values.

### Requirement 15: History Table Aging/Decay

**User Story:** As a developer, I want the history heuristic table to be periodically decayed, so that values do not grow without bound and overflow after many search iterations.

#### Acceptance Criteria

1. THE Search SHALL halve (or otherwise decay) all history table entries between iterative deepening iterations.
2. WHEN history values are decayed, THE Search SHALL preserve the relative ordering of history entries.
3. WHEN history aging is applied, THE Engine SHALL pass all existing search-related tests without regressions.

### Requirement 16: Fix adjust_for_score Repeated Application Bug

**User Story:** As a developer, I want the score-based time adjustment to be applied at most once per search, so that the soft time limit is not repeatedly shrunk across iterations.

#### Acceptance Criteria

1. THE TimeManager SHALL apply the `adjust_for_score` reduction at most once per `allocate()` or `start()` cycle.
2. WHEN `adjust_for_score` is called multiple times with a score above the threshold, THE TimeManager SHALL not reduce the soft limit below the value after the first application.
3. WHEN the fix is applied, THE Engine SHALL pass all existing tests without regressions.

### Requirement 17: Optimize is_draw to Scan Only Since Last Irreversible Move

**User Story:** As a developer, I want `is_draw` to scan only the positions since the last irreversible move, so that draw detection is faster in long games.

#### Acceptance Criteria

1. THE Board SHALL limit the `is_draw` hash-history scan to positions from `game_ply_ - half_move_count` to `game_ply_`, stepping by 2 (same side to move).
2. WHEN the optimized scan is used, THE Board SHALL produce identical draw detection results to the current full-history scan for all positions.

### Requirement 18: Fix UCI Castling Notation for Ponder Moves

**User Story:** As a developer, I want `move_to_uci()` to produce correct castling notation for ponder moves, so that GUIs receive valid move strings.

#### Acceptance Criteria

1. WHEN `move_to_uci()` formats a castling ponder move, THE UCI_Handler SHALL determine the castling side from the move itself (piece color encoded in the move), not from `board_.side_to_move()`.
2. WHEN a ponder move is a castling move, THE UCI_Handler SHALL produce the correct UCI notation (e.g., `e1g1` for white kingside, `e8g8` for black kingside).

### Requirement 19: Replace clock() with std::chrono::steady_clock

**User Story:** As a developer, I want all timing to use wall-clock time via `std::chrono::steady_clock`, so that time measurements are accurate regardless of CPU load or threading.

#### Acceptance Criteria

1. THE TimeManager SHALL use `std::chrono::steady_clock` instead of `clock()` for all time measurements.
2. THE Search SHALL use `std::chrono::steady_clock` instead of `clock()` in `SearchStats`.
3. WHEN `steady_clock` is used, THE Engine SHALL measure wall-clock time, not CPU time.

### Requirement 20: Make Zobrist a Singleton or Use Static Data

**User Story:** As a developer, I want Zobrist keys to be shared across all Board instances, so that memory is not wasted on identical copies.

#### Acceptance Criteria

1. THE Engine SHALL store Zobrist random keys as static data (either a singleton instance or static member arrays) shared across all Board objects.
2. WHEN multiple Board objects exist, THE Engine SHALL use a single copy of the Zobrist key tables.

### Requirement 21: ScoredMove.score Signed Type

**User Story:** As a developer, I want `ScoredMove.score` to use a signed integer type, so that negative scores are representable without fragile unsigned-to-positive mapping.

#### Acceptance Criteria

1. THE Engine SHALL change `ScoredMove.score` from `U16` to a signed type (e.g., `int16_t` or `int`).
2. WHEN the type is changed, THE Engine SHALL compile and pass all tests without regressions.

### Requirement 22: Convert MoveGenerator from Static-Methods Class to Namespace

**User Story:** As a developer, I want MoveGenerator to be a namespace rather than a class with only static methods, so that the code accurately reflects its design.

#### Acceptance Criteria

1. THE Engine SHALL convert `MoveGenerator` from a class with static methods to a namespace containing free functions.
2. WHEN the conversion is applied, THE Engine SHALL compile and pass all tests, including perft and move generation tests, without regressions.

### Requirement 23: Add --output Flag to analyze-sts.py

**User Story:** As a developer, I want `analyze-sts.py` to support a `--output` flag for JSON or CSV export, so that STS results can be tracked programmatically over time.

#### Acceptance Criteria

1. WHEN the `--output` flag is provided with a file path, THE script SHALL write results in JSON or CSV format to the specified file.
2. WHEN the `--output` flag is omitted, THE script SHALL print results to stdout as it does today.
3. THE exported data SHALL include per-category scores, total score, and estimated Elo.
