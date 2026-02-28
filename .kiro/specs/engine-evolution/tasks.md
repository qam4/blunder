# Implementation Tasks

## Phase 1: Bug Fixes & Correctness Hardening

- [x] 1. Remove reset_hash_table() from Board::search() (Req 41)
  - [x] 1.1 Remove the reset_hash_table() call from Board::search() in source/Board.cpp
  - [x] 1.2 Add reset_hash_table() call to Xboard::setup() in source/Xboard.cpp so TT is cleared between games
  - [x] 1.3 Verify the engine builds and existing tests pass

- [x] 2. Fix aspiration window follow_pv_ reset (Req 42)
  - [x] 2.1 Add follow_pv_ = 1 before the full-window re-search in Board::search() in source/Board.cpp

- [x] 3. Implement twofold repetition during search (Req 43)
  - [x] 3.1 Add bool in_search parameter to Board::is_draw() in source/Board.h and source/Board.cpp
  - [x] 3.2 When in_search is true, use repetition_count > 0 (twofold) instead of > 1 (threefold)
  - [x] 3.3 Update AlphaBeta.cpp and Quiesce.cpp to call is_draw(true)
  - [x] 3.4 Update all other is_draw() call sites to pass false (or default parameter)

- [x] 4. Add side_relative_eval() helper (Req 44)
  - [x] 4.1 Add int side_relative_eval() method to Board class in source/Board.h and source/Board.cpp
  - [x] 4.2 Replace who2move * evaluate() in NegaMax.cpp with side_relative_eval()
  - [x] 4.3 Replace who2move * evaluate() in Quiesce.cpp with side_relative_eval()

- [x] 5. Implement adaptive null move reduction (Req 45)
  - [x] 5.1 Replace fixed R=2 with adaptive R in AlphaBeta.cpp: R=3 when depth > 6, R=2 otherwise

- [x] 6. Fix SEE non-capture handling and stale TODOs (Req 46)
  - [x] 6.1 Add early return for non-capture moves at top of MoveGenerator::see() in source/See.cpp
  - [x] 6.2 Remove stale TODO comments about en-passant and promotions in source/See.cpp

- [x] 7. Remove redundant NDEBUG guards around assert (Req 47)
  - [x] 7.1 Audit all source files for #ifndef NDEBUG wrapping single assert() calls
  - [x] 7.2 Replace all found instances with bare assert() calls
  - [x] 7.3 Verify the engine builds in both debug and release configurations

## Phase 2: Code Quality & Architecture Refactoring

- [x] 8. Extract TranspositionTable class from global state (Req 34, 35)
  - [x] 8.1 Create source/TranspositionTable.h with TranspositionTable class owning the hash table vector, with clear(), probe(), and record() methods
    - Replace `#define` constants (HASH_TABLE_SIZE, HASH_EXACT, HASH_ALPHA, HASH_BETA) with `constexpr` / `enum class HashFlag`
    - Replace `typedef struct HASHE_s { ... } HASHE;` with plain `struct HASHE { ... };`
    - _Requirements: 34.6, 35.2, 36.3, 36.6_
  - [x] 8.2 Create source/TranspositionTable.cpp implementing probe() and record() by moving logic from source/Hash.cpp
    - _Requirements: 34.6, 35.2_
  - [x] 8.3 Update Board class to use TranspositionTable& instead of global hash_table array; update all probe_hash/record_hash call sites in source/Board.cpp, source/AlphaBeta.cpp
    - _Requirements: 34.6, 35.2_
  - [x] 8.4 Remove global hash_table array from source/Hash.cpp and source/Hash.h
    - _Requirements: 35.2_
  - [ ]* 8.5 Write unit tests for TranspositionTable in test/source/TestTranspositionTable.cpp
    - Test probe returns UNKNOWN_SCORE on miss
    - Test record + probe round-trip for EXACT, ALPHA, BETA flags
    - **Validates: Requirements 10.1, 10.2, 10.3, 10.4, 10.5**

- [x] 9. Extract PrincipalVariation class from global state (Req 34, 35)
  - [x] 9.1 Refactor source/PrincipalVariation.h to make PrincipalVariation a class owning pv_table_ and pv_length_ arrays as member data, with reset(), store_move(), score_move(), print(), get_best_move(), length() methods
    - _Requirements: 34.4, 35.1_
  - [x] 9.2 Update source/PrincipalVariation.cpp to implement member functions instead of free functions operating on globals
    - _Requirements: 34.4, 35.1_
  - [x] 9.3 Update all PV call sites in source/Board.cpp, source/AlphaBeta.cpp to use PrincipalVariation object instead of global arrays
    - _Requirements: 34.4, 35.1_

- [x] 10. Extract TimeManager class (Req 34, 35)
  - [x] 10.1 Create source/TimeManager.h with TimeManager class owning start_time_, search_time_, max_nodes_ with start(), is_time_over(), should_stop() methods
    - _Requirements: 34.5_
  - [x] 10.2 Move time management logic from Board (is_search_time_over, should_stop_search) into TimeManager methods
    - _Requirements: 34.5_
  - [x] 10.3 Update Board::search() and AlphaBeta to use TimeManager& instead of Board member time fields
    - _Requirements: 34.5_

- [x] 11. Extract Evaluator interface and HandCraftedEvaluator (Req 34)
  - [x] 11.1 Create source/Evaluator.h with abstract Evaluator base class (virtual evaluate(), virtual side_relative_eval()) and HandCraftedEvaluator subclass
    - _Requirements: 34.3_
  - [x] 11.2 Move evaluate() logic and piece-square tables from source/Board.cpp into HandCraftedEvaluator
    - _Requirements: 34.3_
  - [x] 11.3 Update all evaluate() and side_relative_eval() call sites to use Evaluator& reference
    - _Requirements: 34.3_

- [x] 12. Extract Search class (Req 34, 35)
  - [x] 12.1 Create source/Search.h with Search class taking Board&, Evaluator&, TranspositionTable&, TimeManager& and owning PrincipalVariation, search counters (nodes_visited_, searched_moves_, search_ply_, max_search_ply_, follow_pv_)
    - _Requirements: 34.2, 35.3_
  - [x] 12.2 Create source/Search.cpp by moving alphabeta, negamax, minimax, quiesce, search() logic from source/Board.cpp, source/AlphaBeta.cpp, source/NegaMax.cpp, source/MiniMax.cpp, source/Quiesce.cpp into Search methods
    - _Requirements: 34.2, 35.3_
  - [x] 12.3 Strip Board class down to game state only: piece placement, bitboards, side_to_move, castling_rights, ep_square, half_move_count, full_move_count, do_move, undo_move
    - _Requirements: 34.1_
  - [x] 12.4 Update source/Xboard.cpp to create and wire Search, Evaluator, TranspositionTable, TimeManager objects and call Search::search() instead of Board::search()
    - _Requirements: 34.2_
  - [x] 12.5 Verify the engine builds and all existing tests pass after decomposition
    - _Requirements: 34.1, 34.2, 34.3, 34.4, 34.5, 34.6, 35.1, 35.2, 35.3, 35.4_

- [x] 13. Checkpoint — Verify decomposition
  - Ensure all tests pass, ask the user if questions arise.

- [x] 14. Modernize C++ patterns (Req 36, 40)
  - [x] 14.1 Replace typedefs in source/Types.h with `<cstdint>` using-declarations: `using U64 = uint64_t;`, `using U32 = uint32_t;`, `using U16 = uint16_t;`, `using U8 = uint8_t;`
    - Audit all U16 usages to confirm they work with actual 16-bit type
    - _Requirements: 36.7, 40.1, 40.2, 40.3_
  - [x] 14.2 Remove `using namespace std;` from source/Common.h; add explicit `std::` prefixes to all affected files
    - _Requirements: 36.1_
  - [x] 14.3 Remove `#define DEBUG` from source/Common.h; add `target_compile_definitions(blunder PRIVATE $<$<CONFIG:Debug>:DEBUG>)` to CMakeLists.txt
    - _Requirements: 36.2_
  - [x] 14.4 Replace C-style casts with static_cast/reinterpret_cast throughout the codebase
    - _Requirements: 36.4_
  - [x] 14.5 Verify the engine builds in both Debug and Release configurations after all modernization changes
    - _Requirements: 36.1, 36.2, 36.3, 36.4, 36.5, 36.6, 36.7_

- [x] 15. Refactor Xboard command loop (Req 37)
  - [x] 15.1 Replace the if/strcmp chain in source/Xboard.cpp with a `std::unordered_map<std::string, Handler>` dispatch table; each command handler becomes a separate method or lambda
    - _Requirements: 37.1, 37.4_
  - [x] 15.2 Eliminate `goto no_ponder` with structured control flow (bool flag)
    - _Requirements: 37.2_
  - [x] 15.3 Replace raw char arrays, sscanf, and `#define _CRT_SECURE_NO_WARNINGS` with `std::string`, `std::getline`, and `std::istringstream`
    - _Requirements: 37.3, 36.5_

- [x] 16. Refactor Move encoding (Req 38)
  - [x] 16.1 Create a Move class (or struct with methods) wrapping the internal U32, with member functions from(), to(), captured(), flags(), is_capture(), is_promotion(), etc.
    - _Requirements: 38.1, 38.2, 38.3_
  - [x] 16.2 Separate move score into a ScoredMove wrapper used only in MoveList, so score is not packed into the move identity bits
    - _Requirements: 38.4_
  - [x] 16.3 Migrate all Move_t call sites (source/MoveGenerator.cpp, source/MoveList.cpp, source/AlphaBeta.cpp, source/See.cpp, etc.) to use the new Move class
    - _Requirements: 38.1, 38.2, 38.3_

- [x] 17. Implement consistent error handling (Req 39)
  - [x] 17.1 Define a consistent error reporting strategy: assert() for invariant violations, std::optional or error codes for Parser functions, a logging utility for runtime warnings
    - _Requirements: 39.2_
  - [x] 17.2 Update source/Parser.cpp to return error information (std::optional or error codes) on invalid FEN / invalid move text instead of silently producing corrupt state
    - _Requirements: 39.3_
  - [x] 17.3 Update source/ValidateMove.cpp to use the same error reporting mechanism instead of raw cerr
    - _Requirements: 39.4_

- [x] 18. Checkpoint — Verify Phase 2 complete
  - Ensure all tests pass, ask the user if questions arise.

## Phase 3: Algorithm Correctness Validation

- [x] 19. Implement algorithm equivalence tests (Req 1, 2)
  - [x] 19.1 Create test/source/TestAlgorithmEquivalence.cpp with Catch2 test cases
    - _Requirements: 1.1, 1.2, 1.3, 2.1, 2.2, 2.3_
  - [ ]* 19.2 Write property test: MiniMax and NegaMax produce identical scores for any valid FEN at the same depth
    - **Property 1: minimax_score == negamax_score for all positions and depths 1–4**
    - **Validates: Requirements 1.1, 1.2, 1.3**
  - [ ]* 19.3 Write property test: AlphaBeta with full window produces the same score as NegaMax
    - **Property 2: alphabeta(-MAX_SCORE, MAX_SCORE, d) == negamax(d) for all positions and depths 1–4**
    - **Validates: Requirements 2.1, 2.2**
  - [ ]* 19.4 Write property test: AlphaBeta visits fewer or equal nodes than NegaMax at the same depth
    - **Property 3: alphabeta_nodes <= negamax_nodes for all positions and depths**
    - **Validates: Requirement 2.2**

- [x] 20. Implement board invariance tests (Req 3, 4, 5, 13)
  - [x] 20.1 Create test/source/TestBoardInvariance.cpp with Catch2 test cases
    - _Requirements: 3.1, 3.2, 3.3, 4.1, 4.2, 4.3, 4.4, 5.1, 5.2, 5.3, 13.1, 13.2, 13.3, 13.4_
  - [ ]* 20.2 Write property test: Board state is identical before and after any search algorithm completes
    - **Property 4: board_hash_after_search == board_hash_before_search for all positions and algorithms**
    - **Validates: Requirements 3.1, 3.2, 3.3**
  - [ ]* 20.3 Write property test: do_move + undo_move is identity for all legal moves
    - **Property 5: For every legal move m, do_move(m) then undo_move(m) restores hash, bitboards, board_array, castling, ep, half_move, side_to_move**
    - **Validates: Requirements 4.1, 4.2, 4.3, 4.4**
  - [ ]* 20.4 Write property test: Incremental Zobrist hash equals full recomputation after do_move
    - **Property 6: After do_move(m), get_hash() == Zobrist::get_zobrist_key(board) for all legal moves**
    - **Validates: Requirements 5.1, 5.2, 5.3**
  - [ ]* 20.5 Write property test: do_null_move + undo_null_move is identity
    - **Property 7: do_null_move then undo_null_move restores hash, side_to_move, ep_square, pieces**
    - **Validates: Requirements 13.1, 13.2, 13.3, 13.4**

- [x] 21. Implement SEE and quiescence tests (Req 6, 7)
  - [ ]* 21.1 Write example-based tests for SEE in test/source/TestSee.cpp: undefended capture, equally-defended capture, promotion capture, en-passant capture
    - **Validates: Requirements 6.1, 6.2, 6.3, 6.4, 6.5**
  - [ ]* 21.2 Write property test: Quiescence search has no side effects on board state
    - **Property 8: board_hash_after_quiesce == board_hash_before_quiesce for all positions**
    - **Validates: Requirements 7.1, 7.2, 7.3, 7.4**

- [x] 22. Implement mate and draw detection tests (Req 8, 15)
  - [x] 22.1 Create test/source/TestDrawDetection.cpp with Catch2 test cases for fifty-move rule boundaries and threefold repetition
    - _Requirements: 15.1, 15.2, 15.3, 15.4, 15.5_
  - [ ]* 22.2 Write example-based tests for mate detection: checkmate returns mate score, stalemate returns draw score, mate-in-N at sufficient depth
    - **Validates: Requirements 8.1, 8.2, 8.3, 8.4**
  - [ ]* 22.3 Write example-based tests for draw boundaries: is_draw() at half_move_count 99 vs 100, twofold vs threefold
    - **Validates: Requirements 15.1, 15.2, 15.3, 15.4, 15.5**

- [x] 23. Implement evaluation symmetry tests (Req 16)
  - [x] 23.1 Create test/source/TestEvaluation.cpp with Catch2 test cases
    - _Requirements: 16.1, 16.2, 16.3, 16.4_
  - [ ]* 23.2 Write property test: evaluate(pos) == -evaluate(mirror(pos)) for color-mirrored positions
    - **Property 9: Evaluation is symmetric under color swap for all positions**
    - **Validates: Requirements 16.1**

- [x] 24. Implement search property tests (Req 9, 10, 11, 12, 14, 17, 18)
  - [x] 24.1 Create test/source/TestSearchProperties.cpp with Catch2 test cases
    - _Requirements: 9.1, 9.2, 9.3, 9.4, 12.1, 12.2, 14.1, 14.2, 14.3, 17.1, 17.2, 17.3, 18.1, 18.2, 18.3_
  - [ ]* 24.2 Write property test: PV consists of legal moves that form a valid game sequence
    - **Property 10: Every move in the PV is legal in the position at that ply**
    - **Validates: Requirements 9.1, 9.2, 9.3, 9.4**
  - [ ]* 24.3 Write property test: Node count is monotonically non-decreasing with depth
    - **Property 11: nodes(depth D+1) >= nodes(depth D) for all positions**
    - **Validates: Requirements 12.1, 12.2**
  - [ ]* 24.4 Write property test: Aspiration window re-search produces same result as full-window search
    - **Property 12: aspiration_search(pos, d) == full_window_search(pos, d) for all positions**
    - **Validates: Requirements 14.1, 14.2, 14.3**
  - [x] 24.5 Create test/source/TestFenRoundTrip.cpp with Catch2 test cases for FEN parse → serialize → parse round-trip
    - _Requirements: 11.1, 11.2, 11.3_
  - [ ]* 24.6 Write property test: FEN round-trip produces identical board state
    - **Property 13: parse(serialize(parse(fen))) == parse(fen) for all valid FEN strings**
    - **Validates: Requirements 11.1, 11.2, 11.3**

- [-] 25. Checkpoint — Verify Phase 3 tests pass
  - Ensure all tests pass, ask the user if questions arise.

## Phase 4: Performance & Efficiency

- [ ] 26. Optimize leaf node move generation (Req 20)
  - [ ] 26.1 In source/MiniMax.cpp and source/NegaMax.cpp, move the `depth == 0` check before `is_game_over()` so leaf nodes return evaluation without generating moves
    - _Requirements: 20.1, 20.2_
  - [ ]* 26.2 Write a benchmark test comparing node throughput before and after the optimization
    - **Validates: Requirement 20.2**

- [ ] 27. Implement killer move heuristic (Req 21)
  - [ ] 27.1 Add a `Move_t killers_[MAX_SEARCH_PLY][2]` table to the Search class in source/Search.h
    - _Requirements: 21.1_
  - [ ] 27.2 On beta cutoff of a quiet move in AlphaBeta, store the move as a killer for the current ply
    - _Requirements: 21.1_
  - [ ] 27.3 During move scoring, give killer moves a score between captures and quiet moves
    - _Requirements: 21.1, 21.3_

- [ ] 28. Implement history heuristic (Req 21)
  - [ ] 28.1 Add a `int history_[2][64][64]` table to the Search class in source/Search.h
    - _Requirements: 21.2_
  - [ ] 28.2 Increment history on beta cutoff for quiet moves; use history score as tiebreaker in move ordering
    - _Requirements: 21.2, 21.3_
  - [ ] 28.3 Clear history table at the start of each search
    - _Requirements: 21.2_

- [ ] 29. Implement Late Move Reductions (Req 22)
  - [ ] 29.1 In AlphaBeta, after the first 3-4 moves at depth >= 3, reduce search depth by 1 for non-capture, non-promotion, non-killer quiet moves
    - _Requirements: 22.1_
  - [ ] 29.2 If the reduced-depth search returns a score above alpha, re-search at full depth
    - _Requirements: 22.2_
  - [ ]* 29.3 Write a benchmark test comparing search depth reached in a fixed time budget with and without LMR
    - **Validates: Requirement 22.3**

- [ ] 30. Checkpoint — Verify Phase 4 optimizations
  - Ensure all tests pass, ask the user if questions arise.

## Phase 5: Debugging & Observability

- [ ] 31. Implement search statistics (Req 23)
  - [ ] 31.1 Add a SearchStats struct to source/Search.h with fields: nodes_visited, hash_hits, hash_probes, beta_cutoffs, total_moves_searched, start_time; and methods nps(), hash_hit_rate(), cutoff_rate(), branching_factor(), reset()
    - _Requirements: 23.1, 23.3_
  - [ ] 31.2 Instrument Search::alphabeta() and Search::quiesce() to update SearchStats counters on hash probes, hash hits, and beta cutoffs
    - _Requirements: 23.1_
  - [ ] 31.3 Add a verbose/debug mode to Search that logs per-depth statistics (depth, score, nodes, nps, pv) during iterative deepening
    - _Requirements: 23.2_

- [ ] 32. Expand perft regression tests (Req 24)
  - [ ] 32.1 In test/source/TestPerft.cpp, add Catch2 test cases validating perft for the initial position at depths 1–5 against published correct node counts
    - _Requirements: 24.1_
  - [ ] 32.2 Add perft test cases for at least 5 tricky positions: Kiwipete, en-passant edge cases, promotion positions, castling edge cases
    - _Requirements: 24.2, 24.3_

- [ ] 33. Checkpoint — Verify Phase 5 complete
  - Ensure all tests pass, ask the user if questions arise.

## Phase 6: Opening Book Support

- [ ] 34. Implement Polyglot book reader (Req 25, 26)
  - [ ] 34.1 Create source/Book.h with Book class: open(), has_move(), get_move(), close(), and internal PolyglotEntry struct and polyglot_hash() method
    - _Requirements: 25.1, 26.3_
  - [ ] 34.2 Create source/Book.cpp implementing Polyglot .bin file parsing (big-endian 16-byte entries), binary search lookup by key, and weighted random selection among multiple book moves
    - _Requirements: 25.1, 25.3_
  - [ ] 34.3 Implement polyglot_hash() that computes Polyglot-compatible Zobrist keys from board state (may differ from engine's internal Zobrist scheme)
    - _Requirements: 26.1, 26.2, 26.3_
  - [ ]* 34.4 Write unit tests for Book in test/source/TestBook.cpp: verify polyglot_hash matches published values for initial position and common openings, verify book lookup returns valid moves
    - **Validates: Requirements 25.1, 25.3, 26.1, 26.2**

- [ ] 35. Integrate book with search and Xboard (Req 25)
  - [ ] 35.1 In Xboard (or Search entry point), check book for a move before starting search; if found, return book move directly
    - _Requirements: 25.2, 25.4_
  - [ ] 35.2 Add a command-line flag or xboard command to enable/disable book usage
    - _Requirements: 25.5_

- [ ] 36. Checkpoint — Verify Phase 6 complete
  - Ensure all tests pass, ask the user if questions arise.

## Phase 7: Engine Strength Features

- [ ] 37. Integrate Syzygy tablebases (Req 27)
  - [ ] 37.1 Add the Fathom library as a vendored dependency or git submodule; update CMakeLists.txt
    - _Requirements: 27.1_
  - [ ] 37.2 Implement Syzygy WDL probing in Search: at root and during search, when piece count <= 6, probe tablebases and use the result instead of searching
    - _Requirements: 27.1, 27.2_
  - [ ] 37.3 Add a command-line option or config to set the Syzygy tablebase path
    - _Requirements: 27.3_

- [ ] 38. Implement smart time management (Req 28)
  - [ ] 38.1 Extend TimeManager with allocate() method that computes time based on remaining clock, increment, and expected moves remaining
    - _Requirements: 28.1_
  - [ ] 38.2 Allocate more time when the best move changes between iterative deepening iterations (instability detection)
    - _Requirements: 28.2_
  - [ ] 38.3 Allocate less time when the position evaluation is clearly won or lost
    - _Requirements: 28.3_
  - [ ] 38.4 Enforce a hard time limit that is never exceeded
    - _Requirements: 28.4_

- [ ] 39. Checkpoint — Verify Phase 7 complete
  - Ensure all tests pass, ask the user if questions arise.

## Phase 8: Neural Network / ML / MCTS

- [ ] 40. Implement NNUE evaluation (Req 29)
  - [ ] 40.1 Create source/NNUEEvaluator.h with NNUEEvaluator class inheriting from Evaluator; implement HalfKP architecture (768 → 256 → 32 → 32 → 1) with load(), evaluate(), side_relative_eval(), push(), pop(), add_piece(), remove_piece()
    - _Requirements: 29.1, 29.2, 29.3_
  - [ ] 40.2 Create source/NNUEEvaluator.cpp implementing forward pass with SIMD-friendly aligned accumulators, incremental accumulator updates on add_piece/remove_piece
    - _Requirements: 29.2_
  - [ ] 40.3 Integrate NNUEEvaluator with Board::do_move/undo_move via push/pop/add_piece/remove_piece calls
    - _Requirements: 29.2_
  - [ ] 40.4 Add fallback logic: if no network weights file is available, use HandCraftedEvaluator
    - _Requirements: 29.4_
  - [ ]* 40.5 Write unit tests verifying NNUE evaluate produces valid scores and incremental updates match full recomputation
    - **Validates: Requirements 29.1, 29.2, 29.3**

- [ ] 41. Implement training data generation (Req 30)
  - [ ] 41.1 Add a self-play mode to the engine that plays games using the current search, recording positions with search scores as labels
    - _Requirements: 30.1, 30.2_
  - [ ] 41.2 Output training data in a binary format: [768 floats: input features][1 float: search score][1 float: game result], compatible with standard NNUE training tools
    - _Requirements: 30.2_
  - [ ] 41.3 Add command-line options to configure number of games, search depth, and randomization for training data generation
    - _Requirements: 30.3, 30.4_

- [ ] 42. Implement MCTS search (Req 31)
  - [ ] 42.1 Create source/MCTS.h with MCTSNode struct (move, parent, children, visits, total_value, prior, ucb()) and MCTS class with search(), select(), expand(), simulate(), backpropagate()
    - _Requirements: 31.1, 31.6_
  - [ ] 42.2 Create source/MCTS.cpp implementing UCB1 selection, expansion with neural network policy priors, value head evaluation at leaves, and backpropagation
    - _Requirements: 31.1, 31.2, 31.3_
  - [ ] 42.3 Add configuration for number of simulations per move and exploration constant (C_puct)
    - _Requirements: 31.5_
  - [ ] 42.4 Add a command-line flag or config option to switch between AlphaBeta and MCTS search
    - _Requirements: 31.7_
  - [ ]* 42.5 Write unit tests verifying MCTS produces legal moves and visit counts increase with more simulations
    - **Validates: Requirements 31.1, 31.4, 31.5, 31.6**

- [ ] 43. Implement self-play training pipeline (Req 32)
  - [ ] 43.1 Add a self-play mode using MCTS + neural network that plays complete games, recording board state, MCTS visit count distribution (policy target), and game outcome (value target) for each position
    - _Requirements: 32.1, 32.2_
  - [ ] 43.2 Export training data in a format consumable by a Python training script (e.g., NumPy arrays or protobuf)
    - _Requirements: 32.3_
  - [ ] 43.3 Support runtime loading of updated network weights without recompilation
    - _Requirements: 32.4_
  - [ ] 43.4 Add configuration for: number of self-play games, simulations per move, temperature schedule, and Dirichlet noise for exploration
    - _Requirements: 32.5_

- [ ] 44. Checkpoint — Verify Phase 8 complete
  - Ensure all tests pass, ask the user if questions arise.

## Phase 9: Protocol & Interoperability

- [ ] 45. Implement UCI protocol support (Req 33)
  - [ ] 45.1 Create source/UCI.h with UCI class using a command dispatch map, with handlers for: uci, isready, ucinewgame, position, go, stop, quit, setoption
    - _Requirements: 33.1, 33.3_
  - [ ] 45.2 Create source/UCI.cpp implementing all UCI command handlers: position parsing (startpos + moves, fen + moves), go with time controls (wtime, btime, winc, binc, depth, nodes, movetime), info string output during search (depth, score, nodes, nps, pv)
    - _Requirements: 33.1, 33.2_
  - [ ] 45.3 Add UCI options for hash size, threads, and book usage via setoption handler
    - _Requirements: 33.3_
  - [ ] 45.4 Implement protocol auto-detection in source/main.cpp: read first line, if "uci" launch UCI handler, otherwise launch Xboard handler (replaying the first line)
    - _Requirements: 33.4_
  - [ ]* 45.5 Write unit tests for UCI in test/source/TestUCI.cpp: verify correct responses to uci, isready, position, go commands
    - **Validates: Requirements 33.1, 33.2, 33.3**

- [ ] 46. Final checkpoint — Verify all phases complete
  - Ensure all tests pass, ask the user if questions arise.

## Notes

- Tasks marked with `*` are optional and can be skipped for faster MVP
- Each task references specific requirements for traceability
- Checkpoints ensure incremental validation between phases
- Property tests validate universal correctness properties from the design document
- Phases are sequential: complete Phase 2 before Phase 3, etc.
- Build command: `cmake --build --preset=dev`
- Test files go in `test/source/`, new source files in `source/`
