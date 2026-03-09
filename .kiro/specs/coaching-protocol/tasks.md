# Implementation Plan: Coaching Protocol (Engine Side)

## Overview

Implement the Coaching Protocol in Blunder by adding four new components (CoachJson, PositionAnalyzer, MoveComparator, CoachDispatcher) and wiring them into the existing UCI command loop. Each task builds incrementally — JSON utilities first, then analysis components, then the dispatcher, and finally integration and end-to-end tests.

## Tasks

- [x] 1. Implement CoachJson serialization utilities
  - [x] 1.1 Create `source/CoachJson.h` and `source/CoachJson.cpp` with primitive serializers
    - Implement `to_json(int)`, `to_json(bool)`, `to_json(const std::string&)` with proper escaping of `"`, `\`, and control characters
    - Implement `to_json_null()`, `array()`, and `object()` builders
    - All output must be compact JSON (no pretty-printing, no extra whitespace)
    - _Requirements: 17.1, 17.2_

  - [x] 1.2 Add high-level serializers to CoachJson
    - Implement `serialize_pong(engine_name, engine_version)`
    - Implement `serialize_error(code, message)`
    - Implement `serialize_position_report(const PositionReport&)` — serialize all fields from the PositionReport struct including eval_breakdown, hanging_pieces, threats, pawn_structure, king_safety, top_lines, tactics, threat_map, critical_moment, critical_reason
    - Implement `serialize_comparison_report(const ComparisonReport&)` — serialize all fields including user_move, best_move, eval_drop_cp, classification, nag, best_move_idea, refutation_line, missed_tactics, top_lines, critical_moment, critical_reason
    - _Requirements: 17.1, 17.2, 2.3_

  - [ ]* 1.3 Write property tests for CoachJson (TestCoachJson.cpp)
    - Create `test/source/TestCoachJson.cpp`
    - **Property 13: JSON round-trip** — For all response types, parse and re-serialize in compact form, verify byte-identical output
    - **Validates: Requirements 17.1, 17.2, 17.3**

- [x] 2. Implement PositionAnalyzer — board analysis components
  - [x] 2.1 Create `source/PositionAnalyzer.h` with all data structs and class declaration
    - Define EvalBreakdown, HangingPiece, Threat, PawnFeatures, KingSafety, ThreatMapEntry, Tactic, PositionReport structs
    - Declare the PositionAnalyzer class with all static methods: analyze(), compute_eval_breakdown(), find_hanging_pieces(), find_threats(), analyze_pawns(), assess_king_safety(), build_threat_map(), detect_tactics(), is_critical_moment(), label_line_theme()
    - _Requirements: 4.3, 4.4, 5.1, 6.1, 7.1, 8.1, 9.1, 10.1, 11.1, 12.1_

  - [x] 2.2 Implement `source/PositionAnalyzer.cpp` — eval breakdown and pawn structure
    - Implement `compute_eval_breakdown()` — decompose evaluation into material, mobility, king_safety, pawn_structure components using HandCraftedEvaluator logic
    - Implement `analyze_pawns()` — detect isolated, doubled, and passed pawns using bitboard operations on pawn bitboards
    - _Requirements: 5.1, 5.2, 8.1, 8.2, 8.3_

  - [x] 2.3 Implement hanging pieces and threat map in PositionAnalyzer
    - Implement `find_hanging_pieces()` — use MoveGenerator::attacks_to() to count attackers/defenders per occupied square, report pieces attacked more than defended (exclude kings)
    - Implement `build_threat_map()` — iterate all 64 squares, compute white_attackers, black_attackers, white_defenders, black_defenders, net_attacked for squares that are attacked or contain pieces
    - _Requirements: 6.1, 6.2, 6.3, 10.1, 10.2, 10.3_

  - [x] 2.4 Implement threats, tactics, king safety, and critical moment detection
    - Implement `find_threats()` — detect check, capture, fork, pin, skewer, discovered_attack threats using attack bitboards and sliding piece logic
    - Implement `assess_king_safety()` — evaluate pawn shield, open files near king, generate score and description
    - Implement `detect_tactics()` — scan for forks, pins, skewers, discovered attacks, back-rank threats, overloaded pieces both on-board and in PV lines
    - Implement `is_critical_moment()` — compare best vs 3rd-best PV line eval spread against 100cp threshold
    - Implement `label_line_theme()` — heuristic labeling based on first few PV moves
    - _Requirements: 7.1, 7.2, 7.3, 9.1, 9.2, 11.1, 11.2, 11.3, 12.1, 12.2, 12.3_

  - [x] 2.5 Implement the top-level `analyze()` method
    - Wire all individual analysis methods into `PositionAnalyzer::analyze()` which takes a `const Board&` and `const std::vector<PVLine>&` and returns a complete PositionReport
    - _Requirements: 4.3, 4.4_

  - [ ]* 2.6 Write property tests for PositionAnalyzer (TestPositionAnalyzer.cpp)
    - Create `test/source/TestPositionAnalyzer.cpp`
    - **Property 4: Eval breakdown approximates total eval** — For diverse positions, verify sum of breakdown components is within 50cp of eval_cp
    - **Validates: Requirements 5.1, 5.2**

  - [ ]* 2.7 Write property tests for hanging pieces and pawn structure
    - **Property 5: Hanging pieces correctness** — For diverse positions, verify hanging pieces match independent attacks_to() computation
    - **Validates: Requirements 6.1, 6.2**
    - **Property 6: Pawn structure correctness** — For diverse positions, verify isolated/doubled/passed detection against bitboard definitions
    - **Validates: Requirements 8.1, 8.2**

  - [ ]* 2.8 Write property tests for threat map and critical moment
    - **Property 7: Threat map attacker/defender counts** — For diverse positions, verify counts match attacks_to()
    - **Validates: Requirements 10.1, 10.2, 10.3**
    - **Property 8: Critical moment detection** — Generate PV line sets with known spreads, verify critical_moment flag
    - **Validates: Requirements 12.1, 12.2, 12.3**
    - **Property 14: MultiPV count in eval** — For diverse positions and N values, verify top_lines count equals min(N, legal_moves)
    - **Validates: Requirements 4.2**
    - **Property 15: Position report field completeness** — For diverse positions, verify all required fields present
    - **Validates: Requirements 4.3, 4.4, 5.1, 7.2, 8.2, 9.1, 10.2, 11.2**

- [x] 3. Checkpoint — Ensure all tests pass
  - Ensure all tests pass, ask the user if questions arise.

- [x] 4. Implement MoveComparator
  - [x] 4.1 Create `source/MoveComparator.h` and `source/MoveComparator.cpp` — classification helpers
    - Define ComparisonReport struct with all fields (fen, user_move, user_eval_cp, best_move, best_eval_cp, eval_drop_cp, classification, nag, best_move_idea, refutation_line, missed_tactics, top_lines, critical_moment, critical_reason)
    - Implement `classify(eval_drop_cp)` — good (<=30), inaccuracy (31-100), mistake (101-300), blunder (>300)
    - Implement `compute_nag(eval_drop_cp, is_best_move)` — `!` (<=10), `!?` (11-30), `?!` (31-100), `?` (101-300), `??` (>300); special case for best move
    - Implement `describe_best_move()` — heuristic description based on move characteristics
    - _Requirements: 13.2, 13.3, 13.4, 13.5, 13.6_

  - [x] 4.2 Implement `MoveComparator::compare()` — full comparison flow
    - Run MultiPV search to get top lines and best move
    - If user_move == best_move, set eval_drop_cp = 0
    - Otherwise, apply user's move to board copy, search resulting position, negate score, compute eval_drop_cp = best_eval_cp - user_eval_cp
    - Classify and assign NAG
    - If blunder, extract refutation_line from search after user's move
    - Detect missed_tactics by comparing tactics in best line vs after user's move
    - Populate top_lines, critical_moment, critical_reason from MultiPV results
    - _Requirements: 13.1, 13.2, 13.3, 13.4, 13.5, 13.6, 14.1, 14.2, 14.3, 14.4, 14.5_

  - [ ]* 4.3 Write property tests for MoveComparator (TestMoveComparator.cpp)
    - Create `test/source/TestMoveComparator.cpp`
    - **Property 9: Classification and NAG thresholds** — Generate random eval_drop_cp values 0-1000, verify classification and NAG match threshold tables
    - **Validates: Requirements 13.3, 13.4**
    - **Property 10: Eval drop non-negativity** — For diverse positions and moves, verify eval_drop_cp >= 0 and equals best_eval_cp - user_eval_cp
    - **Validates: Requirements 13.2**
    - **Property 11: Refutation line presence** — For diverse comparison reports, verify refutation_line is non-null/non-empty iff classification is blunder
    - **Validates: Requirements 14.1, 14.2**

- [x] 5. Checkpoint — Ensure all tests pass
  - Ensure all tests pass, ask the user if questions arise.

- [x] 6. Implement CoachDispatcher and UCI integration
  - [x] 6.1 Create `source/CoachDispatcher.h` and `source/CoachDispatcher.cpp`
    - Implement constructor taking `Board&`, `Search&`, `NNUEEvaluator*`
    - Implement `dispatch(args)` — parse subcommand token, route to cmd_ping/cmd_eval/cmd_compare, or send unknown_command error
    - Implement `send_response(json)` — write `BEGIN_COACH_RESPONSE`, JSON line, `END_COACH_RESPONSE` to stdout with no leading/trailing whitespace on markers
    - Implement `send_error(code, message)` — build error envelope and call send_response
    - Implement `wrap_envelope(type, data_json)` — build JSON with protocol, version, type, data fields
    - Wrap all command handling in try-catch for internal_error safety
    - _Requirements: 1.1, 1.3, 2.1, 2.2, 2.3, 15.3_

  - [x] 6.2 Implement `cmd_ping()` in CoachDispatcher
    - Build pong response with status "ok", engine_name "Blunder", engine_version from build version
    - Wrap in envelope with type "pong" and send
    - _Requirements: 3.1, 3.2_

  - [x] 6.3 Implement `cmd_eval(args)` in CoachDispatcher
    - Parse FEN string and optional multipv parameter (default 3)
    - Validate FEN using existing parser, send invalid_fen error on failure
    - Set up board position from FEN
    - Run MultiPV search with specified line count
    - Call PositionAnalyzer::analyze() with board and PV lines
    - Serialize PositionReport to JSON, wrap in envelope with type "position_report", send
    - _Requirements: 4.1, 4.2, 4.3, 4.4, 15.1_

  - [x] 6.4 Implement `cmd_compare(args)` in CoachDispatcher
    - Parse FEN string and move parameter
    - Validate FEN, send invalid_fen error on failure
    - Validate move legality, send invalid_move error on failure
    - Call MoveComparator::compare() with board, search, and user move
    - Serialize ComparisonReport to JSON, wrap in envelope with type "comparison_report", send
    - _Requirements: 13.1, 14.1, 14.2, 14.3, 14.4, 14.5, 15.1, 15.2_

  - [x] 6.5 Wire CoachDispatcher into UCI
    - Add `#include "CoachDispatcher.h"` to `source/UCI.h`
    - Add `CoachDispatcher coach_dispatcher_` member to UCI class
    - Initialize coach_dispatcher_ in UCI constructor with board_, search_, nnue_
    - Add `handlers_["coach"]` entry in `init_handlers()` that delegates to `coach_dispatcher_.dispatch(args)`
    - _Requirements: 1.1, 1.2, 16.1, 16.2, 16.3_

  - [x] 6.6 Update CMakeLists.txt build files
    - Add `source/CoachDispatcher.cpp`, `source/PositionAnalyzer.cpp`, `source/MoveComparator.cpp`, `source/CoachJson.cpp` to `blunder_lib` in root `CMakeLists.txt`
    - Add `test/source/TestCoachProtocol.cpp`, `test/source/TestPositionAnalyzer.cpp`, `test/source/TestMoveComparator.cpp`, `test/source/TestCoachJson.cpp` to `blunder_test` in `test/CMakeLists.txt`
    - _Requirements: 16.1_

  - [ ]* 6.7 Write protocol-level property tests (TestCoachProtocol.cpp)
    - Create `test/source/TestCoachProtocol.cpp`
    - **Property 1: Response framing and envelope structure** — For each coaching command type, verify BEGIN/END markers, single-line valid JSON, envelope fields (protocol, version, type, data)
    - **Validates: Requirements 2.1, 2.2, 2.3**
    - **Property 2: Command routing correctness** — For known subcommands verify correct response type; for unknown subcommands verify error with unknown_command code
    - **Validates: Requirements 1.1, 1.3**
    - **Property 12: Error responses for invalid input** — Generate invalid FEN strings and illegal moves, verify error codes invalid_fen and invalid_move
    - **Validates: Requirements 15.1, 15.2**

  - [ ]* 6.8 Write UCI coexistence property test
    - **Property 3: UCI non-interference** — Interleave coaching and UCI commands, verify UCI commands produce identical output with and without coaching commands present
    - **Validates: Requirements 1.2, 16.1**

- [x] 7. Final checkpoint — Ensure all tests pass
  - Ensure all tests pass, ask the user if questions arise.

## Notes

- Tasks marked with `*` are optional and can be skipped for faster MVP
- The `coach eval` command depends on MultiPV search from the `uci-multipv` spec — ensure that spec is implemented first
- All eval_cp values use side-to-move perspective, consistent with existing `side_relative_eval()` convention
- CoachJson uses a simple string-builder approach — no external JSON library needed
- Property tests use Catch2 GENERATE with curated FEN positions covering opening, middlegame, endgame, and tactical positions
- Each property test references its design document property number and the requirements it validates
