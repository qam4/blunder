# Implementation Plan: Tapered Evaluation Port

## Overview

Port Stockfish-style tapered evaluation from the `eval` branch into `HandCraftedEvaluator`. Changes are confined to `Evaluator.h`, `Evaluator.cpp`, `UCI.cpp`, `UCI.h`, `Board.h`, and a new test file. The implementation proceeds bottom-up: static data → PSQT init → phase calculation → tapered eval → optional terms → UCI wiring → tests.

## Tasks

- [x] 1. Add EvalConfig struct and expand HandCraftedEvaluator declaration
  - [x] 1.1 Add `EvalConfig` struct to `source/Evaluator.h` with `mobility_enabled` and `tempo_enabled` bools defaulting to `true`
    - _Requirements: 8.1, 8.2, 8.3, 8.6_
  - [x] 1.2 Add private members to `HandCraftedEvaluator`: `piece_square_table_[2][14][64]`, `EvalConfig config_`, private methods `psqt_init()` and `phase(const Board&) const`
    - Add public `set_config(const EvalConfig&)` and `config() const` accessors
    - Add constructor declaration `HandCraftedEvaluator()`
    - _Requirements: 7.2, 8.1, 8.5_
  - [x] 1.3 Add `Board::get_hce()` accessor returning `HandCraftedEvaluator&` to `source/Board.h`
    - _Requirements: 10.3, 10.4_

- [x] 2. Implement static data arrays and PSQT initialization in Evaluator.cpp
  - [x] 2.1 Define phase constants (`MIDGAME_LIMIT`, `ENDGAME_LIMIT`, `PHASE_MAX`, `MG`, `EG`, `NUM_PHASES`) and `PIECE_VALUE_BONUS[2][7]` with Stockfish-tuned material values as file-scope constants
    - Remove the old `PIECE_SQUARE` table
    - Values: Pawn (124/206), Knight (781/854), Bishop (825/915), Rook (1276/1380), Queen (2538/2682)
    - _Requirements: 1.1, 1.2, 1.3, 3.5_
  - [x] 2.2 Define `PIECE_SQUARE_BONUS_PAWN[2][64]` (full 64-square per phase) and `PIECE_SQUARE_BONUS[2][5][8][4]` (compressed file-symmetric for Knight, Bishop, Rook, Queen, King) using Stockfish-tuned values from the eval branch
    - _Requirements: 2.1, 2.2_
  - [x] 2.3 Implement `HandCraftedEvaluator::psqt_init()` that expands compressed PSQT data into `piece_square_table_[2][14][64]`, combining material + positional bonus, handling file symmetry mirroring and vertical flip for black pieces
    - _Requirements: 2.3, 2.4, 2.5, 2.6, 7.1, 7.3_
  - [x] 2.4 Implement `HandCraftedEvaluator::HandCraftedEvaluator()` constructor that calls `psqt_init()` and initializes `config_` with defaults
    - _Requirements: 7.1, 8.6_
  - [ ]* 2.5 Write property test: PSQT file symmetry (Property 1)
    - **Property 1: File symmetry in expanded PSQT**
    - Generate random (phase, non-pawn piece, rank) triples, verify `piece_square_table_[phase][piece][rank*8+file] == piece_square_table_[phase][piece][rank*8+(7-file)]`
    - **Validates: Requirements 2.4**
  - [ ]* 2.6 Write property test: Black/white vertical flip (Property 2)
    - **Property 2: Black/white vertical flip in expanded PSQT**
    - Generate random (phase, piece_type, square) triples, verify `piece_square_table_[phase][black_piece][sq] == piece_square_table_[phase][white_piece][sq ^ 56]`
    - **Validates: Requirements 2.5**
  - [ ]* 2.7 Write property test: Expanded PSQT contains material (Property 8)
    - **Property 8: Expanded PSQT contains material plus positional bonus**
    - For all (phase, piece_type, square) with nonzero material, verify table value is nonzero
    - **Validates: Requirements 7.3, 2.6**

- [x] 3. Implement game phase calculation and tapered evaluation
  - [x] 3.1 Implement `HandCraftedEvaluator::phase(const Board&) const` that computes game phase from non-pawn material using `PIECE_VALUE_BONUS[MG]`, clamped to [0, 128]
    - _Requirements: 3.1, 3.2, 3.3, 3.4, 3.5_
  - [x] 3.2 Rewrite `HandCraftedEvaluator::evaluate()` to sum expanded PSQT for all pieces (white positive, black negative) into separate MG/EG scores, compute phase, blend with `(mg * phase + eg * (128 - phase)) / 128`, add optional mobility and tempo terms, return score from white's perspective
    - Use `MoveGenerator::add_all_moves()` for mobility when `config_.mobility_enabled`
    - Add `±28` tempo bonus when `config_.tempo_enabled`
    - _Requirements: 4.1, 4.2, 4.3, 5.1, 5.2, 5.3, 6.1, 6.2_
  - [x] 3.3 Verify `side_relative_eval()` still works correctly (existing implementation should be fine since it delegates to `evaluate()`)
    - _Requirements: 10.1, 10.2_
  - [ ]* 3.4 Write property test: Game phase clamping and formula (Property 3)
    - **Property 3: Game phase clamping and formula**
    - Generate random piece configurations, verify phase is in [0, 128] and matches the formula
    - **Validates: Requirements 3.2, 3.3, 3.4, 3.5**
  - [ ]* 3.5 Write property test: Tapered blending at phase extremes (Property 5)
    - **Property 5: Tapered blending at phase extremes**
    - Verify starting position (phase=128) yields pure MG score, K-vs-K (phase=0) yields pure EG score, with mobility/tempo disabled
    - **Validates: Requirements 4.2**
  - [ ]* 3.6 Write property test: Evaluation color symmetry (Property 4)
    - **Property 4: Evaluation color symmetry**
    - For diverse FEN positions, mirror each position and verify `evaluate(pos) == -evaluate(mirror(pos))` with mobility/tempo disabled
    - **Validates: Requirements 4.1, 4.3**

- [x] 4. Checkpoint — Ensure all tests pass
  - Ensure all tests pass, ask the user if questions arise.

- [x] 5. Wire UCI setoption for Mobility and Tempo
  - [x] 5.1 In `UCI::cmd_uci()`, advertise `option name Mobility type check default true` and `option name Tempo type check default true`
    - _Requirements: 8.5_
  - [x] 5.2 In `UCI::cmd_setoption()`, handle `Mobility` and `Tempo` option names by updating the `HandCraftedEvaluator` config via `board_.get_hce().set_config()`
    - _Requirements: 8.2, 8.3, 8.4, 8.5_
  - [ ]* 5.3 Write property test: Mobility contribution (Property 6)
    - **Property 6: Mobility contribution**
    - For diverse positions, verify difference between eval with/without mobility equals `20 * (white_moves - black_moves)`
    - **Validates: Requirements 5.2**
  - [ ]* 5.4 Write property test: Tempo contribution (Property 7)
    - **Property 7: Tempo contribution**
    - For diverse positions, verify difference between eval with/without tempo is ±28 based on side to move
    - **Validates: Requirements 6.1**
  - [ ]* 5.5 Write property test: side_relative_eval semantics (Property 9)
    - **Property 9: side_relative_eval semantics**
    - For diverse positions, verify `side_relative_eval == evaluate` when white to move, `side_relative_eval == -evaluate` when black to move
    - **Validates: Requirements 10.2**

- [x] 6. Add test file to build and write unit tests
  - [x] 6.1 Create `test/source/TestTaperedEval.cpp` and add it to `test/CMakeLists.txt`
    - _Requirements: 9.1_
  - [ ]* 6.2 Write unit tests: verify exact Stockfish material values in `PIECE_VALUE_BONUS`, default config has all terms enabled, phase=128 for starting position, phase=0 for K-vs-K, known eval for starting position
    - _Requirements: 1.2, 3.3, 3.4, 8.6_

- [x] 7. Final checkpoint — Ensure all tests pass
  - Ensure all tests pass, ask the user if questions arise.

## Notes

- Tasks marked with `*` are optional and can be skipped for faster MVP
- Each task references specific requirements for traceability
- Property tests validate universal correctness properties from the design document
- The eval branch diff (`git diff main..eval -- source/Evaluation.cpp`) should be referenced for exact PSQT data values during implementation of task 2.2
- Build: `cmake --build --preset=dev` | Test: `ctest --preset=dev --output-on-failure`
- Strength validation (Requirement 9) is a manual benchmarking step, not a coding task — run `scripts/run-benchmarks.py` after implementation
