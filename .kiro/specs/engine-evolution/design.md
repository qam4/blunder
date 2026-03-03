# Design Document

## Overview

This design covers the full evolution of the chess engine across 10 phases, from bug fixes through neural network evaluation. The architecture is designed to be incrementally refactored — early phases fix bugs and clean up the code, middle phases add tests and features, and later phases introduce ML/NN capabilities on top of a solid foundation.

The key architectural principle is separation of concerns: the Board class currently does everything, and the refactoring progressively extracts Search, Evaluator, TranspositionTable, PrincipalVariation, and TimeManager into independent, testable components.

## Current Architecture

```
┌─────────────────────────────────────────────────┐
│                   Board (God Object)             │
│                                                  │
│  Game State: bitboards_, board_array_, irrev_    │
│  Search: alphabeta, negamax, minimax, quiesce    │
│  Evaluation: evaluate(), PIECE_SQUARE tables     │
│  Hash: probe_hash, record_hash                   │
│  PV: store_pv_move, print_pv, score_pv_move      │
│  Time: is_search_time_over, should_stop_search   │
│  Counters: nodes_visited_, searched_moves_, etc.  │
└─────────────────────────────────────────────────┘
         ▲                    ▲
         │                    │
┌────────┴───────┐  ┌────────┴────────┐
│  MoveGenerator │  │     Xboard      │
│  (static)      │  │  (200-line      │
│                │  │   strcmp chain)  │
└────────────────┘  └─────────────────┘

Global State:
  - pv_table[MAX_SEARCH_PLY * MAX_SEARCH_PLY]  (global)
  - pv_length[MAX_SEARCH_PLY]                   (global)
  - hash_table[HASH_TABLE_SIZE]                 (global)
```

## Target Architecture (Post Phase 2)

```
┌──────────────────────────────────────────────────────────────┐
│                        Engine                                 │
│                                                               │
│  ┌─────────┐  ┌──────────┐  ┌───────────┐  ┌─────────────┐ │
│  │  Board   │  │  Search  │  │ Evaluator │  │ TimeManager │ │
│  │          │  │          │  │(interface)│  │             │ │
│  │ bitboards│  │ alphabeta│  │           │  │ allocate()  │ │
│  │ do_move  │  │ negamax  │  │ evaluate()│  │ is_over()   │ │
│  │ undo_move│  │ minimax  │  │           │  │             │ │
│  │          │  │ quiesce  │  ├───────────┤  └─────────────┘ │
│  └─────────┘  │          │  │HandCrafted│                    │
│               │ ┌──────┐ │  │Evaluator  │  ┌─────────────┐ │
│               │ │  PV  │ │  ├───────────┤  │   Protocol  │ │
│               │ └──────┘ │  │   NNUE    │  │  (abstract) │ │
│               │ ┌──────┐ │  │ Evaluator │  ├─────────────┤ │
│               │ │  TT  │ │  └───────────┘  │   Xboard    │ │
│               │ └──────┘ │                  ├─────────────┤ │
│               └──────────┘                  │    UCI      │ │
│                                             └─────────────┘ │
└──────────────────────────────────────────────────────────────┘
```

## Phase 1 Design: Bug Fixes & Correctness Hardening

### 1.1 TT Persistence Across Iterative Deepening (Req 41)

**Current:** `Board::search()` calls `reset_hash_table()` at the start.

**Fix:** Remove `reset_hash_table()` from `search()`. Move TT clearing to `Xboard::setup()` (on "new" command) and future `ucinewgame` handler.

**Files changed:** `source/Board.cpp` (search()), `source/Xboard.cpp` (setup(), "new" handler)

### 1.2 Aspiration Window follow_pv_ Reset (Req 42)

**Current:** When aspiration window fails, re-search happens without resetting `follow_pv_`.

**Fix:** Add `follow_pv_ = 1;` before the full-window re-search in `Board::search()`.

```cpp
if ((value <= alpha) || (value >= beta))
{
    alpha = -MAX_SCORE;
    beta = MAX_SCORE;
    follow_pv_ = 1;  // ADD THIS
    value = alphabeta(alpha, beta, current_depth, IS_PV, DO_NULL);
}
```

**Files changed:** `source/Board.cpp` (search())

### 1.3 Twofold Repetition During Search (Req 43)

**Current:** `is_draw()` always uses threefold (repetition_count > 1).

**Fix:** Add a `bool in_search` parameter to `is_draw()`. When `in_search == true`, use `repetition_count > 0` (twofold). AlphaBeta and Quiesce pass `true`; root-level calls pass `false`.

**Files changed:** `source/Board.cpp` (is_draw()), `source/Board.h`, `source/AlphaBeta.cpp`, `source/Quiesce.cpp`

### 1.4 Evaluate Sign Convention Helper (Req 44)

**Current:** NegaMax and Quiesce manually compute `who2move * evaluate()`.

**Fix:** Add `int Board::side_relative_eval()` that encapsulates the flip:

```cpp
int Board::side_relative_eval()
{
    int who2move = (side_to_move() == WHITE) ? 1 : -1;
    return who2move * evaluate();
}
```

Replace all `who2move * evaluate()` call sites with `side_relative_eval()`.

**Files changed:** `source/Board.h`, `source/Board.cpp`, `source/NegaMax.cpp`, `source/Quiesce.cpp`

### 1.5 Adaptive Null Move Reduction (Req 45)

**Current:** Fixed `R = 2` in AlphaBeta.

**Fix:** Replace with:
```cpp
int R = (depth > 6) ? 3 : 2;
```

Also add zugzwang guard: skip null move when side has only king + pawns.

**Files changed:** `source/AlphaBeta.cpp`

### 1.6 SEE Non-Capture Handling & Stale TODOs (Req 46)

**Fix:** Add early return at top of `MoveGenerator::see()`:
```cpp
if (!is_capture(move) && !is_ep_capture(move)) return 0;
```

Remove stale TODO comments about en-passant and promotions.

**Files changed:** `source/See.cpp`

### 1.7 Remove Redundant NDEBUG Guards (Req 47)

**Fix:** Find-and-replace all instances of:
```cpp
#ifndef NDEBUG
    assert(expr);
#endif
```
with just `assert(expr);`

**Files changed:** `source/Board.cpp`, `source/MoveList.h`, `source/See.cpp`, and any others found during audit


## Phase 2 Design: Code Quality & Architecture Refactoring

### 2.1 Board Class Decomposition (Req 34, 35)

Extract the following classes from Board:

#### Search class (`source/Search.h`, `source/Search.cpp`)
```cpp
class Search {
public:
    Search(Board& board, Evaluator& eval, TranspositionTable& tt, TimeManager& tm);
    Move_t search(int depth, int search_time = DEFAULT_SEARCH_TIME,
                  int max_nodes = -1, bool xboard = false);
    int alphabeta(int alpha, int beta, int depth, int is_pv, int can_null);
    int negamax(int depth);
    int minimax(int depth, bool maximizing_player);
    Move_t minimax_root(int depth, bool maximizing_player);
    Move_t negamax_root(int depth);
    int quiesce(int alpha, int beta);

private:
    Board& board_;
    Evaluator& eval_;
    TranspositionTable& tt_;
    TimeManager& tm_;
    PrincipalVariation pv_;  // owns PV state
    int search_ply_;
    int max_search_ply_;
    int nodes_visited_;
    int searched_moves_;
    int follow_pv_;
    Move_t search_best_move_;
    int search_best_score_;
};
```

#### Evaluator interface (`source/Evaluator.h`)
```cpp
class Evaluator {
public:
    virtual ~Evaluator() = default;
    virtual int evaluate(const Board& board) = 0;
    virtual int side_relative_eval(const Board& board) = 0;
};

class HandCraftedEvaluator : public Evaluator {
public:
    int evaluate(const Board& board) override;
    int side_relative_eval(const Board& board) override;
};
```

#### TranspositionTable class (`source/TranspositionTable.h`, `source/TranspositionTable.cpp`)
```cpp
class TranspositionTable {
public:
    TranspositionTable(int size = HASH_TABLE_SIZE);
    void clear();
    int probe(U64 hash, int depth, int alpha, int beta, Move_t& best_move);
    void record(U64 hash, int depth, int val, int flags, Move_t best_move);
private:
    std::vector<HASHE> table_;
};
```

#### PrincipalVariation class (refactor existing `source/PrincipalVariation.h`)
```cpp
class PrincipalVariation {
public:
    void reset();
    void store_move(int search_ply, Move_t move);
    void score_move(MoveList& list, int search_ply, Move_t best_move, int& follow_pv);
    void print(Board& board);
    Move_t get_best_move() const;
    int length() const;
private:
    Move_t pv_table_[MAX_SEARCH_PLY * MAX_SEARCH_PLY];
    int pv_length_[MAX_SEARCH_PLY];
};
```

#### TimeManager class (`source/TimeManager.h`)
```cpp
class TimeManager {
public:
    void start(int search_time, int max_nodes = -1);
    bool is_time_over(int nodes_visited) const;
    bool should_stop(int nodes_visited) const;
private:
    clock_t start_time_;
    int search_time_;
    int max_nodes_;
};
```

### 2.2 Modern C++ Cleanup (Req 36, 40)

#### Types.h transformation:
```cpp
// Before:
typedef unsigned long long U64;
typedef unsigned char U8;
typedef unsigned int U16;  // BUG: actually 32 bits
typedef unsigned int U32;

// After:
#include <cstdint>
using U64 = uint64_t;
using U8  = uint8_t;
using U16 = uint16_t;
using U32 = uint32_t;
```

#### Hash.h transformation:
```cpp
// Before:
#define HASH_TABLE_SIZE (1024*1024)
#define HASH_EXACT 0
#define HASH_ALPHA 1
#define HASH_BETA  2
typedef struct HASHE_s { ... } HASHE;

// After:
constexpr int HASH_TABLE_SIZE = 1024 * 1024;
enum class HashFlag : int { EXACT = 0, ALPHA = 1, BETA = 2 };
struct HASHE { U64 key; int depth; int flags; int value; Move_t best_move; };
```

#### Common.h transformation:
```cpp
// Remove: using namespace std;
// Remove: #define DEBUG
// Add explicit std:: prefixes throughout codebase
// Control DEBUG via CMake: target_compile_definitions(blunder PRIVATE $<$<CONFIG:Debug>:DEBUG>)
```

### 2.3 Xboard Refactoring (Req 37)

Replace the if/strcmp chain with a dispatch table:

```cpp
class Xboard {
    using Handler = std::function<void(const std::string& args)>;
    std::unordered_map<std::string, Handler> commands_;

    void init_commands() {
        commands_["quit"]     = [this](auto&) { running_ = false; };
        commands_["force"]    = [this](auto&) { engine_side_ = STM_NONE; };
        commands_["new"]      = [this](auto&) { handle_new(); };
        commands_["setboard"] = [this](auto& args) { handle_setboard(args); };
        commands_["usermove"] = [this](auto& args) { handle_usermove(args); };
        // ... etc
    }

    void run() {
        std::string line;
        while (running_ && std::getline(std::cin, line)) {
            auto cmd = extract_command(line);
            auto it = commands_.find(cmd);
            if (it != commands_.end()) {
                it->second(extract_args(line));
            }
        }
    }
};
```

Eliminate `goto no_ponder` with a `bool skip_ponder` flag.
Replace `sscanf` with `std::istringstream`.
Remove `#define _CRT_SECURE_NO_WARNINGS`.

### 2.4 Move Encoding (Req 38)

This is a large refactor best done after the other cleanups. The approach:
1. Create a `Move` class that wraps the U32 internally
2. Add member functions for `from()`, `to()`, `captured()`, etc.
3. Separate score into a `ScoredMove` wrapper used only in MoveList
4. Gradually migrate call sites

### 2.5 Error Handling (Req 39)

Define a simple logging approach:
- `assert()` for programmer errors (invariant violations)
- Return `std::optional` or error codes from Parser functions
- Remove all `#ifndef NDEBUG` guards around single asserts
- Replace `cerr <<` in ValidateMove with a consistent logging function


## Phase 3 Design: Algorithm Correctness Validation (Req 1–19)

### Test Framework

Use the existing Catch2 test framework (already in `test/`). Add new test files:

- `test/source/TestAlgorithmEquivalence.cpp` — Reqs 1, 2 (MiniMax ↔ NegaMax ↔ AlphaBeta)
- `test/source/TestBoardInvariance.cpp` — Reqs 3, 4, 5, 13 (do_move/undo_move, null move, hash)
- `test/source/TestDrawDetection.cpp` — Req 15 (fifty-move, threefold)
- `test/source/TestEvaluation.cpp` — Req 16 (symmetry, sign convention)
- `test/source/TestSearchProperties.cpp` — Reqs 7, 8, 9, 12, 14, 17, 18 (quiesce, mate, PV, depth monotonicity, aspiration, global reset, overflow)

### Test Strategy

Tests use a set of well-known FEN positions covering:
- Starting position
- Middlegame positions with tactical themes
- Endgame positions (KRK, KQK)
- Positions with en-passant, castling, promotions
- Checkmate and stalemate positions
- Positions from the WAC (Win At Chess) test suite already in `test/data/`

For each position, tests verify the correctness properties at multiple depths (typically 1–4 to keep test runtime reasonable).

### Correctness Properties

Each requirement maps to one or more testable properties:

| Requirement | Property | Test Approach |
|---|---|---|
| Req 1 | minimax_score == negamax_score | Run both on same FEN at same depth, compare |
| Req 2 | alphabeta_full_window == negamax | Run both, compare scores |
| Req 3 | board_after_search == board_before | Snapshot board state, run search, compare |
| Req 4 | do_move + undo_move == identity | For each legal move, snapshot → do → undo → compare |
| Req 5 | incremental_hash == full_recompute | After do_move, compare get_hash() vs get_zobrist_key() |
| Req 13 | null_do + null_undo == identity | Snapshot → do_null → undo_null → compare |
| Req 15 | fifty_move at 100, threefold at 3 | Set up positions at boundary, verify is_draw() |
| Req 16 | eval(pos) == -eval(mirror(pos)) | Mirror position, compare evaluations |

## Phase 4 Design: Performance & Efficiency (Req 20–22)

### 4.1 Leaf Node Optimization (Req 20)

In `MiniMax.cpp` and `NegaMax.cpp`, move the depth check before `is_game_over()`:

```cpp
// Before:
if (is_game_over()) { ... }
if (depth == 0) { return evaluate(); }

// After:
if (depth == 0) { return evaluate(); }  // or side_relative_eval()
if (is_game_over()) { ... }
```

This eliminates move generation at every leaf node.

### 4.2 Killer Moves (Req 21)

Add a killer move table to the Search class:
```cpp
Move_t killers_[MAX_SEARCH_PLY][2];  // 2 killer moves per ply
```

On beta cutoff of a quiet move, store it as a killer. During move scoring, give killers a score between captures and quiet moves.

### 4.3 History Heuristic (Req 21)

Add a history table:
```cpp
int history_[2][64][64];  // [side][from][to]
```

Increment on cutoff, use as tiebreaker for quiet move ordering.

### 4.4 Late Move Reductions (Req 22)

In AlphaBeta, after the first few moves:
```cpp
if (i >= 4 && depth >= 3 && !is_capture(move) && !is_promotion(move) && !in_check) {
    int reduction = 1;  // or use a log-based formula
    value = -alphabeta(-alpha - 1, -alpha, depth - 1 - reduction, NO_PV, DO_NULL);
    if (value > alpha) {
        value = -alphabeta(-beta, -alpha, depth - 1, IS_PV, DO_NULL);  // re-search
    }
} else {
    // normal search
}
```

## Phase 5 Design: Debugging & Observability (Req 23–24)

### 5.1 Search Statistics (Req 23)

Add a `SearchStats` struct to the Search class:
```cpp
struct SearchStats {
    int nodes_visited = 0;
    int hash_hits = 0;
    int hash_probes = 0;
    int beta_cutoffs = 0;
    int total_moves_searched = 0;
    clock_t start_time;

    double nps() const;
    double hash_hit_rate() const;
    double cutoff_rate() const;
    double branching_factor(int depth) const;
    void reset();
};
```

### 5.2 Perft Regression Tests (Req 24)

Expand `test/source/TestPerft.cpp` with known-correct values:

| Position | Depth | Expected Nodes |
|---|---|---|
| Starting position | 1 | 20 |
| Starting position | 2 | 400 |
| Starting position | 3 | 8,902 |
| Starting position | 4 | 197,281 |
| Starting position | 5 | 4,865,609 |
| Kiwipete | 1–4 | published values |
| Position 3 (en-passant) | 1–4 | published values |
| Position 4 (promotions) | 1–4 | published values |
| Position 5 (castling) | 1–4 | published values |

## Phase 6 Design: Opening Book Support (Req 25–26)

### 6.1 Polyglot Book Reader

New files: `source/Book.h`, `source/Book.cpp`

```cpp
class Book {
public:
    bool open(const std::string& path);
    bool has_move(U64 polyglot_key) const;
    Move_t get_move(U64 polyglot_key, const Board& board) const;
    void close();
private:
    struct PolyglotEntry {
        U64 key;
        U16 move;
        U16 weight;
        U32 learn;
    };
    std::vector<PolyglotEntry> entries_;
    U64 polyglot_hash(const Board& board) const;
};
```

The Polyglot format uses its own Zobrist key scheme. If the engine's keys don't match, `polyglot_hash()` computes the Polyglot-compatible hash from the board state.

### 6.2 Integration with Search

In `Xboard::search_best_move()` (and future UCI handler):
```cpp
if (book_.has_move(polyglot_key)) {
    *move = book_.get_move(polyglot_key, board_);
    return 0;
}
// else fall through to normal search
```

## Phase 7 Design: Engine Strength Features (Req 27–28)

### 7.1 Syzygy Tablebases (Req 27)

Integrate the Fathom library (MIT license) for Syzygy probing:
- Add as a git submodule or vendored dependency
- Probe WDL at root and during search when piece count ≤ 6
- Use DTZ (Distance To Zeroing) at root for optimal play

### 7.2 Pondering — Think on Opponent's Time (Req 50)

The Xboard loop already has scaffolding for pondering: `ponder_` flag, `easy`/`hard` handlers, `ponder_move` tracking, and `make_move`/`un_make` around `ponder_until_input()`. The stub is currently a no-op.

#### Platform-Specific Non-Blocking Input

Pondering requires detecting stdin input without blocking:

```cpp
// source/InputDetect.h
#ifndef INPUTDETECT_H
#define INPUTDETECT_H

/// Returns true if there is data waiting on stdin (non-blocking).
bool input_available();

#endif
```

```cpp
// source/InputDetect.cpp
#include "InputDetect.h"

#ifdef _WIN32
#include <windows.h>
#include <io.h>

bool input_available()
{
    // Check if stdin is a pipe (redirected) or console
    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
    DWORD avail = 0;
    if (GetFileType(h) == FILE_TYPE_PIPE)
    {
        PeekNamedPipe(h, nullptr, 0, nullptr, &avail, nullptr);
        return avail > 0;
    }
    // Console: check for pending input events
    INPUT_RECORD rec;
    DWORD count = 0;
    while (PeekConsoleInput(h, &rec, 1, &count) && count > 0)
    {
        if (rec.EventType == KEY_EVENT && rec.Event.KeyEvent.bKeyDown)
        {
            return true;
        }
        ReadConsoleInput(h, &rec, 1, &count);  // discard non-key events
    }
    return false;
}

#else
#include <sys/select.h>
#include <unistd.h>

bool input_available()
{
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    struct timeval tv = {0, 0};
    return select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv) > 0;
}
#endif
```

#### Search Abort Mechanism

The Search class needs a way to be interrupted externally:

```cpp
// In Search.h:
void set_abort(bool a) { abort_ = a; }
bool is_aborted() const { return abort_; }

// In alphabeta/quiesce, check periodically (every 2048 nodes):
if (abort_) return 0;
```

#### Ponder Flow

```
Engine makes move → extract ponder_move from PV[1]
  → make_move(ponder_move)
  → search_.set_abort(false)
  → start search in ponder_until_input()
  → periodically check input_available()
  → if input arrives:
      → search_.set_abort(true)  // stop search
      → read the input line
      → if opponent played ponder_move: "ponder hit" — use partial results
      → else: "ponder miss" — discard results
  → un_make(ponder_move)
```

Pondering is only supported in Xboard mode (and future UCI). Interactive mode does not ponder.

### 7.3 Time Management (Req 28)

Replace the simple `time_left / 40 + inc / 2` formula with:

```cpp
class TimeManager {
    int allocate(int time_left, int inc, int moves_to_go, bool best_move_changed) {
        int base = time_left / std::max(moves_to_go, 20) + inc * 3 / 4;
        if (best_move_changed) base = base * 3 / 2;  // more time if unstable
        return std::min(base, time_left - 500);  // safety margin
    }
};
```

## Phase 8 Design: Neural Network / ML (Req 29–32)

### 8.1 NNUE Evaluation (Req 29)

Architecture: HalfKP (768 → 256 → 32 → 32 → 1), similar to Stockfish NNUE.

```cpp
class NNUEEvaluator : public Evaluator {
public:
    bool load(const std::string& weights_path);
    int evaluate(const Board& board) override;
    int side_relative_eval(const Board& board) override;

    // Incremental updates
    void push();   // save accumulator state
    void pop();    // restore accumulator state
    void add_piece(U8 piece, int square);
    void remove_piece(U8 piece, int square);

private:
    alignas(64) int16_t accumulator_[2][256];  // [perspective][features]
    // Network weights loaded from file
};
```

Integrate with Board::do_move/undo_move via observer pattern or direct calls.

### 8.2 MCTS (Req 31) — Current Implementation

The current MCTS implementation uses the handcrafted evaluator for leaf evaluation
and uniform priors for move selection. This is functional but does not yet implement
the AlphaZero feedback loop where the neural network drives the search.

```cpp
struct MCTSNode {
    Move_t move;
    MCTSNode* parent;
    std::vector<std::unique_ptr<MCTSNode>> children;
    int visits = 0;
    double total_value = 0.0;
    double prior = 0.0;  // currently uniform; will come from policy network

    double ucb(double c_puct) const;
};

class MCTS {
public:
    MCTS(Board& board, Evaluator& eval);
    Move_t search(int num_simulations);
    MCTSNode* search_return_root(int num_simulations);
private:
    MCTSNode* select(MCTSNode* root);
    void expand(MCTSNode* node);
    double simulate(MCTSNode* node);  // currently uses handcrafted eval
    void backpropagate(MCTSNode* node, double value);
};
```

### 8.3 Training Pipeline — Current State (Req 30, 32)

Two data generation modes are implemented:

**AlphaBeta self-play** (Task 41): Fixed-size binary format.
```
[768 floats: features][1 float: search score][1 float: game result]
```

**MCTS self-play** (Task 43): Variable-length binary format with policy targets.
```
[768 floats: features][1 int: num_moves][num_moves floats: policy][num_moves ints: move_indices][1 float: value]
```

The Python training script (`scripts/train_nnue.py`) supports both formats via
`--format alphabeta|mcts`. However, the current NNUE architecture is a single-head
value network (768→256→32→32→1) that only uses the value target. The policy targets
from MCTS self-play are stored but not yet used for training.

### 8.4 AlphaZero-Style Architecture — What's Missing (Req 31, 32)

The full AlphaZero approach requires a self-improving feedback loop where the neural
network and MCTS search reinforce each other. Here's how it works and what we need:

#### Background: How AlphaZero Works

In AlphaZero, a single neural network serves two purposes during MCTS:

1. **Policy head** — Given a board position, output a probability distribution over
   all legal moves. This tells MCTS which moves are worth exploring. Without a policy
   head, MCTS explores all moves equally (uniform priors), wasting simulations on
   obviously bad moves like hanging your queen.

2. **Value head** — Given a board position, output a single number estimating who is
   winning (from the side-to-move's perspective). This replaces random rollouts or
   handcrafted evaluation at leaf nodes.

The training loop is:
```
┌─────────────────────────────────────────────────────────┐
│  1. Self-play: MCTS uses current network to play games  │
│     - Policy head → move priors for tree exploration     │
│     - Value head → leaf node evaluation                  │
│     - Record (position, visit_distribution, game_result) │
│                          ↓                               │
│  2. Train: Update network to match MCTS's findings       │
│     - Policy head learns to predict visit distribution   │
│     - Value head learns to predict game outcome          │
│                          ↓                               │
│  3. Reload: Engine loads new weights                     │
│                          ↓                               │
│  4. Repeat from step 1 with stronger network             │
└─────────────────────────────────────────────────────────┘
```

The key insight: MCTS with a good policy head is much stronger than MCTS with
uniform priors, because it focuses simulations on promising moves. And the policy
head gets better by learning from MCTS's visit distributions, which reflect what
moves are actually good after deep search. This creates a virtuous cycle.

#### What We Have vs What We Need

| Component | Current State | AlphaZero Target |
|---|---|---|
| MCTS search | ✅ Working with UCB1 | Same, but uses NN priors |
| Leaf evaluation | ❌ Handcrafted eval | NN value head |
| Move priors | ❌ Uniform (1/N) | NN policy head |
| Network arch | ❌ Value-only (768→256→32→32→1) | Dual-head (shared trunk → policy + value) |
| Self-play data | ✅ Records policy + value targets | Same |
| Training script | ❌ Trains value head only | Trains both heads |
| Iterative loop | ❌ Manual one-shot | Automated generate→train→reload cycle |

#### 8.4.1 Dual-Head Network Architecture

Replace the single-head NNUE with a dual-head network that shares a feature
transformer but has separate output heads:

```
Input: 768 HalfKP features
         │
    ┌────┴────┐
    │  Shared  │
    │  Trunk   │  768 → 256 → 128 (shared representation)
    └────┬────┘
         │
    ┌────┴────────────┐
    │                  │
┌───┴───┐        ┌────┴────┐
│ Value  │        │ Policy  │
│ Head   │        │  Head   │
│128→32→1│        │128→N_mov│
└───┬────┘        └────┬────┘
    │                   │
  float              float[N]
  (who's             (probability per
   winning)           legal move)
```

C++ interface:

```cpp
class DualHeadNetwork {
public:
    bool load(const std::string& weights_path);

    /// Run forward pass, returning value and policy for all legal moves.
    /// @param board     Current position (for feature extraction)
    /// @param moves     Legal moves in this position
    /// @param policy    Output: probability for each move (same order as moves)
    /// @param value     Output: position evaluation [-1, +1]
    void evaluate(const Board& board,
                  const std::vector<Move_t>& moves,
                  std::vector<float>& policy,
                  float& value);

    /// Value-only evaluation (for use as Evaluator drop-in)
    int evaluate_value(const Board& board);

private:
    // Shared trunk weights
    float w_trunk1_[768 * 256];   // 768 → 256
    float b_trunk1_[256];
    float w_trunk2_[256 * 128];   // 256 → 128
    float b_trunk2_[128];

    // Value head weights
    float w_value1_[128 * 32];    // 128 → 32
    float b_value1_[32];
    float w_value2_[32];          // 32 → 1
    float b_value2_;

    // Policy head weights
    float w_policy_[128 * 4096];  // 128 → 4096 (64*64 from-to pairs)
    float b_policy_[4096];

    void extract_features(const Board& board, float features[768]);
    void forward_trunk(const float features[768], float trunk_out[128]);
};
```

The policy head outputs logits for all possible from-to square pairs (64×64 = 4096).
For a given position, we mask to only the legal moves and apply softmax to get
probabilities. This is simpler than move-indexed output and handles promotions
by encoding them as distinct from-to pairs.

#### 8.4.2 MCTS with Neural Network Integration

Modify MCTS to accept a DualHeadNetwork and use it for:
- **Expansion**: When expanding a node, call the policy head to set `prior` on
  each child node instead of using uniform 1/N.
- **Simulation**: When evaluating a leaf, call the value head instead of the
  handcrafted evaluator. No random rollouts needed.

```cpp
class MCTS {
public:
    // New constructor that accepts the dual-head network
    MCTS(Board& board, DualHeadNetwork& network);

    // Existing constructor for handcrafted eval (backward compatible)
    MCTS(Board& board, Evaluator& eval);

private:
    void expand(MCTSNode* node) {
        auto moves = generate_legal_moves();
        if (network_) {
            // Use policy head for priors
            std::vector<float> policy;
            float value;
            network_->evaluate(board_, moves, policy, value);
            for (int i = 0; i < moves.size(); i++) {
                auto child = make_node(moves[i]);
                child->prior = policy[i];
                node->children.push_back(child);
            }
        } else {
            // Fallback: uniform priors
            float uniform = 1.0f / moves.size();
            for (auto& m : moves) {
                auto child = make_node(m);
                child->prior = uniform;
                node->children.push_back(child);
            }
        }
    }

    double simulate(MCTSNode* node) {
        if (network_) {
            // Use value head
            float value;
            std::vector<float> policy;  // unused here
            auto moves = generate_legal_moves();
            network_->evaluate(board_, moves, policy, value);
            return value;
        } else {
            // Fallback: handcrafted eval normalized to [-1, +1]
            return eval_->side_relative_eval(board_) / 600.0;
        }
    }

    DualHeadNetwork* network_ = nullptr;  // null = use handcrafted eval
    Evaluator* eval_ = nullptr;           // fallback evaluator
};
```

#### 8.4.3 Dual-Head Training Script

Update `scripts/train_nnue.py` (or create a new `scripts/train_alphazero.py`) to
train both heads simultaneously:

```python
class DualHeadNNUE(nn.Module):
    def __init__(self):
        super().__init__()
        # Shared trunk
        self.trunk1 = nn.Linear(768, 256)
        self.trunk2 = nn.Linear(256, 128)
        # Value head
        self.value1 = nn.Linear(128, 32)
        self.value2 = nn.Linear(32, 1)
        # Policy head
        self.policy = nn.Linear(128, 4096)  # 64*64 from-to

    def forward(self, features, legal_move_mask=None):
        # Shared trunk
        x = F.relu(self.trunk1(features))
        x = F.relu(self.trunk2(x))
        # Value head: tanh to [-1, +1]
        v = F.relu(self.value1(x))
        v = torch.tanh(self.value2(v))
        # Policy head: masked softmax over legal moves
        p = self.policy(x)
        if legal_move_mask is not None:
            p = p.masked_fill(~legal_move_mask, float('-inf'))
        p = F.softmax(p, dim=-1)
        return v, p
```

Loss function combines both heads:
```python
loss = mse_loss(predicted_value, game_outcome) \
     + cross_entropy_loss(predicted_policy, mcts_visit_distribution)
```

The value loss teaches the network to predict who wins. The policy loss teaches
it to predict which moves MCTS found most promising (the visit distribution).

#### 8.4.4 Iterative Training Loop

An orchestration script that automates the AlphaZero cycle:

```bash
#!/bin/bash
# scripts/alphazero_loop.sh
ITERATIONS=10
GAMES_PER_ITER=200
SIMS=400
WEIGHTS="weights/alphazero_current.bin"

for i in $(seq 1 $ITERATIONS); do
    echo "=== Iteration $i ==="

    # 1. Self-play with current network
    ./blunder --selfplay --mcts \
        --nnue $WEIGHTS \
        --selfplay-games $GAMES_PER_ITER \
        --mcts-simulations $SIMS \
        --selfplay-output training_iter${i}.bin

    # 2. Train on accumulated data
    python scripts/train_alphazero.py \
        --input training_iter${i}.bin \
        --output $WEIGHTS \
        --format mcts \
        --epochs 20

    # 3. Weights are automatically reloaded on next iteration
    # 4. Optionally: evaluate against previous version
    python scripts/compare_nnue_vs_handcrafted.py \
        --nnue $WEIGHTS --games 50
done
```

Each iteration produces a stronger network because:
- Better policy → MCTS explores better moves → higher quality training data
- Better value → MCTS evaluates positions more accurately → better move selection
- More data accumulates across iterations → network generalizes better

## Phase 9 Design: Protocol & Interoperability (Req 33)

### UCI Protocol Handler

New files: `source/UCI.h`, `source/UCI.cpp`

```cpp
class UCI {
public:
    UCI(Board& board, Search& search, Book& book);
    void run();
private:
    void handle_uci();
    void handle_isready();
    void handle_ucinewgame();
    void handle_position(const std::string& args);
    void handle_go(const std::string& args);
    void handle_stop();
    void handle_setoption(const std::string& args);

    std::unordered_map<std::string, Handler> commands_;
};
```

### Protocol Auto-Detection

In `main.cpp`:
```cpp
std::string first_line;
std::getline(std::cin, first_line);
if (first_line == "uci") {
    UCI uci(board, search, book);
    uci.run();
} else {
    Xboard xboard;
    xboard.run();  // replay first_line
}
```

## Phase 10 Design: Strength Regression Testing (Req 48, 49)

### 10.1 Automated Cutechess SPRT Script (Req 48)

Enhance the existing `scripts/run-cutechess.sh` into a more robust, configurable regression testing tool:

```bash
scripts/regression-test.sh [options]
  --baseline <path>    Path to baseline engine binary (default: build/rel/blunder)
  --candidate <path>   Path to candidate engine binary (default: build/dev-mingw/blunder)
  --tc <time_control>  Time control (default: 10+0.1)
  --book <path>        Opening book (default: books/i-gm1950.bin)
  --rounds <n>         Max rounds (default: 2500)
  --concurrency <n>    Parallel games (default: 4)
  --elo0 <n>           SPRT null hypothesis (default: 0)
  --elo1 <n>           SPRT alternative hypothesis (default: 10)
```

The script:
1. Validates both engine binaries exist and are executable
2. Creates a timestamped output directory under `scripts/output/`
3. Runs cutechess-cli with SPRT and captures the log
4. Parses the final SPRT result (H0 accepted = regression, H1 accepted = improvement, inconclusive)
5. Exits with code 0 on pass (H1 or inconclusive), non-zero on regression (H0)

### 10.2 WAC/STS Solve Rate Tracking (Req 49)

Add a test target or script that:
1. Runs the engine on each WAC position at a fixed depth (e.g., depth 8) or time (e.g., 1s)
2. Compares the engine's best move against the known solution
3. Reports solve count / total and percentage
4. Similarly for STS: runs each position, scores according to the STS scoring system
5. Logs results with git commit hash for historical tracking

The existing `test/data/test-positions/WAC.epd` and `STS1-STS15_LAN_v6.epd` files provide the test data.

## Testing Strategy

All phases include tests. The test structure:

```
test/source/
├── TestBoard.cpp              (existing)
├── TestMove.cpp               (existing)
├── TestMoveGenerator.cpp      (existing)
├── TestParser.cpp             (existing)
├── TestPerft.cpp              (existing, expanded in Phase 5)
├── TestSearch.cpp             (existing)
├── TestSee.cpp                (existing)
├── TestZobrist.cpp            (existing)
├── TestAlgorithmEquivalence.cpp  (Phase 3 — Req 1, 2)
├── TestBoardInvariance.cpp       (Phase 3 — Req 3, 4, 5, 13)
├── TestDrawDetection.cpp         (Phase 3 — Req 15)
├── TestEvaluation.cpp            (Phase 3 — Req 16)
├── TestSearchProperties.cpp      (Phase 3 — Req 7, 8, 9, 12, 14, 17, 18)
├── TestTranspositionTable.cpp    (Phase 3 — Req 10)
├── TestFenRoundTrip.cpp          (Phase 3 — Req 11)
├── TestBook.cpp                  (Phase 6 — Req 25, 26)
└── TestUCI.cpp                   (Phase 9 — Req 33)
```

## Performance Budget & Regression Policy

A core tension in chess engine development is that making the engine "smarter" (better evaluation, deeper search techniques) can also make it slower per node. Every change must be evaluated against this tradeoff.

### Guiding Principle

A change is only beneficial if the strength gained from better decisions outweighs the strength lost from reduced search speed. A 10% slower evaluation that improves positional understanding by 50 Elo is a win. A 10% slower evaluation that gains 5 Elo is probably not.

### Performance Baselines

Before and after any change that touches the hot path (evaluate, move generation, search loop, do_move/undo_move), measure:
- **NPS** (nodes per second) on a fixed position at a fixed depth
- **Perft speed** for move generation changes (starting position depth 5 is a good benchmark)
- **Search depth reached** in a fixed time budget (e.g., 5 seconds on the starting position)

### Hot Path Rules

These functions are called millions of times per search. Changes here must be zero-cost or justified by measurable strength gains:
- `evaluate()` / `side_relative_eval()` — called at every leaf node and quiescence node
- `do_move()` / `undo_move()` — called at every node in the search tree
- `MoveGenerator::add_all_moves()` — called at every non-leaf node
- `alphabeta()` / `quiesce()` — the search loop itself

### Architecture Overhead

The Phase 2 decomposition (extracting Search, Evaluator, TranspositionTable, etc.) introduces virtual function calls and indirection. This is acceptable because:
- Virtual dispatch overhead is negligible compared to the work done per node
- The Evaluator interface enables swapping HandCrafted ↔ NNUE without code changes
- But: avoid adding virtual calls inside tight inner loops (e.g., per-square evaluation loops)

### Memory Considerations

- The TranspositionTable is now owned by Board, meaning each Board instance allocates ~24MB. This is fine for the engine (one Board in play) but wasteful in tests that construct many Boards. Consider making TT a shared/external resource when extracting the Search class.
- NNUE accumulators add memory per search stack frame. Keep accumulator updates incremental to avoid per-node allocation.

## Build System Changes

- Add new source files to `CMakeLists.txt` as they are created
- Add `target_compile_definitions(blunder PRIVATE $<$<CONFIG:Debug>:DEBUG>)` to replace `#define DEBUG`
- Add Fathom as dependency for Syzygy support (Phase 7)
- Add optional PyTorch/ONNX dependency for NNUE inference (Phase 8)
