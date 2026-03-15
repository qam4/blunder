# Implementation Plan: HCE GM-Strength

## Overview

Raise Blunder's HCE-mode from ~2212 Elo to 2500+ Elo through four phases: search improvements, HCE evaluation enhancements, TT upgrades, and miscellaneous fixes. All changes preserve the coaching protocol's `EvalBreakdown` sub-scores. Implementation language: C++.

## Tasks

- [x] 1. Phase 1 — Search Improvements
  - [x] 1.1 Implement check extensions in `alphabeta()`
    - In `Search::alphabeta()` (Search.cpp ~line 500), after computing `in_check`, add a `depth_extension` variable: if `in_check`, set `extension = 1`, else `0`
    - Apply extension to all recursive `alphabeta()` calls: use `depth - 1 + extension` instead of `depth - 1`
    - Do NOT apply check extensions in `quiesce()` (it already handles captures after checks)
    - _Requirements: 1.1, 1.2, 1.3, 1.4_

  - [x] 1.2 Implement reverse futility pruning in `alphabeta()`
    - Before the move loop in `alphabeta()`, after null move pruning and after computing `in_check`
    - Add: if `depth >= 1 && depth <= 3 && !in_check && !is_pv`, compute `static_eval` via `board_.get_evaluator().side_relative_eval(board_)`
    - If `static_eval - depth * 120 >= beta`, return `beta` immediately
    - Define `constexpr int RFP_MARGIN_PER_DEPTH = 120` in Search.cpp
    - _Requirements: 3.1, 3.2, 3.3_

  - [x] 1.3 Implement futility pruning in `alphabeta()`
    - Reuse the `static_eval` computed in 1.2 (compute it if not already done for RFP)
    - Define `constexpr int FUTILITY_MARGIN[3] = { 0, 200, 500 }` in Search.cpp
    - Before each move in the move loop: if `depth <= 2 && !in_check && !is_pv && is_quiet && static_eval + FUTILITY_MARGIN[depth] <= alpha`, skip the move
    - Still search captures, promotions, and moves that give check (check detection: call `MoveGenerator::in_check()` after `do_move`)
    - _Requirements: 2.1, 2.2, 2.3, 2.4, 2.5_

  - [x] 1.4 Implement SEE pruning in quiescence search
    - In `Search::quiesce()` (Search.cpp ~line 700), before `board_.do_move(move)` for each capture
    - Call `MoveGenerator::see(board_, move)` and if the result is `< 0`, skip the move with `continue`
    - This avoids the cost of making/unmaking losing captures
    - _Requirements: 4.1, 4.2, 4.3_

  - [x] 1.5 Implement late move pruning in `alphabeta()`
    - Add a `quiet_moves_searched` counter in the move loop, incremented for each quiet move searched
    - Define `constexpr int lmp_threshold(int depth) { return 3 + depth * 4; }` in Search.cpp
    - If `depth >= 1 && depth <= 3 && !in_check && !is_pv && is_quiet && !is_killer_move && quiet_moves_searched > lmp_threshold(depth)`, skip the move
    - _Requirements: 5.1, 5.2, 5.3, 5.4_

  - [x] 1.6 Implement logarithmic LMR with lookup table
    - Add `static int lmr_table_[MAX_SEARCH_PLY][64]` and `static bool lmr_initialized_` to `Search` class in Search.h
    - Add `static void init_lmr_table()` that computes `lmr_table_[d][i] = max(1, (int)floor(log(d) * log(i) / 2.0))` for d in [1, MAX_SEARCH_PLY), i in [1, 64)
    - Call `init_lmr_table()` once (static init guard) at the start of `Search::search()`
    - Replace the current fixed `depth - 2` LMR reduction with `depth - 1 - lmr_table_[min(depth, MAX_SEARCH_PLY-1)][min(i, 63)]`
    - Preserve existing LMR exclusions: captures, promotions, killer moves, in check
    - When reduced-depth search returns > alpha, re-search at full depth (existing PVS re-search behavior already handles this)
    - _Requirements: 6.1, 6.2, 6.3, 6.4_

  - [x] 1.7 Implement countermove heuristic
    - Add `Move_t countermoves_[2][64][64] = {}` to `Search` class in Search.h
    - Clear countermove table in `Search::search()` with `std::memset(countermoves_, 0, sizeof(countermoves_))`
    - On beta cutoff for quiet moves in `alphabeta()`, record the move as countermove: `countermoves_[side][move_from(prev_move)][move_to(prev_move)] = move`
    - Pass the previous move through `alphabeta()` (add a `Move_t prev_move` parameter, default `0U`)
    - Rename `score_killers()` to `score_quiet_moves()` and add countermove scoring at priority 70 (below killer 80/90, above history 6..79 range — adjust history range to 6..69)
    - _Requirements: 7.1, 7.2, 7.3, 7.4_

  - [x] 1.8 Implement singular extensions
    - Add `bool singular_excluded_[MAX_SEARCH_PLY] = {}` to `Search` class in Search.h
    - Define `constexpr int SE_MIN_DEPTH = 8` and `constexpr int SE_MARGIN = 50` in Search.cpp
    - In `alphabeta()`, after TT probe returns a best_move (but before the move loop):
      - If `depth >= SE_MIN_DEPTH && search_ply > 0 && !singular_excluded_[search_ply]` and TT entry has `HASH_BETA` or `HASH_EXACT` flag with `tt_depth >= depth - 3`:
      - Set `singular_excluded_[search_ply] = true`
      - Run verification search: `alphabeta(tt_value - SE_MARGIN - 1, tt_value - SE_MARGIN, depth / 2, NO_PV, DO_NULL)` excluding the TT move
      - Set `singular_excluded_[search_ply] = false`
      - If verification result `< tt_value - SE_MARGIN`, extend the TT move by 1 ply
    - Requires modifying TT `probe()` to also return the stored depth and flags (add output parameters or a struct)
    - _Requirements: 8.1, 8.2, 8.3, 8.4, 8.5_

  - [ ]* 1.9 Write property tests for Phase 1 search improvements
    - **Property 3: Pruning is disabled when in check** — verify that for positions in check, search results are identical with and without futility/RFP/LMP/LMR enabled
    - **Validates: Requirements 2.3, 3.2, 5.2, 6.4**
    - **Property 6: LMR lookup table matches logarithmic formula** — verify `lmr_table_[d][i] == max(1, floor(log(d)*log(i)/2.0))` for all valid d, i
    - **Validates: Requirements 6.1**
    - **Property 7: Countermove table is cleared per search** — verify countermove table is zeroed at start of each search
    - **Validates: Requirements 7.4**
    - Place tests in `test/source/TestHceGmStrength.cpp` using Catch2

- [x] 2. Phase 1 Checkpoint
  - Ensure all tests pass, ask the user if questions arise.
  - Build with `cmake --preset dev && cmake --build build/dev`
  - Run existing tests to confirm no regressions

- [ ] 3. Phase 2 — HCE Evaluation Improvements
  - [x] 3.1 Implement pawn structure evaluation with pawn hash table
    - Add `PawnHashEntry` struct to Evaluator.h: `{ U64 key; int mg_score; int eg_score; }`
    - Add to `HandCraftedEvaluator`: `static constexpr int PAWN_HASH_SIZE = 16384`, `PawnHashEntry pawn_hash_[PAWN_HASH_SIZE]`, `int last_pawn_structure_ = 0`
    - Add `int eval_pawn_structure(const Board& board, int phase)` private method
    - Compute pawn-only Zobrist hash by XOR-ing `Zobrist::get_pieces(WHITE_PAWN, sq)` and `Zobrist::get_pieces(BLACK_PAWN, sq)` for all pawn squares
    - Probe pawn hash table; on miss, evaluate and store:
      - Passed pawns: tapered bonus increasing with rank (MG: 10-70, EG: 15-120 scaled by rank distance from start)
      - Isolated pawns: -10 MG / -20 EG per isolated pawn (no friendly pawns on adjacent files)
      - Doubled pawns: -10 MG / -10 EG per extra pawn on same file
      - Backward pawns: -8 MG / -10 EG (cannot advance safely, no friendly pawn behind on adjacent files)
      - Connected pawns: 5-15 MG / 5-10 EG scaled by rank (defended by or adjacent to friendly pawn)
    - Use file bitboard masks (`FILE_BB[f]`) and adjacent file masks for pawn feature detection
    - Store `last_pawn_structure_` for coaching protocol access
    - Add `int get_pawn_structure_score() const` accessor to `HandCraftedEvaluator`
    - _Requirements: 9.1, 9.2, 9.3, 9.4, 9.5, 9.6, 9.7_

  - [ ]* 3.2 Write property tests for pawn structure evaluation
    - **Property 8: Pawn structure scoring is consistent with pawn features** — verify passed pawn bonus increases monotonically with rank, isolated/doubled/backward pawns are penalized, connected pawns are bonused
    - **Validates: Requirements 9.1, 9.2, 9.3, 9.4, 9.5**
    - **Property 9: Pawn hash table caching is transparent** — verify eval with empty vs pre-populated pawn hash produces identical results
    - **Validates: Requirements 9.6**
    - Place tests in `test/source/TestHceGmStrength.cpp`

  - [x] 3.3 Implement king safety evaluation
    - Add `int eval_king_safety(const Board& board, int phase)` private method to `HandCraftedEvaluator`
    - Add `int last_king_safety_ = 0` member and `int get_king_safety_score() const` accessor
    - Only compute when `phase > 40` (out of 128); return 0 for endgame positions
    - Evaluate for each side:
      - Pawn shield: -15 MG per missing pawn on king's file and adjacent files within 1-2 ranks ahead of castled king
      - Open/semi-open files near king: -20 MG per open or semi-open file adjacent to king
      - King zone attacks: -8 MG per enemy piece attacking king zone squares (king square + 8 neighbors, use `KING_LOOKUP_TABLE[king_sq]`)
    - Compute enemy attacks using `MoveGenerator::knight_targets()`, `MoveGenerator::rook_attacks()`, `MoveGenerator::bishop_attacks()` on occupied bitboard, intersected with king zone
    - King safety is MG-only (EG weight = 0), tapered by phase
    - _Requirements: 10.1, 10.2, 10.3, 10.4, 10.5_

  - [ ]* 3.4 Write property tests for king safety evaluation
    - **Property 11: King safety scoring reflects shield and attacks** — verify intact shield scores better than broken shield; open files penalized; more attackers = worse score; endgame positions contribute zero
    - **Validates: Requirements 10.1, 10.2, 10.3, 10.4**
    - Place tests in `test/source/TestHceGmStrength.cpp`

  - [x] 3.5 Implement bitboard-based mobility evaluation
    - Add `int eval_mobility(const Board& board, int phase)` private method to `HandCraftedEvaluator`
    - Add `int last_mobility_ = 0` member and `int get_mobility_score() const` accessor
    - For each piece, compute pseudo-legal destination squares using:
      - Knights: `MoveGenerator::knight_targets(1ULL << sq)` (from `KNIGHT_LOOKUP_TABLE`)
      - Bishops: `MoveGenerator::bishop_attacks(occupied, sq)`
      - Rooks: `MoveGenerator::rook_attacks(occupied, sq)`
      - Queens: `MoveGenerator::rook_attacks(occupied, sq) | MoveGenerator::bishop_attacks(occupied, sq)`
    - Exclude squares occupied by friendly pieces and squares controlled by enemy pawns (compute enemy pawn attack mask using pawn attack patterns)
    - Apply per-piece-type tapered weights: knight/bishop 4/4 MG/EG per square, rook 2/3, queen 1/2
    - Replace the current `MoveGenerator::add_all_moves` mobility approach in `evaluate()` with this bitboard-based approach
    - _Requirements: 11.1, 11.2, 11.3, 11.4, 11.5, 11.6_

  - [ ]* 3.6 Write property tests for mobility evaluation
    - **Property 12: Mobility uses bitboard attack counts excluding friendly and pawn-controlled squares** — verify mobility count equals popcount of attack BB minus friendly and enemy-pawn-controlled squares; knight/bishop weighted higher than rook/queen; tapered between MG/EG
    - **Validates: Requirements 11.2, 11.3, 11.4**
    - Place tests in `test/source/TestHceGmStrength.cpp`

  - [x] 3.7 Implement bishop pair, rook file bonuses, and rook/queen on 7th rank
    - Add `int eval_piece_bonuses(const Board& board, int phase)` private method to `HandCraftedEvaluator`
    - Bishop pair: if `pop_count(board.bitboard(BISHOP | side)) >= 2`, add tapered bonus 30 MG / 50 EG
    - Rook on open file (no pawns of either color on file): 20 MG / 25 EG per rook
    - Rook on semi-open file (no friendly pawns, enemy pawn present): 10 MG / 15 EG per rook
    - Rook/Queen on 7th rank: if white rook/queen on rank 6 (index) and black king on rank 7, add 20 MG / 30 EG; symmetric for black
    - Use `FILE_BB[file]` masks intersected with pawn bitboards for file status detection
    - _Requirements: 12.1, 13.1, 13.2, 14.1, 14.2_

  - [ ]* 3.8 Write property tests for piece bonuses
    - **Property 13: Bishop pair bonus is applied correctly** — verify 30/50 cp tapered bonus when side has 2+ bishops vs 1 bishop
    - **Validates: Requirements 12.1**
    - **Property 14: Rook file bonuses are applied correctly** — verify open file 20/25, semi-open 10/15, no bonus with friendly pawns
    - **Validates: Requirements 13.1, 13.2**
    - **Property 15: Rook/Queen on 7th rank bonus** — verify 20/30 cp bonus when rook/queen on 7th and enemy king on 8th
    - **Validates: Requirements 14.1, 14.2**
    - Place tests in `test/source/TestHceGmStrength.cpp`

  - [x] 3.9 Wire new eval components into `HandCraftedEvaluator::evaluate()`
    - Modify `evaluate()` to call `eval_pawn_structure()`, `eval_king_safety()`, `eval_mobility()`, `eval_piece_bonuses()` and sum their contributions
    - Remove the old `MoveGenerator::add_all_moves` mobility code from `evaluate()`
    - Keep the existing PSQT material scoring and tempo bonus
    - Add `pawn_structure_enabled`, `king_safety_enabled`, `piece_bonuses_enabled` flags to `EvalConfig` struct
    - Update `PositionAnalyzer::compute_eval_breakdown()` to use the new HCE sub-score accessors (`get_pawn_structure_score()`, `get_king_safety_score()`, `get_mobility_score()`) instead of its own approximations
    - _Requirements: 9.7, 10.5, 11.6_

  - [ ]* 3.10 Write property test for eval sub-score decomposition
    - **Property 10: Eval sub-scores sum to total eval** — verify material + mobility + king_safety + pawn_structure + piece_bonuses + tempo ≈ total HCE eval for a set of test positions
    - **Validates: Requirements 9.7, 10.5, 11.6**
    - Place tests in `test/source/TestHceGmStrength.cpp`

- [x] 4. Phase 2 Checkpoint
  - Ensure all tests pass, ask the user if questions arise.
  - Build and run test suite to confirm no regressions

- [ ] 5. Phase 3 — TT Improvements
  - [x] 5.1 Add generation field and aging to TranspositionTable
    - Add `uint8_t generation` field to `HASHE` struct in TranspositionTable.h
    - Add `uint8_t generation_ = 0` private member to `TranspositionTable`
    - Add `void new_generation() { generation_ = (generation_ + 1) & 0xFF; }` and `uint8_t generation() const` public methods
    - In `record()`: store `generation_` in the entry; implement depth-preferred replacement:
      - Empty slot (key == 0): always replace
      - Stale entry (entry.generation != generation_): always replace
      - Same generation, new depth >= existing depth: replace
      - Same generation, new depth < existing depth: keep existing
    - In `clear()`: also zero the generation field of each entry
    - Call `tt.new_generation()` at the start of `Search::search()` (before iterative deepening loop)
    - _Requirements: 15.1, 15.2, 15.3, 15.4, 17.1, 17.2, 17.3, 17.4_

  - [ ]* 5.2 Write property tests for TT replacement and aging
    - **Property 16: TT depth-preferred replacement with aging** — verify all four replacement cases: empty slot, stale entry, deeper/equal replaces, shallower same-gen kept
    - **Validates: Requirements 15.1, 15.2, 15.3, 17.3**
    - **Property 18: TT generation tracking** — verify generation increments by 1, wraps at 256, and record stores current generation
    - **Validates: Requirements 17.1, 17.2, 17.4**
    - Place tests in `test/source/TestHceGmStrength.cpp`

  - [x] 5.3 Implement configurable TT size via UCI and Xboard
    - The existing `TranspositionTable::resize(int size_mb)` already handles resizing
    - In the UCI handler (find `setoption` parsing), add handling for `name Hash value N`: call `board.get_tt().resize(N)`
    - In the Xboard handler, add handling for `memory N`: call `board.get_tt().resize(N)`
    - Clamp N to [1, 4096] range before calling resize
    - Set default TT size to 64 MB in `TranspositionTable` constructor (change `HASH_TABLE_SIZE` or pass 64 MB worth of entries)
    - _Requirements: 16.1, 16.2, 16.3, 16.4_

  - [ ]* 5.4 Write property test for TT sizing
    - **Property 17: TT resize accepts valid sizes** — verify resize(N) for N in [1, 4096] produces approximately N MB of storage; verify 64 MB default
    - **Validates: Requirements 16.1, 16.2, 16.3, 16.4**
    - Place tests in `test/source/TestHceGmStrength.cpp`

- [x] 6. Phase 3 Checkpoint
  - Ensure all tests pass, ask the user if questions arise.

- [x] 7. Phase 4 — Miscellaneous Fixes
  - [x] 7.1 Implement history table aging
    - The iterative deepening loop in `Search::search()` already halves history values at `current_depth > 1` (lines ~195-205 in Search.cpp)
    - Verify the aging happens BEFORE the search at each new depth (it currently does)
    - If the implementation is already correct, mark this as verified — no code change needed
    - _Requirements: 18.1, 18.2_

  - [x] 7.2 Optimize `is_draw()` repetition detection
    - In `Board::is_draw()`, compute scan start as `max(0, game_ply_ - irrev_.half_move_count)` instead of scanning full history
    - Step by 2 when scanning (only same-side-to-move positions can repeat)
    - Preserve correct threefold detection: 3 occurrences outside search, 2 during search
    - _Requirements: 19.1, 19.2, 19.3_

  - [ ]* 7.3 Write property tests for Phase 4
    - **Property 19: History table aging halves values** — verify all history values are floor(original/2) after aging
    - **Validates: Requirements 18.1**
    - **Property 20: Repetition detection correctness** — verify optimized is_draw produces identical results to naive full-history scan for a set of game histories
    - **Validates: Requirements 19.1, 19.2, 19.3**
    - Place tests in `test/source/TestHceGmStrength.cpp`

- [x] 8. Final Checkpoint
  - Ensure all tests pass, ask the user if questions arise.
  - Build with both `dev` and `rel` presets
  - Run WAC and STS benchmark suites to measure strength improvement
  - _Requirements: 20.1, 20.2, 20.3, 20.4_

## Notes

- Tasks marked with `*` are optional and can be skipped for faster MVP
- Each task references specific requirements for traceability
- Checkpoints ensure incremental validation between phases
- Property tests validate universal correctness properties from the design document
- The SPRT validation (Requirement 20) is performed manually by the developer between phases using fast-chess
- All eval sub-scores must remain individually accessible for the coaching protocol's `EvalBreakdown`
