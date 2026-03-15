# Implementation Plan: Coaching Improvements

## Overview

Six targeted fixes to the coaching protocol across four source files (`PositionAnalyzer.h`, `PositionAnalyzer.cpp`, `CoachDispatcher.cpp`, `CoachJson.cpp`). Changes are additive — new struct fields, rewritten analysis functions, and updated serialization. Build: `cmake --build --preset=dev`, test: `ctest --preset=dev --output-on-failure`.

## Tasks

- [x] 1. Add new struct fields to PositionAnalyzer.h
  - [x] 1.1 Add `tempo` and `piece_bonuses` fields to `EvalBreakdown` struct
    - Add `int tempo;` and `int piece_bonuses;` after `pawn_structure`
    - _Requirements: 1.1_
  - [x] 1.2 Add `uci_move` field to `Threat` struct
    - Add `std::string uci_move;` between `target_squares` and `description`
    - _Requirements: 3.3_
  - [x] 1.3 Add `threat_map_summary` field to `PositionReport` struct
    - Add `std::string threat_map_summary;` after `threat_map`
    - _Requirements: 6.2_

- [x] 2. Complete eval breakdown with tempo and piece bonuses (PositionAnalyzer.cpp)
  - [x] 2.1 Rewrite `compute_eval_breakdown()` to populate `tempo` and `piece_bonuses`
    - Read `piece_bonuses` from `hce.get_piece_bonuses_score()`
    - Compute `tempo` as +28 for white-to-move, -28 for black-to-move (white-relative, before side conversion)
    - Compute `material` as `total - pawn_structure - king_safety - mobility - piece_bonuses - tempo`
    - Negate all six fields when side-to-move is BLACK
    - _Requirements: 1.1, 1.2, 1.3, 1.5_
  - [ ]* 2.2 Write property test: eval breakdown sum invariant
    - **Property 1: Eval breakdown sum invariant**
    - Generate random valid boards, verify `material + mobility + king_safety + pawn_structure + tempo + piece_bonuses` equals `side_relative_eval()` within ±2cp, and `tempo` is ±28
    - **Validates: Requirements 1.2, 1.3, 1.5**

- [x] 3. Fix coaching MultiPV population (CoachDispatcher.cpp)
  - [x] 3.1 Filter empty PV lines in `cmd_eval()` before passing to `PositionAnalyzer::analyze()`
    - After `search_.get_multipv_results()`, copy only entries where `!line.moves.empty()` into a new vector
    - Pass the filtered vector to `PositionAnalyzer::analyze()` instead of the raw results
    - _Requirements: 2.1, 2.3, 2.4_
  - [ ]* 3.2 Write property test: MultiPV filtering removes empty lines
    - **Property 3: MultiPV filtering removes empty lines**
    - Generate random vectors of PVLine (some empty, some not), filter, verify all remaining have non-empty moves and no valid entries lost
    - **Validates: Requirements 2.1, 2.3, 2.4**

- [x] 4. Checkpoint
  - Ensure all tests pass (`cmake --build --preset=dev && ctest --preset=dev --output-on-failure`), ask the user if questions arise.

- [x] 5. Enrich threat descriptions with UCI moves (PositionAnalyzer.cpp)
  - [x] 5.1 Add `uci_from_squares()` helper and update `find_threats()` for check and capture threats
    - Add static helper `uci_from_squares(U8 from, U8 to)` returning 4-char UCI string
    - For check threats: pick first valid check-move square from `check_moves`, set `uci_move`, set `target_squares` to the actual destination square (not king square), update description to include `"via {uci_move}"`
    - For capture threats: set `uci_move = uci_from_squares(atk_sq, target_sq)`, update description to include `"via {uci_move}"`
    - For fork/pin/skewer threats: set `uci_move` to empty string (positional, not single-move)
    - _Requirements: 3.1, 3.2, 3.3_
  - [ ]* 5.2 Write property test: check and capture threats include UCI move
    - **Property 4: Check and capture threats include UCI move notation**
    - Generate random boards, call `find_threats()`, verify every check/capture threat has non-empty `uci_move` of 4-5 chars with first two chars encoding `source_square`
    - **Validates: Requirements 3.1, 3.2**

- [x] 6. Implement discovered attack detection (PositionAnalyzer.cpp)
  - [x] 6.1 Add on-board discovered attack detection to `detect_tactics()`
    - After existing pin/skewer block, iterate own sliders; for each ray, find own non-king blocker with opponent higher-value piece behind it
    - Remove blocker from occupied, recompute slider attacks, verify slider now attacks the target
    - Check blocker has at least one move off the line
    - Report as `Tactic{type: "discovered_attack", in_pv: false}`
    - _Requirements: 4.5, 4.6_
  - [x] 6.2 Add PV-line discovered attack detection to `detect_tactics()`
    - In the PV walk loop, after each `do_move()`, check if the moved piece was blocking a slider ray
    - If moving it reveals an attack from a same-side slider onto a higher-value opponent piece, report with `in_pv: true`
    - Limit to first 4 half-moves per PV line
    - _Requirements: 4.4, 4.6_

- [x] 7. Rewrite position-aware king safety (PositionAnalyzer.cpp)
  - [x] 7.1 Rewrite `assess_king_safety()` description generation
    - Determine castling status: g1/g8 → "kingside castled", c1/c8 → "queenside castled", e1/e8 with rights → "king uncastled, still has castling rights", e1/e8 without rights → "king stuck in center, castling rights lost", elsewhere → "king displaced to {square}"
    - Use `board.castling_rights()` bitmask (0x1=WK, 0x2=WQ, 0x4=BK, 0x8=BQ)
    - Detect pawn storm: enemy pawns on rank 4+ (for white king) or rank 5- (for black king) on adjacent files
    - Keep existing open file detection and score computation
    - Build description by concatenating: castling status + pawn shield status + pawn storm clause + open file clause
    - When shield intact and no threats: e.g. "kingside castled, solid pawn shield"
    - _Requirements: 5.1, 5.2, 5.3, 5.4, 5.5, 5.6_
  - [ ]* 7.2 Write property test: king safety description reflects castling status
    - **Property 5: King safety description reflects castling status**
    - Generate boards with king on g1/g8 → description contains "kingside castled"; c1/c8 → "queenside castled"; e1/e8 with rights → "uncastled"; e1/e8 without rights → "castling rights lost"
    - **Validates: Requirements 5.1, 5.2, 5.3**
  - [ ]* 7.3 Write property test: king safety description reflects pawn storm and open files
    - **Property 6: King safety description reflects pawn storm and open files**
    - Generate boards with enemy pawns advanced near king → description contains "pawn storm"; open file near king → "open file"; intact shield → "safe" or "solid"
    - **Validates: Requirements 5.4, 5.5, 5.6**

- [x] 8. Checkpoint
  - Ensure all tests pass (`cmake --build --preset=dev && ctest --preset=dev --output-on-failure`), ask the user if questions arise.

- [x] 9. Filter threat map and add summary (PositionAnalyzer.cpp)
  - [x] 9.1 Rewrite `build_threat_map()` to filter and cap at 16 entries
    - Compute all 64 squares as before, but only include squares where: (a) `net_attacked` is true, (b) occupied and opponent attackers > own defenders, or (c) key central square (d4/d5/e4/e5) with ≥1 attacker
    - Sort by priority: occupied net-attacked pieces (by piece value descending) first, then contested central squares
    - Truncate to 16 entries max
    - _Requirements: 6.1, 6.4, 6.5_
  - [x] 9.2 Generate `threat_map_summary` string in `build_threat_map()` or `analyze()`
    - Build plain-English summary from top filtered entries (e.g. "Nf3 is undefended and attacked; d5 is contested by both sides")
    - When no squares qualify: set summary to `"no significant threats on the board"` and threat_map to empty
    - Store in `report.threat_map_summary`
    - _Requirements: 6.2, 6.3_
  - [ ]* 9.3 Write property test: threat map filtering invariant
    - **Property 7: Threat map filtering invariant**
    - Generate random boards, call `build_threat_map()`, verify every entry meets at least one inclusion criterion
    - **Validates: Requirements 6.1**
  - [ ]* 9.4 Write property test: threat map size cap
    - **Property 8: Threat map size cap**
    - Generate random boards, verify `build_threat_map()` returns ≤16 entries
    - **Validates: Requirements 6.4**

- [x] 10. Update CoachJson serialization (CoachJson.cpp)
  - [x] 10.1 Add `tempo` and `piece_bonuses` to eval_breakdown serialization
    - In `serialize_position_report()`, add `{"tempo", to_json(r.breakdown.tempo)}` and `{"piece_bonuses", to_json(r.breakdown.piece_bonuses)}` to the breakdown object
    - _Requirements: 1.4_
  - [x] 10.2 Add `uci_move` field to threat serialization
    - In `serialize_position_report()` threat serialization lambda, add `{"uci_move", to_json(t.uci_move)}` to each threat object
    - _Requirements: 3.4_
  - [x] 10.3 Add `threat_map_summary` to position report serialization
    - Add `{"threat_map_summary", to_json(r.threat_map_summary)}` to the top-level position report object
    - _Requirements: 6.2_
  - [ ]* 10.4 Write property test: JSON serialization includes all new fields
    - **Property 2: JSON serialization includes all new fields**
    - Generate random PositionReport structs, serialize via `serialize_position_report()`, verify JSON contains keys `"tempo"`, `"piece_bonuses"`, `"uci_move"`, `"threat_map_summary"`
    - **Validates: Requirements 1.4, 3.4, 6.2**

- [x] 11. Final checkpoint
  - Ensure all tests pass (`cmake --build --preset=dev && ctest --preset=dev --output-on-failure`), ask the user if questions arise.

## Notes

- Tasks marked with `*` are optional and can be skipped for faster MVP
- Each task references specific requirements for traceability
- Checkpoints ensure incremental validation
- Property tests use rapidcheck with random board generators (minimum 100 iterations)
- All tests go in `test/source/TestCoachingImprovements.cpp`
- Build: `cmake --build --preset=dev` / Test: `ctest --preset=dev --output-on-failure`
