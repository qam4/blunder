# Implementation Plan: UCI MultiPV Support

## Overview

Add MultiPV support to the blunder chess engine's UCI protocol and alpha-beta search. The implementation proceeds bottom-up: define new types, modify the search core for root move exclusion, build the MultiPV loop in iterative deepening, update UCI option handling and info output, then wire everything together. Each task builds incrementally on the previous one.

## Tasks

- [x] 1. Define PVLine struct and add MultiPV state to Search
  - [x] 1.1 Create the `PVLine` struct in `source/Search.h`
    - Add `PVLine` struct with `int score`, `std::vector<Move_t> moves`, `best_move()`, and `ponder_move()` helpers
    - Add `excluded_root_moves_` (`std::vector<Move_t>`) and `multipv_results_` (`std::vector<PVLine>`) private members to `Search`
    - Add `extract_pv_moves()` private helper declaration to `Search`
    - Add `const std::vector<PVLine>& get_multipv_results() const` public accessor to `Search`
    - _Requirements: 3.1, 3.3, 5.1, 5.2_

  - [x] 1.2 Implement `extract_pv_moves()` in `source/Search.cpp`
    - Walk the existing `PrincipalVariation` table and return moves as a `std::vector<Move_t>`
    - _Requirements: 3.1_

  - [x] 1.3 Update `Search::search()` signature to accept `int multipv_count` parameter
    - Add `int multipv_count = 1` parameter to `Search::search()` declaration in `Search.h` and definition in `Search.cpp`
    - Default value of 1 preserves existing call sites
    - _Requirements: 3.2_

- [x] 2. Implement root move exclusion in alphabeta
  - [x] 2.1 Add root move exclusion logic at ply 0 in `Search::alphabeta()` in `source/Search.cpp`
    - At the top of the move loop, when `search_ply == 0` and `excluded_root_moves_` is non-empty, skip any move found in the exclusion vector
    - This is the only change to the core alpha-beta algorithm
    - _Requirements: 3.1_

  - [ ]* 2.2 Write property test: Distinct root moves in MultiPV results (Property 2)
    - **Property 2: Distinct root moves in MultiPV results**
    - For positions with ≥2 legal moves and multipv_count in {2, 3, 4}, verify all PV lines have distinct root moves
    - **Validates: Requirements 3.1**

- [x] 3. Implement MultiPV search loop in iterative deepening
  - [x] 3.1 Modify the iterative deepening loop in `Search::search()` in `source/Search.cpp`
    - At each depth: clear `excluded_root_moves_`, loop `pv_index` from 0 to `min(multipv_count, num_legal_moves) - 1`
    - Each iteration: run `alphabeta()`, store score + extracted PV into `multipv_results_[pv_index]`, add root move to `excluded_root_moves_`
    - Check time limit / abort flag between PV iterations; if exceeded, stop and keep results from last fully completed depth
    - Sort `multipv_results_` by descending score after all iterations at a depth
    - When `multipv_count == 1`, the loop runs once with empty exclusion set (identical to current behavior)
    - _Requirements: 3.1, 3.2, 3.3, 3.4, 7.1, 7.2_

  - [x] 3.2 Update info line output in `Search::search()` for MultiPV
    - When `output_mode_ == UCI` and `multipv_count > 1`: output one info line per PV line with `multipv <1-indexed>` field
    - When `multipv_count == 1`: omit `multipv` field (backward compatible)
    - Each info line includes: depth, score cp, nodes, nps, time, pv move sequence, and conditionally multipv index
    - _Requirements: 4.1, 4.2, 4.3_

  - [x] 3.3 Update bestmove/ponder extraction from MultiPV results
    - After search completes, set `search_best_move_` and `search_best_score_` from `multipv_results_[0]`
    - Return `multipv_results_[0].best_move()` as the bestmove
    - _Requirements: 5.1, 5.2_

  - [ ]* 3.4 Write property test: MultiPV=1 backward compatibility (Property 3)
    - **Property 3: MultiPV=1 backward compatibility**
    - For each test position, compare multipv=1 results with single-PV search results; same best move and score
    - **Validates: Requirements 3.2**

  - [ ]* 3.5 Write property test: PV lines sorted by descending score (Property 4)
    - **Property 4: PV lines sorted by descending score**
    - For each test position and multipv_count, verify scores are non-increasing
    - **Validates: Requirements 3.3**

  - [ ]* 3.6 Write property test: PV count capped by legal moves (Property 5)
    - **Property 5: PV count capped by legal moves**
    - For positions with varying numbers of legal moves, verify result count = min(N, L)
    - **Validates: Requirements 3.4**

- [x] 4. Checkpoint - Verify search logic
  - Ensure all tests pass, ask the user if questions arise.

- [x] 5. Add MultiPV option to UCI handler
  - [x] 5.1 Add `multipv_count_` member to `UCI` class in `source/UCI.h`
    - Add `int multipv_count_ = 1;` private member
    - _Requirements: 2.3_

  - [x] 5.2 Advertise MultiPV option in `UCI::cmd_uci()` in `source/UCI.cpp`
    - Add `option name MultiPV type spin default 1 min 1 max 256` output line before `uciok`
    - _Requirements: 1.1_

  - [x] 5.3 Handle MultiPV in `UCI::cmd_setoption()` in `source/UCI.cpp`
    - Parse `setoption name MultiPV value N`, clamp N to [1, 256], store in `multipv_count_`
    - _Requirements: 2.1, 2.2_

  - [x] 5.4 Pass `multipv_count_` to search and handle MCTS bypass in `UCI::start_search()` in `source/UCI.cpp`
    - When `use_mcts_` is true: ignore `multipv_count_`, pass 1 to search (or don't pass it)
    - When `use_mcts_` is false: pass `multipv_count_` to `search_.search()`
    - After search: extract bestmove from `search_.get_multipv_results()[0].best_move()` and ponder from `ponder_move()`
    - _Requirements: 6.1, 8.1_

  - [ ]* 5.5 Write property test: MultiPV count clamping (Property 1)
    - **Property 1: MultiPV count clamping**
    - Generate random integers, send setoption, verify stored value equals clamp(N, 1, 256)
    - **Validates: Requirements 2.1, 2.2**

  - [ ]* 5.6 Write property test: Bestmove and ponder from top PV (Property 8)
    - **Property 8: Bestmove and ponder from top PV**
    - For each test position and multipv_count, verify bestmove equals first move of highest-ranked PV line, ponder equals second move
    - **Validates: Requirements 5.1, 5.2**

- [x] 6. Create test file and register in CMake
  - [x] 6.1 Create `test/source/TestMultiPV.cpp` with unit tests
    - UCI handshake advertises MultiPV option (Req 1.1)
    - Default MultiPV count is 1 (Req 2.3)
    - Book move takes precedence over MultiPV search (Req 6.1)
    - Time limit stops MultiPV mid-depth gracefully (Req 7.1, 7.2)
    - Single legal move position returns one PV line regardless of MultiPV count
    - _Requirements: 1.1, 2.3, 6.1, 7.1, 7.2_

  - [x] 6.2 Register `test/source/TestMultiPV.cpp` in `test/CMakeLists.txt`
    - Add `source/TestMultiPV.cpp` to the `blunder_test` executable sources
    - _Requirements: all_

  - [ ]* 6.3 Write property test: Multipv field conditional presence (Property 6)
    - **Property 6: Multipv field conditional presence**
    - Capture info output, verify `multipv` field present iff `multipv_count > 1`, values range from 1 to number of PV lines
    - **Validates: Requirements 4.1, 4.3**

  - [ ]* 6.4 Write property test: Info line field completeness (Property 7)
    - **Property 7: Info line field completeness**
    - Capture info output, verify each line contains depth, score cp, nodes, nps, time, pv; and multipv index when multipv_count > 1
    - **Validates: Requirements 4.2**

  - [ ]* 6.5 Write property test: MCTS ignores MultiPV (Property 9)
    - **Property 9: MCTS ignores MultiPV**
    - For test positions in MCTS mode with various multipv_count values, verify single result returned
    - **Validates: Requirements 8.1**

- [x] 7. Final checkpoint - Ensure all tests pass
  - Ensure all tests pass, ask the user if questions arise.

## Notes

- Tasks marked with `*` are optional and can be skipped for faster MVP
- Each task references specific requirements for traceability
- Property tests use Catch2 `GENERATE` with random seeds across diverse FEN positions
- All property tests go in `test/source/TestMultiPV.cpp` alongside unit tests
- Checkpoints ensure incremental validation after search logic and after full integration
