# Requirements Document

## Introduction

This spec covers the full evolution of the chess engine — from validating and hardening the existing algorithms, to performance optimization, debugging infrastructure, opening book support, and eventually machine learning / neural network evaluation. The goal is to transform this engine from a working hobby project into a competitive, well-tested, and extensible chess engine that can grow over time.

## Methodology

The requirements in this spec are driven by a structured code review process:

1. **Code Review** (Phase 0) — systematic review of every source file to identify bugs, correctness issues, code smells, architecture problems, missing features, and performance bottlenecks
2. **Bug Fixes & Correctness Hardening** (Phase 1) — fix the actual bugs and correctness issues found in the code review
3. **Code Quality & Architecture Refactoring** (Phase 2) — address the structural and style issues found in the code review
4. **Algorithm Correctness Validation** (Phase 3) — build a test suite that proves the algorithms work correctly
5. **Performance & Efficiency** (Phase 4) — optimize search, move ordering, and evaluation
6. **Debugging & Observability** (Phase 5) — add instrumentation and regression testing
7. **Opening Book Support** (Phase 6) — integrate Polyglot opening books
8. **Engine Strength Features** (Phase 7) — tablebases, time management
9. **Neural Network / ML / MCTS** (Phase 8) — NNUE evaluation, MCTS, AlphaZero-style self-play
10. **Protocol & Interoperability** (Phase 9) — UCI protocol support
11. **Strength Regression Testing** (Phase 10) — automated cutechess SPRT testing, WAC/STS solve rate tracking

Each phase builds on the previous one. Bug fixes and architecture cleanup come first because they establish a solid, testable foundation for everything that follows.

## Glossary

- **Board**: The central game-state object holding bitboards, piece arrays, move history, and search state
- **MiniMax**: A search algorithm that explores the game tree by alternating between maximizing and minimizing players
- **NegaMax**: A simplified variant of MiniMax that uses score negation instead of explicit min/max alternation
- **AlphaBeta**: An optimized search algorithm that prunes branches of the game tree using alpha-beta bounds, with PVS (Principal Variation Search) and null-move pruning enhancements
- **Quiescence_Search**: A search extension applied at leaf nodes of AlphaBeta that continues searching capture moves to avoid the horizon effect
- **SEE**: Static Exchange Evaluation; estimates the material outcome of a sequence of captures on a single square without performing a full search
- **Principal_Variation**: The sequence of best moves found during search, stored in a triangular PV table
- **Zobrist_Hashing**: A hashing scheme that produces a unique 64-bit key for each board position, used for the transposition table and draw detection
- **Transposition_Table**: A hash table that caches previously evaluated positions to avoid redundant search
- **FEN**: Forsyth-Edwards Notation; a standard string format for describing a chess board position
- **Parser**: The module responsible for converting FEN strings into Board objects and SAN move strings into Move_t values
- **Perft**: A debugging function that counts the number of leaf nodes at a given depth, used to verify move generation correctness
- **Move_Generator**: The module responsible for generating legal and pseudo-legal moves for a given board position
- **NNUE**: Efficiently Updatable Neural Network; a neural network evaluation that can be incrementally updated as moves are made/unmade, used by Stockfish and others
- **MCTS**: Monte Carlo Tree Search; a search algorithm that uses random simulations to evaluate positions, famously used by AlphaZero
- **Polyglot**: A standard binary format for chess opening books, using Zobrist hashing for position lookup
- **Syzygy**: Endgame tablebases that provide perfect play information for positions with few pieces
- **LMR**: Late Move Reductions; a search optimization that reduces depth for moves expected to be weak based on move ordering
- **NPS**: Nodes Per Second; a measure of search speed

## Requirements

---

## Phase 0: Code Review Findings

This phase documents the findings from a systematic review of the entire codebase. Each finding is categorized and drives one or more requirements in subsequent phases.

### CR-1: Board Class is a God Object
**Files:** Board.h, Board.cpp, AlphaBeta.cpp, MiniMax.cpp, NegaMax.cpp, Quiesce.cpp
**Severity:** Architecture
**Finding:** The Board class owns game state, search algorithms (minimax, negamax, alphabeta, quiesce), evaluation, hash probing, PV tracking, time management, and search counters. This makes it impossible to test search independently from board state, and prevents swapping evaluation strategies.
**Drives:** Req 34, 35

### CR-2: Global Mutable State (pv_table, hash_table)
**Files:** PrincipalVariation.cpp, Hash.cpp
**Severity:** Architecture
**Finding:** `pv_table[MAX_SEARCH_PLY * MAX_SEARCH_PLY]`, `pv_length[MAX_SEARCH_PLY]`, and `hash_table[HASH_TABLE_SIZE]` are global arrays. This makes the engine non-thread-safe and creates hidden coupling between components.
**Drives:** Req 35

### CR-3: Hash Table Reset Kills Iterative Deepening Performance
**Files:** Board.cpp (search())
**Severity:** Bug (Performance)
**Finding:** `reset_hash_table()` is called at the start of every `search()` call, wiping all TT entries from previous iterations. Entries from depth 1 are gone when depth 2 starts. This is a significant performance regression — the TT should persist across iterations and only be cleared between games.
**Drives:** Req 41

### CR-4: Aspiration Window Re-Search Doesn't Reset follow_pv_
**Files:** Board.cpp (search())
**Severity:** Bug
**Finding:** When the aspiration window fails and triggers a full-window re-search, `follow_pv_` is not reset to 1. The PV-following move ordering may not activate on the re-search, leading to suboptimal move ordering.
**Drives:** Req 42

### CR-5: is_game_over() Generates Moves Redundantly
**Files:** Board.cpp (is_game_over()), MiniMax.cpp, NegaMax.cpp
**Severity:** Performance
**Finding:** `is_game_over()` calls `MoveGenerator::add_all_moves()` to check if there are legal moves. MiniMax and NegaMax call `is_game_over()` before the depth check, so at every leaf node moves are generated and immediately discarded. AlphaBeta avoids this by checking `n == 0` after its own move generation.
**Drives:** Req 19, 20

### CR-6: Evaluate Sign Convention is Fragile
**Files:** Board.cpp (evaluate()), NegaMax.cpp, Quiesce.cpp, AlphaBeta.cpp
**Severity:** Code Smell / Latent Bug
**Finding:** `evaluate()` returns a score from White's perspective. NegaMax and Quiesce manually compute `who2move * evaluate()` to flip the sign. This is correct but fragile — if anyone calls `evaluate()` directly from AlphaBeta (e.g., for razoring or futility pruning), they'd need to remember the flip. There's no helper function to enforce this.
**Drives:** Req 44

### CR-7: Repetition Detection Uses Threefold During Search
**Files:** Board.cpp (is_draw())
**Severity:** Code Smell / Strength
**Finding:** `is_draw()` requires `repetition_count > 1` (threefold) everywhere, including during search. Most engines use twofold during search to avoid getting stuck in repetition loops. The current implementation is technically correct per the rules but suboptimal for playing strength.
**Drives:** Req 43

### CR-8: Null Move Reduction is Fixed at R=2
**Files:** AlphaBeta.cpp
**Severity:** Strength
**Finding:** Null move pruning uses a fixed `R = 2`. Most modern engines use adaptive R (e.g., R=3 for depth > 6). Fixed R=2 is conservative and costs search depth at higher depths.
**Drives:** Req 45

### CR-9: U16 Typedef is Actually 32 Bits
**Files:** Types.h
**Severity:** Bug
**Finding:** `typedef unsigned int U16;` — `unsigned int` is 32 bits on most platforms, not 16. The name is misleading and could cause subtle bugs if anyone relies on it being 16 bits.
**Drives:** Req 40

### CR-10: `using namespace std;` in Header File
**Files:** Common.h
**Severity:** Code Smell
**Finding:** `using namespace std;` in Common.h pollutes the global namespace for every file that includes it. This is a well-known C++ anti-pattern that can cause name collisions.
**Drives:** Req 36

### CR-11: `#define DEBUG` in Common.h
**Files:** Common.h
**Severity:** Code Smell
**Finding:** `#define DEBUG` is hardcoded in Common.h rather than being controlled by the build system. This means DEBUG is always defined regardless of build configuration.
**Drives:** Req 36

### CR-12: C-Style Typedefs and #define Constants
**Files:** Types.h, Hash.h
**Severity:** Code Smell
**Finding:** Uses `typedef` instead of `using`, `#define` for constants (HASH_TABLE_SIZE, HASH_EXACT, etc.), and C-style struct typedef (`typedef struct HASHE_s { ... } HASHE;`). These should use modern C++ equivalents.
**Drives:** Req 36

### CR-13: Xboard::run() is a 200+ Line if/strcmp Chain with goto
**Files:** Xboard.cpp
**Severity:** Code Smell
**Finding:** The command loop uses sequential `strcmp` calls, raw `char[]` buffers, `sscanf`, `#define _CRT_SECURE_NO_WARNINGS`, and `goto no_ponder`. This is hard to maintain and extend.
**Drives:** Req 37

### CR-14: Move_t is a Raw U32 with Free Functions
**Files:** Move.h
**Severity:** Code Smell
**Finding:** `Move_t` is `typedef U32` with ~20 free functions for construction and field access. The move score is packed into the same 32 bits as the move data, mixing search-specific state with move identity.
**Drives:** Req 38

### CR-15: Redundant #ifndef NDEBUG Guards Around assert()
**Files:** Board.cpp, MoveList.h, and others
**Severity:** Code Smell
**Finding:** Multiple files wrap `assert()` calls in `#ifndef NDEBUG` / `#endif`, which is redundant since `assert()` is already a no-op when NDEBUG is defined.
**Drives:** Req 47

### CR-16: SEE Has Stale TODO Comments
**Files:** See.cpp
**Severity:** Code Smell
**Finding:** Comments say `// TODO: handle en-passant captures` and `// TODO: handle promotions`, but both are actually implemented in the code below. The TODOs are misleading.
**Drives:** Req 46

### CR-17: SEE Behavior on Non-Capture Moves is Undefined
**Files:** See.cpp
**Severity:** Code Smell
**Finding:** SEE can be called on non-capture moves. For non-captures, `capture = board[to]` is EMPTY (0), so `gain[0] = 0`. The function proceeds to find defenders, which works but gives a misleading result. The precondition should be documented or enforced.
**Drives:** Req 46

### CR-18: Inconsistent Error Handling
**Files:** Various
**Severity:** Code Smell
**Finding:** The codebase mixes `assert()`, `cerr <<`, and silent failures. ValidateMove prints to cerr. Parser may silently produce corrupt state on invalid input. There's no consistent error reporting strategy.
**Drives:** Req 39

---

## Phase 1: Bug Fixes & Correctness Hardening

### Requirement 41: Hash Table Should Not Be Reset Between Iterative Deepening Depths

**User Story:** As a developer, I want the transposition table to persist across iterative deepening iterations within a single search() call, so that work from shallower depths is reused and search is faster.

#### Acceptance Criteria

1. THE engine SHALL NOT call reset_hash_table() at the start of Board::search(); the TT SHALL persist across iterative deepening iterations
2. THE engine SHALL only clear the TT between games (e.g., on "new" or "ucinewgame"), not between moves or search calls
3. WHEN the TT persists across iterations, THE engine SHALL produce the same best move as before (correctness preserved) while visiting fewer nodes (performance improved)

### Requirement 42: Aspiration Window Re-Search Must Reset follow_pv_

**User Story:** As a developer, I want the aspiration window re-search to reset follow_pv_ before the full-window retry, so that PV-following move ordering works correctly on the re-search.

#### Acceptance Criteria

1. WHEN the aspiration window search fails (score <= alpha or score >= beta), THE engine SHALL reset follow_pv_ = 1 before the full-window re-search
2. WHEN the aspiration window search succeeds, THE engine SHALL proceed normally without resetting follow_pv_
3. AFTER the fix, THE engine SHALL produce the same or better search results on positions where aspiration failures occur

### Requirement 43: Twofold Repetition Detection During Search

**User Story:** As a developer, I want the engine to treat a single repetition as a draw during search (twofold), so that it avoids getting stuck in repetition loops during play.

#### Acceptance Criteria

1. DURING search (search_ply_ > 0), THE engine SHALL treat a position as drawn if it has appeared at least once before in the current game + search path (twofold repetition)
2. AT the root (search_ply_ == 0), THE engine SHALL use the standard threefold repetition rule
3. THE is_draw() function SHALL accept a parameter or use context to distinguish between root-level and search-level repetition detection

### Requirement 44: Evaluate Sign Convention Safety

**User Story:** As a developer, I want the evaluate() sign convention to be explicitly documented and enforced, so that future search enhancements (razoring, futility pruning, etc.) don't introduce sign bugs.

#### Acceptance Criteria

1. THE evaluate() function SHALL be documented as returning a score from White's perspective (positive = good for White)
2. THE engine SHALL provide a side_relative_eval() helper that returns evaluate() from the side-to-move's perspective (applying the who2move flip)
3. ALL call sites that need a side-to-move-relative score (negamax, quiesce, future pruning techniques) SHALL use side_relative_eval() instead of manually computing who2move * evaluate()
4. THE alphabeta function SHALL use side_relative_eval() if it ever needs a static eval directly (e.g., for razoring, null-move verification, or futility pruning)

### Requirement 45: Adaptive Null Move Reduction

**User Story:** As a developer, I want null move pruning to use an adaptive reduction (R) based on depth, so that the engine searches more efficiently at higher depths.

#### Acceptance Criteria

1. THE engine SHALL use R=3 when depth > 6, and R=2 otherwise (or a similar adaptive scheme)
2. THE engine SHALL not apply null move pruning when in check, at PV nodes, or when the side to move has only king and pawns (zugzwang risk)
3. WHEN adaptive R is applied, THE engine SHALL search deeper on average within the same time budget compared to fixed R=2

### Requirement 46: SEE Handling of Non-Capture Moves

**User Story:** As a developer, I want SEE to clearly handle or reject non-capture moves, so that callers don't get misleading results.

#### Acceptance Criteria

1. WHEN SEE is called on a non-capture move, THE function SHALL either return 0 immediately (short-circuit) or assert that the move is a capture
2. THE SEE function SHALL document its precondition: the input move must be a capture (or the behavior for non-captures must be explicitly defined)
3. THE stale TODO comments ("handle en-passant captures", "handle promotions") SHALL be removed since both are already implemented

### Requirement 47: Redundant NDEBUG Guards Around Assert

**User Story:** As a developer, I want to remove the redundant `#ifndef NDEBUG` / `#endif` guards around assert() calls throughout the codebase, since assert() is already a no-op when NDEBUG is defined.

#### Acceptance Criteria

1. ALL instances of `#ifndef NDEBUG` wrapping a single `assert()` call SHALL be replaced with just the bare `assert()` call
2. THE codebase SHALL be audited for all occurrences of this pattern across all source files
3. THE behavior in both debug and release builds SHALL remain identical after the cleanup

---

## Phase 2: Code Quality & Architecture Refactoring

### Requirement 34: Board Class Decomposition (God Object)

**User Story:** As a developer, I want to break up the Board class which currently owns game state, search, evaluation, hashing, PV tracking, and time management, so that each concern is in its own class and the codebase is easier to extend and test.

#### Acceptance Criteria

1. THE Board class SHALL be responsible only for game state: piece placement, bitboards, side_to_move, castling_rights, ep_square, half_move_count, full_move_count, do_move, undo_move
2. Search logic (alphabeta, negamax, minimax, quiesce, iterative deepening) SHALL be extracted into a Search class that takes a Board reference
3. Evaluation logic (evaluate, piece-square tables) SHALL be extracted into an Evaluator class (or interface) that can be swapped (hand-crafted vs NNUE)
4. PV tracking (pv_table, pv_length, store_pv_move, print_pv, score_pv_move) SHALL be extracted into a PrincipalVariation class that owns its own state instead of using globals
5. Time management (is_search_time_over, should_stop_search, search_time_, search_start_time_) SHALL be extracted into a TimeManager class
6. Transposition table (probe_hash, record_hash) SHALL be extracted into a TranspositionTable class that owns the hash_table array instead of using a global

### Requirement 35: Eliminate Global Mutable State

**User Story:** As a developer, I want to remove global mutable arrays (pv_table, pv_length, hash_table) so that the engine is thread-safe and testable without hidden state coupling.

#### Acceptance Criteria

1. pv_table and pv_length SHALL be owned by a PrincipalVariation object, not declared as global arrays
2. hash_table SHALL be owned by a TranspositionTable object, not declared as a global array
3. ALL search-related counters (nodes_visited_, searched_moves_, search_ply_, max_search_ply_) SHALL be owned by the Search class, not by Board
4. AFTER refactoring, two independent searches on different Board instances SHALL not interfere with each other

### Requirement 36: Replace C-Style Patterns with Modern C++

**User Story:** As a developer, I want to modernize the codebase to use C++17 idioms so that the code is safer, more readable, and easier to maintain.

#### Acceptance Criteria

1. `using namespace std;` in Common.h SHALL be removed; all std types SHALL use explicit `std::` prefix or targeted using-declarations in .cpp files
2. `#define DEBUG` SHALL be replaced with a CMake-controlled compile definition
3. `#define` constants (HASH_TABLE_SIZE, HASH_EXACT, HASH_ALPHA, HASH_BETA) SHALL be replaced with `constexpr` or `enum class`
4. C-style casts SHALL be replaced with `static_cast`, `reinterpret_cast`, or `bit_cast` as appropriate
5. Raw char arrays and sscanf in Xboard.cpp SHALL be replaced with `std::string` and `std::istringstream`
6. `typedef struct HASHE_s { ... } HASHE;` SHALL use a plain `struct HASHE { ... };`
7. `typedef unsigned long long U64` etc. SHALL use `using U64 = uint64_t;` with `<cstdint>` types

### Requirement 37: Xboard Protocol Refactoring

**User Story:** As a developer, I want to refactor the Xboard command loop from a monolithic if/strcmp chain into a command dispatch table, so that adding new commands (and later UCI) is clean and maintainable.

#### Acceptance Criteria

1. THE Xboard::run() method SHALL use a command dispatch map (`std::unordered_map<std::string, handler>`) instead of sequential strcmp calls
2. THE `goto no_ponder` pattern SHALL be eliminated in favor of structured control flow
3. THE `#define _CRT_SECURE_NO_WARNINGS` SHALL be removed by replacing sscanf with safe C++ alternatives
4. EACH command handler SHALL be a separate method or lambda, not inline code in the run() loop

### Requirement 38: Move Encoding Type Safety

**User Story:** As a developer, I want the move encoding to use a proper Move class or strongly-typed wrapper instead of raw U32 with free functions, so that move construction and access are type-safe and self-documenting.

#### Acceptance Criteria

1. Move_t SHALL be a class (or struct with methods) rather than a raw `typedef U32`
2. Move construction SHALL use named constructors or a builder pattern instead of bare bitwise operations
3. Move field access (from, to, captured, flags, score) SHALL be member functions, not free functions
4. THE move score field SHALL be separated from the move data (score is search-specific, not part of the move identity)

### Requirement 39: Consistent Error Handling

**User Story:** As a developer, I want consistent error handling instead of a mix of assert, cerr, and silent failures, so that bugs are caught early and diagnosed quickly.

#### Acceptance Criteria

1. `#ifndef NDEBUG` guards around single assert statements SHALL be removed (assert is already a no-op in release builds)
2. THE engine SHALL define a consistent error reporting mechanism (exceptions, error codes, or a logging framework)
3. Parser errors (invalid FEN, invalid move text) SHALL return error information rather than silently producing corrupt state
4. ValidateMove SHALL use the same error reporting mechanism as the rest of the engine

### Requirement 40: U16 Typedef Mismatch

**User Story:** As a developer, I want to fix the misleading typedef `typedef unsigned int U16` which is actually 32 bits on most platforms, so that type sizes are correct and intention is clear.

#### Acceptance Criteria

1. `typedef unsigned int U16` SHALL be replaced with `using U16 = uint16_t;` (actual 16-bit type) or renamed to reflect its actual size
2. ALL usages of U16 SHALL be audited to confirm they work correctly with the corrected type size
3. ALL fixed-width types SHALL use `<cstdint>` types: `uint8_t`, `uint16_t`, `uint32_t`, `uint64_t`

---

## Phase 3: Algorithm Correctness Validation

### Requirement 1: MiniMax and NegaMax Score Equivalence

**User Story:** As a developer, I want to verify that MiniMax and NegaMax produce the same evaluation scores for any given position, so that I can confirm NegaMax is a correct refactoring of MiniMax.

#### Acceptance Criteria

1. WHEN a Board is set up from a valid FEN and both MiniMax and NegaMax are searched to the same depth, THE Board SHALL return identical best-move scores from minimax_root and negamax_root
2. WHEN MiniMax is called with maximizing_player=true for White, THE Board SHALL return the same score as NegaMax called for the same position and depth
3. WHEN MiniMax is called with maximizing_player=false for Black, THE Board SHALL return the negation of the NegaMax score for the same position and depth

### Requirement 2: AlphaBeta Score Consistency with NegaMax

**User Story:** As a developer, I want to verify that AlphaBeta with a full window produces the same score as NegaMax, so that I can confirm the pruning does not alter the result.

#### Acceptance Criteria

1. WHEN AlphaBeta is called with alpha=-MAX_SCORE and beta=MAX_SCORE (full window) at a given depth, THE Board SHALL return the same score as NegaMax at the same depth for the same position
2. WHEN AlphaBeta prunes branches via beta-cutoff, THE Board SHALL visit fewer or equal nodes compared to NegaMax at the same depth
3. WHEN AlphaBeta is called with PVS enabled, THE Board SHALL return the same score as AlphaBeta called without PVS for the same position and depth

### Requirement 3: Board State Invariance After Search

**User Story:** As a developer, I want to ensure that running any search algorithm leaves the Board in its original state, so that searches have no side effects on the game state.

#### Acceptance Criteria

1. WHEN any search algorithm (MiniMax, NegaMax, or AlphaBeta) completes, THE Board SHALL have the same Zobrist hash as before the search started
2. WHEN any search algorithm completes, THE Board SHALL have the same side_to_move, castling_rights, ep_square, half_move_count, and full_move_count as before the search started
3. WHEN any search algorithm completes, THE Board SHALL have the same piece placement on all 64 squares as before the search started

### Requirement 4: do_move / undo_move Round-Trip Correctness

**User Story:** As a developer, I want to verify that do_move followed by undo_move restores the Board to its exact prior state, so that the search tree traversal is correct.

#### Acceptance Criteria

1. WHEN do_move is called with a legal move and then undo_move is called with the same move, THE Board SHALL have the same Zobrist hash as before do_move
2. WHEN do_move is called with a legal move and then undo_move is called with the same move, THE Board SHALL have the same bitboard state for all 14 piece bitboards as before do_move
3. WHEN do_move is called with a legal move and then undo_move is called with the same move, THE Board SHALL have the same board_array_ contents for all 64 squares as before do_move
4. WHEN do_move is called with a capture, en-passant, castling, or promotion move and then undo_move is called, THE Board SHALL restore castling_rights, ep_square, half_move_count, and side_to_move to their prior values

### Requirement 5: Zobrist Hash Incremental Update Correctness

**User Story:** As a developer, I want to verify that the incrementally updated Zobrist hash after each move matches a full recomputation, so that the transposition table lookups are reliable.

#### Acceptance Criteria

1. WHEN do_move is called, THE Board SHALL have a Zobrist hash equal to the value computed by Zobrist::get_zobrist_key from scratch on the resulting position
2. WHEN do_move followed by undo_move is called, THE Board SHALL have a Zobrist hash equal to the original hash before do_move
3. WHEN a sequence of moves is played and then fully undone, THE Board SHALL have a Zobrist hash equal to the initial position hash

### Requirement 6: SEE Correctness

**User Story:** As a developer, I want to verify that the Static Exchange Evaluation correctly estimates material gain or loss for capture sequences, so that move ordering and pruning decisions are sound.

#### Acceptance Criteria

1. WHEN SEE is called on a capture where the capturing piece is undefended, THE SEE SHALL return the value of the captured piece
2. WHEN SEE is called on a capture where the captured piece is defended by an equal-value piece, THE SEE SHALL return zero
3. WHEN SEE is called on a capture involving a promotion, THE SEE SHALL include the promotion piece value minus the pawn value in the result
4. WHEN SEE is called on an en-passant capture, THE SEE SHALL return the correct material exchange value
5. THE SEE SHALL return a value that is symmetric: SEE of White capturing on a square SHALL equal the negation of SEE of Black capturing on the mirrored position

### Requirement 7: Quiescence Search Correctness

**User Story:** As a developer, I want to verify that the Quiescence Search correctly extends the search through capture sequences and does not miss tactical threats, so that the evaluation at leaf nodes is stable.

#### Acceptance Criteria

1. WHEN Quiescence_Search is called on a quiet position (no captures available), THE Board SHALL return the static evaluation (stand-pat score)
2. WHEN Quiescence_Search is called on a position with a winning capture sequence, THE Board SHALL return a score at least as high as the stand-pat score
3. WHEN Quiescence_Search completes, THE Board SHALL have the same piece placement and Zobrist hash as before the call (no side effects)
4. WHEN Quiescence_Search is called, THE Board SHALL evaluate only capture moves and not consider quiet moves

### Requirement 8: Mate Detection Correctness

**User Story:** As a developer, I want to verify that all search algorithms correctly detect checkmate and stalemate, so that the engine plays correctly in endgame positions.

#### Acceptance Criteria

1. WHEN a position is checkmate, THE Board SHALL return a score within the range [-MATE_SCORE + ply, MATE_SCORE - ply] from all search algorithms
2. WHEN a position is stalemate, THE Board SHALL return DRAW_SCORE from all search algorithms
3. WHEN a mate-in-N position is searched at sufficient depth, THE Board SHALL find the mating move and return a mate score that reflects the correct distance to mate
4. WHEN a position is checkmate for the side to move, THE Board SHALL return a negative mate score (losing) from the perspective of the side to move

### Requirement 9: Principal Variation Integrity

**User Story:** As a developer, I want to verify that the Principal Variation collected during search consists of legal moves that form a valid game sequence, so that the PV display and PV-based move ordering are correct.

#### Acceptance Criteria

1. WHEN AlphaBeta search completes, THE Principal_Variation SHALL contain only legal moves for the position at each ply
2. WHEN the Principal_Variation is replayed on the Board using do_move, each move in the sequence SHALL be a legal move in the resulting position
3. WHEN AlphaBeta search completes, THE Principal_Variation length SHALL equal pv_length[0]
4. WHEN the Principal_Variation is replayed and then fully undone, THE Board SHALL return to its original state

### Requirement 10: Transposition Table Consistency

**User Story:** As a developer, I want to verify that the transposition table stores and retrieves correct values, so that hash-based pruning does not corrupt search results.

#### Acceptance Criteria

1. WHEN record_hash is called with a depth, value, and flag, THE Board SHALL retrieve the same value via probe_hash when the position hash matches and the stored depth is sufficient
2. WHEN probe_hash is called with a hash that does not match any stored entry, THE Board SHALL return UNKNOWN_SCORE
3. WHEN a HASH_EXACT entry is stored, THE Board SHALL return the stored value regardless of alpha and beta bounds
4. WHEN a HASH_ALPHA entry is stored and the stored value is less than or equal to alpha, THE Board SHALL return alpha
5. WHEN a HASH_BETA entry is stored and the stored value is greater than or equal to beta, THE Board SHALL return beta

### Requirement 11: FEN Round-Trip Correctness

**User Story:** As a developer, I want to verify that parsing a FEN string into a Board and then converting it back produces an equivalent FEN, so that the Parser is correct.

#### Acceptance Criteria

1. FOR ALL valid FEN strings, parsing the FEN into a Board and then converting the Board back to a FEN string SHALL produce a FEN that, when parsed again, yields an identical Board state
2. WHEN an invalid FEN string is provided, THE Parser SHALL handle the error without crashing or corrupting state
3. WHEN a FEN with special states (en-passant square, partial castling rights) is parsed, THE Board SHALL correctly reflect those states in side_to_move, castling_rights, and ep_square

### Requirement 12: Search Depth and Node Count Monotonicity

**User Story:** As a developer, I want to verify that increasing search depth does not decrease the number of nodes visited, so that the iterative deepening framework is functioning correctly.

#### Acceptance Criteria

1. WHEN search is called at depth D and then at depth D+1 for the same position, THE Board SHALL visit a node count at depth D+1 that is greater than or equal to the node count at depth D
2. WHEN AlphaBeta is called at a given depth, THE Board SHALL visit fewer or equal nodes compared to NegaMax at the same depth for the same position

### Requirement 13: Null Move do/undo Round-Trip Correctness

**User Story:** As a developer, I want to verify that do_null_move followed by undo_null_move restores the Board to its exact prior state, so that null-move pruning does not corrupt the search tree.

#### Acceptance Criteria

1. WHEN do_null_move is called and then undo_null_move is called, THE Board SHALL have the same Zobrist hash as before do_null_move
2. WHEN do_null_move is called and then undo_null_move is called, THE Board SHALL have the same side_to_move, castling_rights, ep_square, half_move_count, and full_move_count as before do_null_move
3. WHEN do_null_move is called and then undo_null_move is called, THE Board SHALL have the same piece placement on all 64 squares (null moves must not alter any pieces)
4. WHEN do_null_move is called on a position with an en-passant square set, THE Board SHALL clear the ep_square and update the Zobrist hash accordingly, and undo_null_move SHALL restore both

### Requirement 14: Aspiration Window Re-Search Correctness

**User Story:** As a developer, I want to verify that the aspiration window mechanism in iterative deepening produces the same result as a full-window search, so that the optimization does not miss better moves.

#### Acceptance Criteria

1. WHEN the aspiration window search falls outside [alpha, beta] and triggers a re-search with a full window, THE Board SHALL return a score equal to what a direct full-window search at the same depth would return
2. WHEN the aspiration window search succeeds (score within bounds), THE Board SHALL return the same best move as a full-window search at the same depth
3. WHEN iterative deepening completes, THE search_best_move_ SHALL be a legal move in the current position

### Requirement 15: Draw Detection Boundary Correctness

**User Story:** As a developer, I want to verify that draw detection correctly identifies fifty-move rule draws and threefold repetition, so that the engine does not play on in drawn positions or falsely declare draws.

#### Acceptance Criteria

1. WHEN half_move_count reaches exactly 100, THE Board::is_draw() SHALL return true (fifty-move rule)
2. WHEN half_move_count is 99, THE Board::is_draw() SHALL return false for the fifty-move rule
3. WHEN a position has appeared exactly twice in hash_history_ (current occurrence is the third), THE Board::is_draw() SHALL return true (threefold repetition)
4. WHEN a position has appeared only once in hash_history_ (current occurrence is the second), THE Board::is_draw() SHALL return false
5. WHEN a pawn move or capture resets half_move_count, THE Board SHALL not falsely detect a fifty-move draw on the next move

### Requirement 16: Evaluation Symmetry and Consistency

**User Story:** As a developer, I want to verify that the evaluation function is symmetric (swapping colors yields the negated score) and that all search algorithms interpret it consistently, so that the engine plays equally well as White and Black.

#### Acceptance Criteria

1. WHEN evaluate() is called on a position and then on the color-mirrored position (all pieces swapped White↔Black, board flipped vertically), THE Board SHALL return the negation of the original score
2. WHEN NegaMax calls evaluate(), it SHALL multiply by who2move (+1 for White, -1 for Black) to convert to the side-to-move perspective
3. WHEN MiniMax calls evaluate(), it SHALL use the raw score (positive = good for White) without side-to-move adjustment, consistent with its explicit max/min alternation
4. WHEN AlphaBeta calls quiesce() at depth 0, the stand-pat score SHALL use the same who2move convention as NegaMax

### Requirement 17: Global State Reset Between Searches

**User Story:** As a developer, I want to verify that reset_pv_table() and reset_hash_table() fully clear their respective global arrays, so that stale data from a previous search does not corrupt the next search.

#### Acceptance Criteria

1. WHEN reset_pv_table() is called, ALL entries in pv_table[0..MAX_SEARCH_PLY*MAX_SEARCH_PLY-1] SHALL be zero and ALL entries in pv_length[0..MAX_SEARCH_PLY-1] SHALL be zero
2. WHEN reset_hash_table() is called, ALL entries in hash_table[0..HASH_TABLE_SIZE-1] SHALL have key=0, depth=0, flags=0, value=0, and best_move=0
3. WHEN Board::search() is called, it SHALL call both reset_hash_table() and reset_pv_table() before beginning the iterative deepening loop

### Requirement 18: Search Ply Overflow Protection

**User Story:** As a developer, I want to verify that the search does not exceed MAX_SEARCH_PLY depth, so that move_stack_ and pv_table array bounds are never violated.

#### Acceptance Criteria

1. WHEN search_ply_ reaches MAX_SEARCH_PLY - 1 during any search, THE Board SHALL not call do_move again (the search must return before exceeding the bound)
2. WHEN Quiescence_Search detects search_ply_ > MAX_SEARCH_PLY - 1, it SHALL return the stand-pat score immediately without further recursion
3. WHEN AlphaBeta is called, THE search_ply_ SHALL never exceed MAX_SEARCH_PLY during the entire search tree traversal

### Requirement 19: is_game_over() Performance and Correctness

**User Story:** As a developer, I want to verify that is_game_over() correctly detects terminal positions and understand its performance impact, so that MiniMax and NegaMax do not waste time generating moves at leaf nodes.

#### Acceptance Criteria

1. WHEN is_game_over() is called on a checkmate position, it SHALL return true
2. WHEN is_game_over() is called on a stalemate position, it SHALL return true
3. WHEN is_game_over() is called on a drawn position (fifty-move or threefold repetition), it SHALL return true
4. WHEN MiniMax or NegaMax calls is_game_over() before the depth==0 check, THE Board SHALL generate moves redundantly at leaf nodes — this is a known performance issue to address in future optimization
5. WHEN is_game_over() returns true and the position is checkmate, THE search algorithm SHALL distinguish checkmate from stalemate by checking in_check


---

## Phase 4: Performance & Efficiency

### Requirement 20: Move Generation Efficiency at Leaf Nodes

**User Story:** As a developer, I want MiniMax and NegaMax to check depth before calling is_game_over(), so that leaf nodes do not waste time generating moves that are immediately discarded.

#### Acceptance Criteria

1. WHEN depth == 0, THE search algorithm SHALL return the evaluation without calling is_game_over() or generating moves
2. WHEN the optimization is applied, THE engine SHALL visit the same number of nodes but complete the search faster (measurable via benchmarks)

### Requirement 21: Move Ordering Improvements

**User Story:** As a developer, I want better move ordering (killer moves, history heuristic) so that AlphaBeta prunes more effectively and searches deeper in the same time.

#### Acceptance Criteria

1. THE engine SHALL implement killer move heuristic — storing two non-capture moves per ply that caused beta cutoffs
2. THE engine SHALL implement history heuristic — scoring quiet moves by how often they caused cutoffs across the search
3. WHEN move ordering is improved, THE engine SHALL visit fewer nodes at the same depth compared to the current implementation (measurable via benchmarks)

### Requirement 22: Late Move Reductions (LMR)

**User Story:** As a developer, I want to implement Late Move Reductions so that moves ordered late in the list are searched at reduced depth, allowing the engine to search deeper overall.

#### Acceptance Criteria

1. THE engine SHALL reduce the search depth for moves that are not captures, promotions, or killer moves and are searched after the first few moves
2. WHEN a reduced-depth search returns a score above alpha, THE engine SHALL re-search at full depth to verify
3. WHEN LMR is enabled, THE engine SHALL search deeper on average within the same time budget

---

## Phase 5: Debugging & Observability

### Requirement 23: Search Statistics and Logging

**User Story:** As a developer, I want detailed search statistics (branching factor, hash hit rate, cutoff rate, NPS) so that I can diagnose performance issues and measure the impact of changes.

#### Acceptance Criteria

1. THE engine SHALL track and report: nodes per second (NPS), hash table hit rate, beta-cutoff rate, and effective branching factor
2. THE engine SHALL support a verbose/debug mode that logs per-depth statistics during iterative deepening
3. THE statistics SHALL be resettable between searches and accessible programmatically for testing

### Requirement 24: Perft Integration for Regression Testing

**User Story:** As a developer, I want perft results to be validated against known-correct node counts for standard positions, so that any move generation change can be quickly regression-tested.

#### Acceptance Criteria

1. THE engine SHALL validate perft results for the initial position at depths 1–6 against published correct values
2. THE engine SHALL validate perft results for at least 5 tricky positions (promotions, en-passant, castling edge cases) from the perft test suite
3. WHEN any move generation code is changed, THE perft tests SHALL detect regressions

---

## Phase 6: Opening Book Support

### Requirement 25: Polyglot Opening Book Integration

**User Story:** As a developer, I want the engine to read Polyglot (.bin) opening book files so that it can play known strong openings without spending search time.

#### Acceptance Criteria

1. THE engine SHALL parse Polyglot .bin format opening book files (the books/ directory already contains .bin files)
2. WHEN the current position matches a book entry, THE engine SHALL return the book move instead of searching
3. THE engine SHALL support weighted random selection among multiple book moves for the same position
4. WHEN no book move is found, THE engine SHALL fall back to normal search seamlessly
5. THE engine SHALL support enabling/disabling book usage via a command-line flag or xboard command

### Requirement 26: Opening Book Zobrist Compatibility

**User Story:** As a developer, I want to verify that the engine's Zobrist keys are compatible with the Polyglot book format, so that book lookups work correctly.

#### Acceptance Criteria

1. THE engine's Zobrist hash for the initial position SHALL match the Polyglot specification's hash for the same position
2. FOR ALL positions reachable in the first 10 moves of common openings, THE engine's hash SHALL match the Polyglot hash
3. IF the engine's Zobrist scheme differs from Polyglot, THE engine SHALL provide a conversion function

---

## Phase 7: Engine Strength Features

### Requirement 27: Endgame Tablebases (Syzygy)

**User Story:** As a developer, I want the engine to probe Syzygy endgame tablebases so that it plays perfectly in positions with few pieces remaining.

#### Acceptance Criteria

1. THE engine SHALL support probing Syzygy WDL (Win/Draw/Loss) tablebases for positions with up to 6 pieces
2. WHEN a tablebase hit occurs, THE engine SHALL use the tablebase result instead of searching
3. THE engine SHALL support configuring the tablebase path via command-line or config

### Requirement 28: Time Management

**User Story:** As a developer, I want smarter time management so that the engine allocates more time to complex positions and less to simple ones, rather than using a fixed time per move.

#### Acceptance Criteria

1. THE engine SHALL allocate time based on remaining clock time and expected number of moves remaining
2. WHEN the best move changes between iterations, THE engine SHALL allocate more time to stabilize the result
3. WHEN the position is clearly won or lost, THE engine SHALL spend less time searching
4. THE engine SHALL never exceed the allocated time (hard time limit)

---

## Phase 8: Neural Network / Machine Learning Evaluation

### Requirement 29: NNUE-Style Evaluation

**User Story:** As a developer, I want to replace or augment the hand-crafted evaluation with a neural network (NNUE-style), so that the engine can learn positional understanding from training data.

#### Acceptance Criteria

1. THE engine SHALL support loading a neural network weights file for evaluation
2. THE engine SHALL implement efficiently-updatable neural network evaluation (incremental updates on do_move/undo_move)
3. THE NNUE evaluation SHALL be usable as a drop-in replacement for the current evaluate() function
4. THE engine SHALL support falling back to the hand-crafted evaluation when no network file is available

### Requirement 30: Training Data Generation

**User Story:** As a developer, I want the engine to generate labeled training data from self-play, so that I can train and improve the neural network evaluation.

#### Acceptance Criteria

1. THE engine SHALL support a self-play mode that generates positions with search scores as labels
2. THE output format SHALL be compatible with standard NNUE training tools (e.g., bullet format or similar)
3. THE engine SHALL support configuring the number of games, search depth, and randomization for training data generation
4. THE generated data SHALL include the board position (FEN), search score, and game result


### Requirement 31: Monte Carlo Tree Search (MCTS)

**User Story:** As a developer, I want to implement MCTS as an alternative search algorithm, so that the engine can use AlphaZero-style search that combines neural network evaluation with stochastic simulations.

#### Acceptance Criteria

1. THE engine SHALL implement MCTS with UCB1 (Upper Confidence Bound) for node selection during tree traversal
2. THE engine SHALL support plugging in a neural network policy head to guide move selection (prior probabilities) during the expansion phase
3. THE engine SHALL support plugging in a neural network value head to evaluate leaf positions during the simulation phase, as an alternative to random rollouts
4. WHEN MCTS is used with a trained neural network, THE engine SHALL not require hand-crafted evaluation — the network provides both policy and value
5. THE engine SHALL support configuring the number of simulations per move and the exploration constant (C_puct)
6. THE MCTS implementation SHALL be compatible with the existing Board/do_move/undo_move infrastructure
7. THE engine SHALL support switching between AlphaBeta and MCTS search via a command-line flag or configuration

### Requirement 32: Self-Play Training Pipeline

**User Story:** As a developer, I want a self-play training loop (similar to AlphaZero) where the engine plays games against itself using MCTS + neural network, generates training data, and improves iteratively.

#### Acceptance Criteria

1. THE engine SHALL support a self-play mode where MCTS + neural network plays complete games against itself
2. FOR each position in self-play, THE engine SHALL record: board state, MCTS visit count distribution (policy target), and game outcome (value target)
3. THE training pipeline SHALL support exporting data in a format consumable by a Python-based training script (e.g., NumPy arrays or protobuf)
4. THE engine SHALL support loading updated network weights after training without recompilation (runtime weight loading)
5. THE pipeline SHALL support configuring: number of self-play games, simulations per move, temperature schedule, and Dirichlet noise for exploration

---

## Phase 9: Protocol & Interoperability

### Requirement 33: UCI Protocol Support

**User Story:** As a developer, I want the engine to support the UCI (Universal Chess Interface) protocol in addition to xboard, so that it can be used with modern GUIs and tournament managers.

#### Acceptance Criteria

1. THE engine SHALL implement the core UCI commands: uci, isready, ucinewgame, position, go, stop, quit
2. THE engine SHALL report search info (depth, score, nodes, nps, pv) via UCI info strings during search
3. THE engine SHALL support UCI options for hash size, threads, and book usage
4. THE engine SHALL auto-detect whether the GUI is speaking UCI or xboard based on the initial handshake

---

## Phase 10: Strength Regression Testing

### Requirement 48: Automated Cutechess SPRT Regression Testing

**User Story:** As a developer, I want an automated workflow that runs cutechess-cli SPRT matches between the current build and a baseline build, so that I can detect strength regressions before merging changes.

#### Acceptance Criteria

1. THE engine SHALL provide a CMake target or script that builds both a baseline (old) and candidate (new) engine binary
2. THE script SHALL invoke cutechess-cli with SPRT parameters (elo0=0, elo1=10, alpha=0.05, beta=0.05) to determine if the candidate is a regression
3. THE script SHALL use the existing Polyglot opening books (books/ directory) for game diversity
4. THE script SHALL produce a summary report with Elo difference, confidence interval, and pass/fail result
5. THE script SHALL support configurable parameters: time control, number of concurrent games, opening book, and SPRT bounds
6. THE script SHALL exit with a non-zero code if the SPRT test concludes the candidate is weaker (regression detected)

### Requirement 49: WAC/STS Solve Rate Tracking

**User Story:** As a developer, I want to track WAC and STS solve rates across versions so that I have a quick tactical/positional sanity check alongside the full SPRT regression test.

#### Acceptance Criteria

1. THE engine SHALL provide a script or test target that runs the WAC test suite at a fixed depth or time and reports the solve rate
2. THE engine SHALL provide a script or test target that runs the STS test suite and reports the aggregate score
3. THE solve rates SHALL be logged with version/commit information so trends can be tracked over time
4. A significant drop in solve rate (e.g., >5% on WAC or >50 points on STS) SHALL be flagged as a warning
