# Implementation Plan: Code Quality Hardening

## Overview

Incremental code quality improvements for the Blunder chess engine across seven categories: code modernization, architecture cleanup, protocol correctness, performance micro-optimizations, bug fixes, timing modernization, and tooling. All changes are strength-neutral. Tasks are ordered so that foundational changes (constants, types, namespaces) land first, followed by architecture refactors, then protocol/performance/bug-fix work, and finally tooling.

## Tasks

- [x] 1. Code modernization: Constants, Common.h, and type cleanups
  - [x] 1.1 Modernize Constants.h — convert `const` to `constexpr`, declare `PIECE_CHARS` as `inline const std::string` or `constexpr std::string_view`, declare `PIECE_UNICODE` as `inline const`
    - Update all integral and bitboard constants to `constexpr`
    - Update string constants to `inline const` or `constexpr std::string_view`
    - Verify no duplicate-symbol or ODR-violation warnings when included in multiple TUs
    - _Requirements: 3.1, 3.2, 3.3, 3.4_

  - [x] 1.2 Modernize Common.h — replace `<assert.h>` with `<cassert>`, remove `<iostream>` include, add `<iostream>` to each source file that needs it
    - Replace `<assert.h>` with `<cassert>`
    - Remove `<iostream>` from Common.h
    - Grep for `std::cout` / `std::cerr` usage and add direct `<iostream>` includes where needed
    - _Requirements: 4.1, 4.2, 4.3, 4.4_

  - [x] 1.3 Replace preprocessor defines in Search.h — convert `NO_PV`, `IS_PV`, `NO_NULL`, `DO_NULL` to `constexpr int` or `enum class`
    - Remove `#define` lines and add scoped constants or enum
    - Update all usage sites in Search.cpp and any other files
    - _Requirements: 5.1, 5.2_

  - [x] 1.4 Remove redundant `#ifndef NDEBUG` guards around `assert()` calls in Search.cpp
    - Remove the guards, leave the `assert()` calls in place
    - _Requirements: 6.1, 6.2_

  - [x] 1.5 Declare variables at point of use in Search.cpp — move C-style top-of-function declarations to first use
    - Identify all C-style multi-declarations (e.g., `int i, n, value;`)
    - Move each variable to its point of first use with appropriate type
    - _Requirements: 7.1, 7.2_

  - [x] 1.6 Change `ScoredMove.score` from `U16` to a signed type (`int16_t` or `int`)
    - Update the struct definition
    - Update all comparison and assignment sites
    - _Requirements: 21.1, 21.2_

  - [x] 1.7 Convert MoveGenerator from static-methods class to namespace
    - Change `class MoveGenerator` to `namespace MoveGenerator`
    - Convert all static methods to free functions
    - Update all call sites (remove `MoveGenerator::` if already present, or keep as namespace qualifier)
    - Update test files that reference MoveGenerator
    - _Requirements: 22.1, 22.2_

  - [x] 1.8 Make Zobrist keys static data shared across all Board instances
    - Convert Zobrist key arrays to `static` members or a singleton
    - Add `Zobrist::init()` called once from `main()`
    - Remove per-Board Zobrist copies
    - _Requirements: 20.1, 20.2_

  - [ ]* 1.9 Write property test: Zobrist keys shared across Board instances (Property 11)
    - **Property 11: Zobrist keys are shared across Board instances**
    - Create random (piece, square) pairs, verify two Board instances return identical Zobrist key values
    - **Validates: Requirements 20.1**

- [x] 2. Checkpoint — Ensure all tests pass after code modernization
  - Run `cmake --build --preset=dev && ctest --preset=dev --output-on-failure`
  - Ensure all existing tests pass, ask the user if questions arise.

- [-] 3. Architecture cleanup: CLI config, Board DI, TestPositions relocation
  - [x] 3.1 Extract shared CLI configuration parsing — create `CLIConfig` struct and `parse_cli()` function in `source/CLIConfig.h` / `source/CLIConfig.cpp`
    - Define `CLIConfig` struct with fields: `use_mcts`, `mcts_simulations`, `mcts_cpuct`, `alphazero_mode`, `nnue_path`, `book_path`, `dual_head_path`
    - Implement `parse_cli(int argc, char** argv, CLIConfig& config)` returning protocol mode string
    - _Requirements: 1.1, 1.2, 1.4_

  - [x] 3.2 Refactor `main.cpp` to use `CLIConfig` — replace triplicated MCTS/AlphaZero/NNUE parsing blocks with single `parse_cli()` call
    - Remove duplicated parsing logic from xboard, UCI, and selfplay blocks
    - Pass `CLIConfig` to each protocol handler
    - Verify identical runtime behavior for all existing flag combinations
    - _Requirements: 1.1, 1.2, 1.3, 1.4_

  - [ ]* 3.3 Write property test: CLI parsing produces correct configuration (Property 1)
    - **Property 1: CLI parsing produces correct configuration**
    - Generate random subsets of valid CLI flags, verify `parse_cli` produces matching `CLIConfig` field values
    - **Validates: Requirements 1.1, 1.3**

  - [ ] 3.4 Implement Board dependency injection — Board accepts TT reference and evaluator pointers via constructor/setters
    - Add `Board(TranspositionTable& tt)` constructor
    - Add `set_evaluator()` / `set_nnue()` setters
    - Remove `get_evaluator()` method; move evaluator selection to callers (Search, UCI, Xboard)
    - Keep default constructor with internal `default_tt_`
    - Update all Board construction sites
    - _Requirements: 2.1, 2.2, 2.3, 2.4, 2.5_

  - [x] 3.5 Relocate TestPositions to test directory — move `TestPositions.cpp` and `TestPositions.h` from `source/` to `test/source/` or conditionally compile out of `blunder_lib`
    - Move or conditionally compile the files
    - Update CMakeLists.txt so `blunder_lib` does not contain test symbols
    - Ensure `--test-positions` CLI command still works
    - _Requirements: 8.1, 8.2, 8.3_

- [ ] 4. Checkpoint — Ensure all tests pass after architecture cleanup
  - Run `cmake --build --preset=dev && ctest --preset=dev --output-on-failure`
  - Ensure all existing tests pass, ask the user if questions arise.

- [-] 5. Performance micro-optimizations
  - [-] 5.1 Replace `pop_count` with compiler intrinsic — use `__builtin_popcountll` on GCC/Clang, `__popcnt64` on MSVC, with manual fallback
    - Implement the conditional intrinsic in the appropriate header
    - _Requirements: 12.1, 12.2, 12.3_

  - [ ]* 5.2 Write property test: pop_count intrinsic equivalence (Property 4)
    - **Property 4: pop_count intrinsic equivalence**
    - Generate random U64 values, verify intrinsic matches manual bit-counting loop
    - **Validates: Requirements 12.3**

  - [ ] 5.3 TT bitmask indexing — enforce power-of-two size, replace `hash % size` with `hash & (size - 1)`
    - Add `mask_` member to TranspositionTable
    - Round requested sizes down to nearest power of two
    - Replace modulo with bitmask in `probe()` and `record()`
    - _Requirements: 14.1, 14.2, 14.3_

  - [ ]* 5.4 Write property tests: TT bitmask index equals modulo (Property 5) and TT size is always power of two (Property 6)
    - **Property 5: TT bitmask index equals modulo index**
    - Generate random U64 hash and random power-of-two size, verify `hash & (size - 1) == hash % size`
    - **Property 6: TT size is always a power of two**
    - Generate random requested sizes in [1, 4096], verify actual size is power of two
    - **Validates: Requirements 14.1, 14.2**

  - [ ] 5.5 Pre-reserve NNUE accumulator stack — reserve `acc_stack_` capacity for `MAX_SEARCH_PLY` entries in NNUEEvaluator constructor
    - Add `acc_stack_.reserve(MAX_SEARCH_PLY)` in constructor
    - _Requirements: 13.1, 13.2_

  - [ ] 5.6 Optimize `is_draw` to scan only since last irreversible move — limit hash-history scan to `game_ply_ - half_move_count` to `game_ply_`, stepping by 2
    - Modify `is_draw()` in Board to use the narrower scan window
    - Verify identical results to full-history scan
    - _Requirements: 17.1, 17.2_

  - [ ]* 5.7 Write property test: Optimized is_draw equivalence (Property 9)
    - **Property 9: Optimized is_draw equivalence**
    - Generate random game move sequences from starting position, verify optimized `is_draw()` matches original full-history scan
    - **Validates: Requirements 17.2**

- [ ] 6. Checkpoint — Ensure all tests pass after performance optimizations
  - Run `cmake --build --preset=dev && ctest --preset=dev --output-on-failure`
  - Ensure all existing tests pass, ask the user if questions arise.

- [ ] 7. Bug fixes: History aging and TimeManager
  - [ ] 7.1 Implement history table aging — add `age_history()` method to Search that halves all `history_[2][64][64]` entries between iterative deepening iterations
    - Add the method and call it at the start of each new ID iteration
    - _Requirements: 15.1, 15.2, 15.3_

  - [ ]* 7.2 Write property test: History aging halves values and preserves ordering (Property 7)
    - **Property 7: History aging halves all values and preserves ordering**
    - Generate random history table values, verify each entry equals `prev / 2` after aging, and relative ordering is preserved
    - **Validates: Requirements 15.1, 15.2**

  - [ ] 7.3 Fix `adjust_for_score` repeated application — add `score_adjusted_` flag to TimeManager, reset in `allocate()`/`start()`, check in `adjust_for_score()`
    - Add `bool score_adjusted_ = false` member
    - Guard `adjust_for_score` to apply reduction at most once per cycle
    - Reset flag in `allocate()` and `start()`
    - _Requirements: 16.1, 16.2, 16.3_

  - [ ]* 7.4 Write property test: adjust_for_score idempotence (Property 8)
    - **Property 8: adjust_for_score is idempotent within a cycle**
    - Generate random soft_limit and score values, verify calling `adjust_for_score` twice produces same result as once
    - **Validates: Requirements 16.1, 16.2**

- [ ] 8. Checkpoint — Ensure all tests pass after bug fixes
  - Run `cmake --build --preset=dev && ctest --preset=dev --output-on-failure`
  - Ensure all existing tests pass, ask the user if questions arise.

- [ ] 9. Protocol correctness: UCI ponder, thread safety, TT resize wiring, castling fix
  - [ ] 9.1 Wire TT resize to UCI `setoption name Hash value N` and Xboard `memory N` commands
    - Add `TranspositionTable::resize(int size_mb)` method that reallocates storage and clears entries
    - Wire UCI `setoption` handler to call `resize()`
    - Wire Xboard `memory` handler to call `resize()`
    - _Requirements: 11.1, 11.2, 11.3, 11.4_

  - [ ]* 9.2 Write property tests: TT resize allocates ~N MB (Property 2) and TT resize clears entries (Property 3)
    - **Property 2: TT resize allocates approximately N megabytes**
    - Generate random N in [1, 512], verify storage is approximately N MB
    - **Property 3: TT resize clears all entries**
    - Record random entries, resize, verify all probes miss
    - **Validates: Requirements 11.1, 11.2, 11.4**

  - [ ] 9.3 Implement UCI `go ponder` and `ponderhit` support
    - Parse `ponder` flag in `cmd_go`
    - Add `cmd_ponderhit()` handler that switches from ponder to normal search
    - Ensure `cmd_stop()` aborts ponder search and returns best move
    - _Requirements: 9.1, 9.2, 9.3_

  - [ ] 9.4 Add UCI thread safety — protect `board_` access with `std::mutex` when search thread is running
    - Add `std::mutex board_mutex_` to UCI class
    - Use `std::lock_guard` in `cmd_position`, `cmd_go`, `cmd_stop`
    - Verify no deadlock in rapid `position`/`go`/`stop`/`quit` sequences
    - _Requirements: 10.1, 10.2, 10.3_

  - [ ] 9.5 Fix `move_to_uci()` castling notation for ponder moves — determine castling side from the move itself, not `board_.side_to_move()`
    - Extract piece color from the move encoding
    - Produce correct UCI notation (`e1g1`, `e1c1`, `e8g8`, `e8c8`) regardless of board side
    - _Requirements: 18.1, 18.2_

  - [ ]* 9.6 Write property test: Castling move UCI notation correctness (Property 10)
    - **Property 10: Castling move UCI notation correctness**
    - Test all 4 castling types with both side_to_move values, verify correct UCI string
    - **Validates: Requirements 18.2**

- [ ] 10. Timing modernization: Replace clock() with std::chrono::steady_clock
  - [ ] 10.1 Update TimeManager to use `std::chrono::steady_clock` — replace `clock_t` with `steady_clock::time_point`, update `is_time_over()` and `should_stop()`
    - Replace all `clock()` calls with `steady_clock::now()`
    - Convert time comparisons from clock ticks to chrono durations
    - _Requirements: 19.1, 19.3_

  - [ ] 10.2 Update SearchStats to use `std::chrono::steady_clock` — replace `clock_t start_time` with `steady_clock::time_point`, add `elapsed_secs()` method
    - Update `reset()` to use `steady_clock::now()`
    - Update NPS calculation and time reporting
    - _Requirements: 19.2, 19.3_

- [ ] 11. Checkpoint — Ensure all tests pass after protocol and timing changes
  - Run `cmake --build --preset=dev && ctest --preset=dev --output-on-failure`
  - Ensure all existing tests pass, ask the user if questions arise.

- [ ] 12. Tooling: analyze-sts.py --output flag
  - [ ] 12.1 Add `--output` flag to `scripts/analyze-sts.py` — support JSON and CSV export based on file extension
    - Add `argparse` `--output` argument
    - Implement JSON writer (per-category scores, total score, total percentage, estimated Elo)
    - Implement CSV writer with same fields
    - Default to stdout when `--output` is omitted
    - _Requirements: 23.1, 23.2, 23.3_

  - [ ]* 12.2 Write property test: STS output contains all required fields (Property 12)
    - **Property 12: STS output contains all required fields**
    - Generate random category scores and totals with Hypothesis, verify output contains all required fields
    - **Validates: Requirements 23.3**

- [ ] 13. Final checkpoint — Ensure all tests pass
  - Run `cmake --build --preset=dev && ctest --preset=dev --output-on-failure`
  - Ensure all existing tests pass, ask the user if questions arise.
  - Verify no compiler warnings with `-Wall -Wextra`
  - Review against pre-commit checklist in `.kiro/steering/commit-checklist.md`

## Notes

- Tasks marked with `*` are optional and can be skipped for faster MVP
- Each task references specific requirements for traceability
- Checkpoints ensure incremental validation between phases
- Property tests validate universal correctness properties from the design document
- All changes are strength-neutral — no search or evaluation algorithm changes
- Build: `cmake --build --preset=dev` / Test: `ctest --preset=dev --output-on-failure`
