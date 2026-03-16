# Blunder Engine — Full Code Review

Date: 2026-03-05 (updated)
Previous review: 2026-03-01

This document is a thorough code review of the Blunder chess engine codebase.
Findings are organized by component, with concrete improvement suggestions
prioritized by estimated Elo / performance impact.

The overarching principle: **depth wins**. A simpler, faster engine that
searches 2–3 plies deeper will almost always beat a slower engine with a
fancier eval. Every cycle saved in eval, move generation, or data structure
overhead translates directly into deeper search and stronger play.

Changes since last review (v0.5.0 → v0.5.1):
- UCI protocol implemented (was flagged as missing)
- Benchmark scripts added (run-benchmarks.py, analyze-sts.py)
- Engine-evolution spec fully completed
- All cutechess scripts updated with --protocol flag

---

## 1. Board Representation (Board.h / Board.cpp)

### What's good
- Clean bitboard + mailbox hybrid. Bitboards for attack generation, mailbox
  for quick piece-on-square lookups. Standard and effective.
- Incremental Zobrist hashing in `do_move`/`undo_move` is correct.
- Irreversible state saved/restored via `move_stack_[]` — clean pattern.
- `add_piece`/`remove_piece` correctly update both bitboards and mailbox
  atomically with hash updates.

### Issues and improvements

**[CRITICAL] `do_move` calls `nnue_->refresh()` on every move (full recompute)**

This is the single biggest performance problem in the engine. `refresh()`
iterates all 64 squares and does a 768×256 dot product to rebuild the
accumulator from scratch. The whole point of the incremental NNUE design
(push/pop/add_piece/remove_piece) is to avoid this. Instead, `do_move`
should call `nnue_->remove_piece()` and `nnue_->add_piece()` for only the
pieces that changed (typically 1–3 features per move). This alone could
double your NPS.

```cpp
// Current (slow):
if (nnue_) { nnue_->push(); }
// ... make the move ...
if (nnue_) { nnue_->refresh(*this); }  // O(768 * 256) every node!

// Should be (fast):
if (nnue_) { nnue_->push(); }
// ... for each piece removed/added during the move:
if (nnue_) { nnue_->remove_piece(piece, from); }
if (nnue_) { nnue_->add_piece(piece, to); }
// No refresh needed — accumulator is incrementally correct
```

**[MEDIUM] `pop_count` is a manual loop**

The compiler may or may not optimize this. Use `__builtin_popcountll` (GCC/Clang)
or `_mm_popcnt_u64` (MSVC with SSE4.2) for a single-instruction popcount.
This is called frequently in move generation and eval.

**[MEDIUM] `is_draw` scans the entire game history linearly**

```cpp
for (int i = 0; i < game_ply_; i++) {
    if (irrev_.board_hash == hash_history_[i]) { ... }
}
```

This is O(game_ply) per node. Since the hash can only repeat after a
reversible move, you only need to scan back `half_move_count` entries
(the distance since the last irreversible move). This can be a significant
speedup in long games.

```cpp
// Improved: only scan back to last irreversible move
int start = game_ply_ - irrev_.half_move_count;
if (start < 0) start = 0;
for (int i = start; i < game_ply_; i += 2) { // step by 2: same side to move
    ...
}
```

**[MEDIUM] Board still owns TranspositionTable, Evaluator, and NNUEEvaluator**

Board holds a `shared_ptr<TranspositionTable>`, a `HandCraftedEvaluator`,
and a raw `NNUEEvaluator*`. These should be injected dependencies, not
owned by Board. The `get_evaluator()` method with its NNUE fallback logic
is a leaky abstraction — the caller should decide which evaluator to use.

**[LOW] `Board::reset()` calls `MoveGenerator::init_magic_tables()`**

Magic table init is guarded by a static bool, so it's safe, but it's
conceptually wrong for a Board to initialize global move generator state.
Consider moving this to `main()` or a one-time init function.

**[LOW] `hash_history_` is a fixed array of `MAX_GAME_PLY` (1024) U64s**

This is 8KB per Board object. Fine for a single board, but worth noting
if Board is ever copied frequently (e.g., in SMP search).

---

## 2. Search (Search.h / Search.cpp)

### What's good
- Iterative deepening with aspiration windows — correct and standard.
- PVS (Principal Variation Search) with zero-window re-search — good.
- Null move pruning with adaptive R (2 or 3 based on depth) — reasonable.
- Late Move Reductions (LMR) for quiet non-killer moves — good.
- Killer moves (2 slots per ply) + history heuristic — solid move ordering.
- Instability detection: extends time when best move changes between
  iterations — nice touch.
- Mate distance pruning — correct.
- Pondering support with abort mechanism — well implemented.

### Issues and improvements

**[HIGH] No check extensions**

When the side to move is in check, the search should extend by 1 ply
(i.e., don't decrement depth). This is one of the most impactful search
improvements and is nearly universal in strong engines. Without it, the
engine can miss forced mates and tactical sequences that involve checks.

```cpp
// In alphabeta, after detecting in_check:
int extension = in_check ? 1 : 0;
// Then use (depth - 1 + extension) instead of (depth - 1)
```

**[HIGH] No SEE pruning in quiescence search**

Currently qsearch searches ALL captures. Adding a `see >= 0` filter
(or `see > -threshold`) would prune losing captures and significantly
reduce the qsearch tree. This is standard in strong engines.

```cpp
// In quiesce(), before board_.do_move(move):
if (MoveGenerator::see(board_, move) < 0) continue;
```

**[HIGH] History table overflow**

`history_[side][from][to] += depth * depth` grows without bound. After
many iterations at high depths, values can overflow or become so large
that the scaling in `score_killers` (`h * 73 / (h + 1000)`) saturates.
Most engines age/decay history values periodically (e.g., halve all
entries between iterations, or use a gravity/aging scheme).

**[MEDIUM] `#define NO_PV 0` / `IS_PV 1` / `NO_NULL 0` / `DO_NULL 1`**

These preprocessor defines in Search.h should be `constexpr int` or an
`enum`. Defines pollute the global namespace and don't respect scope.

**[MEDIUM] Redundant `#ifndef NDEBUG` around `assert()`**

In Search.cpp's iterative deepening loop:
```cpp
#ifndef NDEBUG
        assert(value <= MAX_SCORE);
        assert(value >= -MAX_SCORE);
#endif
```
`assert()` already compiles to nothing when `NDEBUG` is defined. The
guard is redundant.

**[MEDIUM] No futility pruning**

At low depths (1–2), if the static eval + a margin is below alpha, quiet
moves are unlikely to raise alpha. Pruning them (futility pruning /
extended futility pruning) can significantly reduce the tree without
losing much accuracy.

**[MEDIUM] No Internal Iterative Deepening (IID)**

When no hash move is available at a PV node, doing a shallow search first
to find a good move to search first can improve move ordering significantly.

**[MEDIUM] LMR conditions could be refined**

Current LMR: `i >= 3 && depth >= 3 && is_quiet && !in_check && !is_killer`.
Consider also:
- Not reducing moves that give check
- Reducing more aggressively for higher move indices (logarithmic reduction)
- Not reducing when the position has a lot of tactical tension

**[MEDIUM] C-style variable declarations at function top**

Throughout Search.cpp: `int i, n, value;` declared at the top of functions.
These should be declared at point of use for clarity and to limit scope.

**[LOW] `clock()` for timing**

`clock()` measures CPU time, not wall time. On multi-threaded systems or
under load, this can be inaccurate. Consider `std::chrono::steady_clock`
for wall-clock timing.

**[LOW] Minimax and negamax_root are dead code in competitive play**

These are useful for testing/debugging but add maintenance burden. Consider
gating them behind a debug/test flag or moving to test utilities.

---

## 3. Move Ordering (MoveGenerator::score_moves / Search::score_killers)

### What's good
- MVV-LVA for capture ordering — standard and correct.
- SEE-based bad capture demotion (score = 0 for SEE < 0) — good.
- Killer moves scored at 90/80, above quiet moves (5) — reasonable.
- History heuristic mapped to 6–79 range — avoids colliding with killers.
- PV move scored at 128, hash move at 255 — correct priority ordering.

### Issues and improvements

**[MEDIUM] SEE only called for "upward" captures**

```cpp
if ((SEE_PIECE_VALUE[piece >> 1] - SEE_PIECE_VALUE[captured >> 1]) >= 0)
```

This means SEE is only called when the attacker is worth >= the victim
(e.g., queen takes pawn). But "downward" captures (pawn takes queen) are
always scored by MVV-LVA without SEE verification. This is fine for
ordering, but means you never identify bad "downward" captures (e.g.,
pawn takes defended queen where the pawn is also lost). In practice this
is rare, so the impact is low.

**[MEDIUM] Promotion scoring**

Promotions get `score + 70`, which puts a quiet promotion at 75 (5 + 70).
This is below killer moves (80/90) and well below hash/PV moves. A queen
promotion should probably be scored higher than killers, especially
capture-promotions.

**[LOW] `score_moves` has two implementations behind `#ifdef MVVLVA`**

The non-MVVLVA path (with SEE) is the better one. Consider removing the
`#ifdef` and keeping only the SEE-enhanced version, or making it a
runtime option.

---

## 4. Move Generation (MoveGenerator.cpp)

### What's good
- Legal move generation with pin-aware logic — this is the right approach.
  Generating only legal moves avoids the cost of make/unmake for illegal
  moves.
- Proper handling of double check (only king moves).
- Single check: capture checker, block, or king move — correct.
- Pin ray handling for sliders and pawns — thorough and correct.
- EP discovered check detection — handles the tricky horizontal pin case.
- Magic bitboards for slider attacks — fast and correct.
- Hyperbola quintessence as a fallback — nice to have both.
- `add_loud_moves` generates only captures + check evasions for qsearch.

### Issues and improvements

**[MEDIUM] `add_loud_moves` doesn't generate promotions when not in check**

When not in check, `add_loud_moves` only generates captures (via the
`_legal_attacks` variants). But queen promotions are "loud" moves that
should be searched in qsearch — a pawn promoting to a queen is a massive
material swing. Missing these means the engine can miss promotion tactics
in the quiescence search.

**[MEDIUM] `MoveGenerator` is entirely static methods — it's a namespace**

All methods are static, all state (magic tables, lookup tables) is global.
This works but is misleading as a class. Consider converting to a
namespace, or at least documenting the rationale.

**[LOW] `get_king_danger_squares` recomputes all enemy attacks**

This is called once per `add_all_moves` / `add_loud_moves`, which is fine.
But it computes full attack maps for all enemy pieces. If you ever need to
optimize, you could cache this or compute it incrementally.

**[LOW] Castling constants are recomputed in `add_castles`**

The `CASTLE_BLOCKING_SQUARES`, `KING_SAFE_SQUARES`, and `CASTLING_RIGHTS`
arrays are declared as local constants inside the function. They should be
`static constexpr` to avoid any potential re-initialization overhead
(though the compiler likely optimizes this away).

---

## 5. Evaluation

### 5a. HandCraftedEvaluator (Evaluator.cpp)

**[HIGH] No tapered evaluation (game phase awareness)**

The king PST rewards centralization, which is correct for endgames but
terrible for middlegames where the king should hide behind pawns. Most
engines interpolate between middlegame and endgame PSTs based on remaining
material. This alone is probably costing 100+ Elo when the handcrafted
eval is active.

**[HIGH] No pawn structure evaluation**

The comment references doubled, blocked, and isolated pawns (D, S, I terms)
but none are implemented. Passed pawns are especially important in endgames.

**[MEDIUM] No bishop pair bonus**

Having two bishops is worth roughly +50cp in most positions. This is a
cheap check (popcount of bishop bitboard) with good Elo return.

**[MEDIUM] No mobility evaluation**

Even a simple "number of legal moves" approximation (using attack bitboards)
adds useful positional knowledge cheaply.

**[LOW] No rook on open/semi-open file bonus**

Standard positional knowledge that's cheap to compute with bitboards.

### 5b. NNUEEvaluator (NNUEEvaluator.cpp)

**[CRITICAL] Accumulator refresh on every move (see Board section above)**

The incremental update infrastructure exists (add_piece/remove_piece/
push/pop) but isn't being used. `do_move` calls `refresh()` instead of
incremental updates. This is the #1 performance issue.

**[HIGH] Forward pass is entirely scalar (no SIMD)**

The L1 accumulator is 256-wide, and the forward pass does element-wise
clipped ReLU and dot products in scalar loops. With SSE2/AVX2:
- Clipped ReLU on 256 int16s: ~4 instructions with AVX2 (vs 256 iterations)
- Dot products: massive speedup with `_mm256_madd_epi16`

This could speed up the forward pass by 4–8x, directly translating to
more NPS and deeper search.

**[MEDIUM] `acc_stack_` uses `std::vector` with `push_back`**

This causes heap allocations during search. Since `MAX_SEARCH_PLY = 64`,
pre-reserve the vector in the constructor, or use a fixed-size array with
a stack pointer.

```cpp
// In constructor:
acc_stack_.reserve(MAX_SEARCH_PLY);
```

**[LOW] Magic numbers in quantization**

`/ 128` and `/ 64` appear in the forward pass without named constants.
Consider `QUANT_SCALE_L2 = 128`, etc.

---

## 6. Static Exchange Evaluation (See.cpp)

### What's good
- Proper swap algorithm with speculative stores and negamax unwind.
- X-ray discovery through `consider_xrays` with `may_xray` mask.
- Handles en passant and promotions correctly.
- Good pruning: `max(-gain[d-1], gain[d]) < 0` early exit.
- Solid test coverage including edge cases.

### Issues and improvements

**[LOW] `get_least_valuable_piece` iterates from pawn to king**

This is correct but could be slightly faster with a bitboard approach
(isolate LSB of each piece type bitboard intersected with attadef).
Marginal improvement.

**[LOW] SEE is not used for pruning in the main search**

Many engines use SEE to prune losing captures at low depths in the main
alpha-beta search (not just for move ordering). This reduces the tree
without losing much accuracy.

---

## 7. Transposition Table (TranspositionTable.h / .cpp)

### What's good
- Simple and correct always-replace scheme.
- Hash move extraction even when depth is insufficient — good for move
  ordering.
- Clean probe/record interface.

### Issues and improvements

**[HIGH] Always-replace replacement scheme**

The current TT always overwrites existing entries regardless of depth or
age. This means a shallow search at a later iteration can overwrite a
deep, valuable entry. Most engines use a replacement strategy that
considers:
- Depth (prefer deeper entries)
- Age (prefer newer entries)
- Entry type (prefer exact over bound entries)

A simple improvement: only replace if `new_depth >= old_depth` or the
entry is from a previous search.

**[HIGH] Fixed 1M entry size, not configurable at runtime**

`HASH_TABLE_SIZE = 1024 * 1024` is hardcoded. The xboard protocol sends
a `memory` command, but `set_memory_size` is a no-op. UCI `setoption Hash`
is parsed but the TT is never actually resized. Allowing the user to set
TT size (e.g., 64MB, 256MB) is standard and can significantly help in
longer time controls.

**[MEDIUM] No aging mechanism**

Entries from previous searches persist and can cause stale best-move
suggestions. Adding a generation counter (incremented each search) and
preferring to replace old-generation entries is standard.

**[MEDIUM] Index computation uses modulo (`hash % table_.size()`)**

If the table size is a power of 2 (which 1024*1024 is), use
`hash & (table_.size() - 1)` for a faster bitmask instead of division.

**[LOW] No prefetch**

Before probing the TT, issuing a `__builtin_prefetch` on the expected
entry address can hide memory latency. This is a small but measurable
improvement at high NPS.

---

## 8. Time Management (TimeManager.h)

### What's good
- Soft/hard limit design — correct approach.
- Instability detection (extend time when best move changes) — good.
- Score-based adjustment (reduce time when clearly winning/losing) — good.
- Safety margin (90% of remaining time) — sensible.

### Issues and improvements

**[MEDIUM] `adjust_for_score` is called every iteration from depth 6+**

This means the soft limit keeps shrinking multiplicatively (× 3/5) every
iteration when the score is > 500cp. After a few iterations, the time
budget can become tiny. It should probably only be applied once, or use
a flag to track whether the adjustment was already made.

**[MEDIUM] No "easy move" detection**

If only one legal move exists, or if the best move has been stable for
many iterations and is clearly best (large score gap to second-best),
the engine should move quickly. This saves time for more complex positions.

**[LOW] Uses `clock()` instead of wall-clock time**

Same issue as in Search — `clock()` measures CPU time. Use
`std::chrono::steady_clock` for accurate wall-clock timing.

---

## 9. Zobrist Hashing (Zobrist.h / .cpp)

### What's good
- Standard Zobrist hashing with piece/square, castling, EP, and side keys.
- Uses `mt19937_64` for high-quality random numbers.
- Fixed seed (0) ensures reproducibility — good for debugging.

### Issues and improvements

**[LOW] Each Board owns its own Zobrist instance**

All Zobrist instances are identical (same seed = same keys). This wastes
memory if multiple Board objects exist. Consider making Zobrist a singleton
or using static member data.

---

## 10. Opening Book (Book.h / Book.cpp)

### What's good
- Standard Polyglot format support — compatible with widely available books.
- Binary search for key lookup — efficient.
- Weighted random selection among book moves — good variety.
- Separate Polyglot hash computation (not reusing engine's Zobrist keys) —
  correct, since Polyglot uses its own published random numbers.
- Configurable book depth limit.

### Issues and improvements

**[LOW] No book learning or move filtering**

The engine plays all book moves with weight-proportional probability.
Some engines filter out moves below a weight threshold or with known
poor results. Minor concern for competitive play.

---

## 11. Protocol Support (Xboard.cpp / UCI.cpp)

### What's good
- Both Xboard and UCI protocols implemented with clean dispatch maps.
- Pondering support in Xboard with ponder hit/miss detection.
- UCI runs search on a separate thread — non-blocking.
- Smart time allocation delegated to TimeManager in both protocols.
- Opening book and NNUE integration in both protocols.

### Issues and improvements

**[MEDIUM] UCI castling notation may be wrong for ponder moves**

`move_to_uci()` checks `board_.side_to_move()` to determine if castling
is white or black. But after `cmd_position` applies all moves, the side
to move is the *next* player. For `send_bestmove`, the best move is for
the current side to move (correct). But if the ponder move is a castling
move, the side to move will have already changed after the best move is
applied mentally — the ponder move's castling side is inferred from the
wrong board state.

**[MEDIUM] No `go ponder` support in UCI**

The UCI protocol supports `go ponder` for pondering on the opponent's
time. Currently only Xboard pondering is implemented.

**[MEDIUM] `setoption Hash` parsed but TT never resized**

The UCI handler parses the Hash option and stores `hash_size_mb_` but
the TT is never actually resized. Same issue in Xboard's `memory` command.

**[MEDIUM] No thread safety in UCI**

UCI search runs on `search_thread_` but `board_` is shared with the main
thread that processes `position` commands. Per UCI spec, GUIs shouldn't
send `position` during search, but defensive coding with a mutex would
prevent data races from misbehaving GUIs.

**[LOW] `set_memory_size` is a no-op in Xboard**

The xboard `memory` command is received but ignored. Should resize the TT.

**[LOW] `MAXMOVES = 500` for game move history**

Theoretical maximum is 5949 half-moves. 500 is fine for practical games
but could overflow in pathological test cases.

---

## 12. Code Quality and Architecture

### What's good
- Clean separation of concerns: Board (state), Search (algorithms),
  Evaluator (scoring), MoveGenerator (move gen), TimeManager (timing).
- Evaluator abstraction with virtual dispatch allows swapping between
  HandCrafted and NNUE — clean design.
- Comprehensive test suite: 20 test files covering perft, move generation,
  SEE, board invariance, draw detection, evaluation symmetry, search
  properties, FEN round-trip, book, and NNUE.
- Move class wraps U32 with clean accessors and builder functions.
- ScoredMove separates sort score from move identity.

### Issues and improvements

**[HIGH] `main.cpp` is ~590 lines with massive duplication**

The MCTS/AlphaZero/NNUE CLI parsing is copy-pasted across xboard, UCI,
and selfplay modes. The same `--mcts-simulations`, `--mcts-cpuct`,
`--alphazero`, `--nnue` parsing block appears 3 times. This should be
extracted into a shared configuration struct + parser function.

**[MEDIUM] `Constants.h` uses `const` globals instead of `constexpr`**

`const int` at namespace scope has internal linkage in C++, but each TU
gets its own copy. For integral constants, `constexpr` is preferred.
`PIECE_CHARS` (a `std::string`) and `PIECE_UNICODE` (a `const char*[]`)
should be `inline const` or `constexpr` to avoid ODR issues and duplicate
storage.

**[MEDIUM] `Common.h` includes `<assert.h>` instead of `<cassert>`**

C header in C++ code. Should use `<cassert>` for proper namespace handling.

**[MEDIUM] `TestPositions.cpp` is in `source/` (production code)**

Test infrastructure (`test_positions_benchmark`, `perft_benchmark`) is
compiled into the main binary via `blunder_lib`. This should live in
`test/` or be conditionally compiled.

**[LOW] `ScoredMove.score` is `U16` (unsigned 16-bit)**

This limits score range to 0–65535. Negative scores aren't representable.
Currently works because all scores are mapped to positive ranges, but
it's fragile.

**[LOW] `Common.h` includes `<iostream>`**

This gets pulled into every translation unit via the include chain.
Consider removing it and including only where needed.

---

## 13. Python Scripts

### What's good
- Well-structured, clean code across all 11 scripts.
- `run-benchmarks.py`: CSV logging with regression detection — solid.
- `analyze-sts.py`: per-category breakdown with strengths/weaknesses.
- `run-cutechess.py`, `compare_nnue_vs_handcrafted.py`, `run-tournament.py`:
  all support `--protocol` flag for xboard/UCI switching.
- Training scripts (train_nnue.py, train_alphazero.py) are well-documented.

### Issues and improvements

**[LOW] `analyze-sts.py` has no `--output` flag for JSON/CSV export**

Currently only prints to stdout. Adding structured output would make it
easier to track results over time programmatically.

---

## 14. Engine Strength — Potential Improvements

This section covers improvements that would directly increase playing
strength (Elo), organized by subsystem. Items here go beyond fixing
existing code — they're new features or algorithmic upgrades.

Current baseline (v0.5.1, 1M nodes):
- WAC: 200/300 (66.67%), ELO ~2725
- STS: 59507/118800 (50.09%), ELO ~1987
- STS strengths: Recapturing (79.3%), Simplification (66.2%), Bishop vs Knight (55.2%)
- STS weaknesses: Undermining (34.2%), Kingside Pawn Advance (35.1%), Queenside Pawn Advance (38.0%)

### 14a. Search Improvements

**[HIGH] Check extensions (~30-50 Elo)**

Extend search by 1 ply when the side to move is in check. Nearly
universal in strong engines. Prevents the horizon effect from hiding
forced mates and tactical sequences involving checks.

**[HIGH] Singular extensions (~20-40 Elo)**

When the TT move is significantly better than all alternatives at a
reduced depth, extend the TT move by 1 ply. This helps the engine
resolve critical positions more deeply. Requires a "verification search"
at reduced depth excluding the TT move.

**[HIGH] Futility pruning (~20-30 Elo)**

At depth 1-2, if static eval + margin is below alpha, skip quiet moves.
Extended futility pruning applies at depth 2-3 with a larger margin.
Razoring is a related technique at depth 1.

**[MEDIUM] Reverse futility pruning / static null move pruning (~15-25 Elo)**

If static eval - margin >= beta at low depths, return beta without
searching. The idea: if the position is already so good that even losing
a margin wouldn't drop below beta, don't bother searching.

**[MEDIUM] Late Move Pruning (LMP) (~10-20 Elo)**

At low depths, after searching the first N moves, skip remaining quiet
moves entirely (not just reduce them like LMR). More aggressive than
LMR but safe at shallow depths.

**[MEDIUM] Countermove heuristic (~10-15 Elo)**

Track which move refutes each (from, to) pair. When a beta cutoff
occurs, record the current move as the "countermove" to the previous
move. Use this for move ordering (scored between killers and history).

**[MEDIUM] Logarithmic LMR (~10-15 Elo)**

Current LMR uses a fixed reduction of 1. Modern engines use a
logarithmic formula: `R = log(depth) * log(move_index) / C`. This
reduces more aggressively for later moves at higher depths.

**[LOW] Aspiration window with multiple re-searches**

Current implementation does one fallback to full window. Strong engines
widen the window progressively (e.g., ×2, ×4, then full window) to
avoid the cost of a full-window search on every fail.

**[LOW] Internal Iterative Deepening (IID) / Internal Iterative Reductions**

When no hash move is available at a PV node, do a shallow search first
to find a good move to try first. Modern engines often use IIR (just
reduce depth by 1) instead of a full IID search.

### 14b. Evaluation Improvements (HandCrafted)

These matter when NNUE weights aren't available or as a training signal.

**[HIGH] Tapered evaluation (~50-100 Elo)**

Interpolate between middlegame and endgame piece-square tables based on
remaining material (game phase). The king PST currently rewards
centralization, which is wrong in the middlegame. With tapered eval:
- Middlegame king: hide behind pawns (castled position)
- Endgame king: centralize aggressively

```cpp
int phase = total_material / MAX_MATERIAL;  // 0.0 = endgame, 1.0 = opening
int score = phase * mg_score + (1 - phase) * eg_score;
```

**[HIGH] Pawn structure (~30-50 Elo)**

Currently no pawn structure evaluation at all. Key terms:
- Passed pawns: bonus increasing with rank (huge in endgames)
- Isolated pawns: penalty (no adjacent friendly pawns)
- Doubled pawns: penalty (two pawns on same file)
- Backward pawns: penalty (can't be defended by adjacent pawns)
- Connected pawns: bonus (pawns defending each other)
- Pawn hash table: cache pawn structure eval (pawns change rarely)

**[MEDIUM] King safety (~20-40 Elo)**

Evaluate king safety based on:
- Pawn shield (pawns in front of castled king)
- Open files near king (penalty)
- Enemy piece attacks toward king zone
- King tropism (distance of enemy pieces to king)

**[MEDIUM] Mobility (~15-25 Elo)**

Count pseudo-legal moves for each piece (using attack bitboards).
More mobile pieces are more valuable. Weight by piece type.

**[MEDIUM] Bishop pair bonus (~10-15 Elo)**

+30 to +50 cp when having both bishops. Cheap to compute (popcount
of bishop bitboard >= 2).

**[LOW] Rook on open/semi-open file (~5-10 Elo)**

Bonus for rooks on files with no friendly pawns (semi-open) or no
pawns at all (open). Cheap bitboard computation.

**[LOW] Rook/queen on 7th rank (~5-10 Elo)**

Bonus when rook or queen reaches the 7th rank (2nd rank for black),
especially when the enemy king is on the 8th rank.

### 14c. NNUE Improvements

**[CRITICAL] Incremental accumulator updates (~2x NPS)**

The #1 performance issue. `do_move` calls `refresh()` (full recompute)
instead of incremental `add_piece`/`remove_piece`. Fixing this alone
could double NPS, translating to ~1 extra ply of search depth.

**[HIGH] SIMD vectorization (~4-8x forward pass speedup)**

The forward pass is entirely scalar. With AVX2:
- Clipped ReLU on 256 int16s: 4 instructions vs 256 scalar ops
- Dot products: `_mm256_madd_epi16` for massive throughput
- Accumulator updates: vectorized add/subtract

**[HIGH] Better training data**

Current NNUE was trained on a small dataset. For competitive strength:
- Generate 10M+ positions from self-play at depth 8+
- Use Stockfish-style data generation (random plies + fixed-depth search)
- Train for more epochs with learning rate scheduling
- Validate with cutechess matches after each training run

**[MEDIUM] HalfKA architecture**

Current HalfKP (768 features) doesn't account for king position.
HalfKA (Half-King-All) uses king_square × piece × square features
(40960 inputs per perspective), giving the network king-relative
positional awareness. This is what modern NNUE engines use.

**[MEDIUM] Larger network**

Current architecture is 768→256→32→32→1. Consider:
- 768→512→32→32→1 (wider L1 for more feature capacity)
- Or HalfKA with 40960→256→32→32→1

**[LOW] Quantization-aware training**

Train with quantization in the loop so the int16 weights better
approximate the float32 training weights. Reduces quantization error.

### 14d. Move Ordering Improvements

**[MEDIUM] Capture history (~10-15 Elo)**

Like history heuristic but for captures: track which captures cause
beta cutoffs, indexed by [piece][to_square][captured_piece]. Use for
ordering captures alongside MVV-LVA.

**[MEDIUM] Continuation history (~10-15 Elo)**

Track move ordering statistics conditioned on the previous move
(1-ply continuation) and the move 2 plies ago (2-ply continuation).
This captures tactical patterns like "after Nf3, Bg5 is often good."

**[LOW] Promotions in qsearch**

`add_loud_moves` doesn't generate queen promotions when not in check.
Missing these means the engine can miss promotion tactics in qsearch.

### 14e. Transposition Table Improvements

**[HIGH] Depth-preferred replacement (~15-25 Elo)**

Replace always-replace with a scheme that considers depth and age.
Simple approach: replace if `new_depth >= old_depth || old_generation != current_generation`.

**[HIGH] Configurable TT size**

Wire UCI Hash and Xboard memory commands to actually resize the TT.
Larger TT = fewer collisions = better move ordering = stronger play,
especially at longer time controls.

**[MEDIUM] TT aging**

Add a generation counter incremented each `ucinewgame` / `new`. Prefer
replacing stale entries over fresh deep entries.

### 14f. Time Management Improvements

**[MEDIUM] Easy move detection (~5-10 Elo in time savings)**

Move instantly when only one legal move exists. Move quickly when the
best move has been stable for 5+ iterations with a large score gap to
the second-best move.

**[MEDIUM] Difficulty-based allocation**

Allocate more time for positions where the eval is close to 0 (unclear)
and less time for positions with a clear advantage. The current
`adjust_for_score` is a rough version of this but has the repeated
application bug.

### 14g. Estimated Elo Budget

If all high-priority improvements were implemented well:

| Improvement                    | Est. Elo |
|-------------------------------|----------|
| NNUE incremental updates      | +100-200 |
| NNUE SIMD                     | +50-100  |
| Better NNUE training data     | +100-200 |
| Check extensions              | +30-50   |
| Singular extensions           | +20-40   |
| Futility pruning              | +20-30   |
| TT replacement + sizing       | +15-25   |
| Tapered eval (fallback)       | +50-100  |
| Pawn structure (fallback)     | +30-50   |
| King safety (fallback)        | +20-40   |
| **Total potential**           | **+435-835** |

Note: Elo gains are not additive — they interact. Real-world gains will
be lower than the sum. The NNUE improvements alone (incremental + SIMD +
better data) are likely worth +200-400 Elo and should be the top priority.

---

## Priority Summary (by estimated impact)

**Status as of v0.7.0** — Most high-priority items from the original review
have been implemented. Items marked ✅ are done; remaining items are listed
by priority.

### Implemented since original review

| # | Item | Status |
|---|------|--------|
| 1 | NNUE incremental accumulator updates | ❌ Not done (HCE focus) |
| 2 | NNUE SIMD vectorization | ❌ Not done (HCE focus) |
| 3 | Check extensions | ✅ Done |
| 4 | Singular extensions | ✅ Done (depth ≥ 8, margin 50cp) |
| 5 | Futility pruning | ✅ Done (depth 1-2, margins 200/500cp) |
| 6 | SEE pruning in qsearch | ❌ Not done |
| 7 | Better NNUE training data | ❌ Not done (HCE focus) |
| 8 | TT depth-preferred replacement + configurable size | ✅ Done (depth+generation replacement, UCI Hash resize) |
| 9 | Tapered eval (MG/EG PSQTs) | ✅ Done (Stockfish-style phase interpolation) |
| 10 | Pawn structure evaluation | ✅ Done (passed, isolated, doubled, backward, connected + pawn hash) |
| 11 | King safety evaluation | ✅ Done (pawn shield, open files, king zone attacks) |
| 12 | History table aging/decay | ✅ Done (halved between iterations) |
| 13 | Refactor main.cpp | ❌ Not done |
| 14 | Board dependency injection | ❌ Not done |
| 15 | Reverse futility pruning | ✅ Done (depth 1-3, 120cp/depth margin) |
| 16 | Late Move Pruning (LMP) | ✅ Done (depth 1-3, threshold 3+depth*4) |
| 17 | Countermove heuristic | ✅ Done (scored at 70, between killers and history) |
| 18 | Logarithmic LMR | ✅ Done (log(depth)*log(move_index)/2.0 table) |
| 19 | Mobility evaluation | ✅ Done (UCI option, togglable) |
| 20 | Bishop pair bonus | ✅ Done (+30 MG, +50 EG) |
| 21 | Capture history / continuation history | ❌ Not done |
| 22 | Promotions in qsearch loud moves | ❌ Not done |
| 23 | HalfKA NNUE architecture | ❌ Not done |
| 24 | Easy move detection | ❌ Not done |
| 25 | is_draw optimization | ❌ Not done |
| 26 | adjust_for_score repeated application fix | ✅ Done (score_adjusted_ flag) |
| 27 | pop_count intrinsic | ✅ Done (__builtin_popcountll / __popcnt64) |
| 28 | Pre-reserve NNUE accumulator stack | ❌ Not checked |
| 29 | TT index via bitmask | ✅ Done (hash & mask_) |
| 30 | Replace #define with constexpr | ✅ Done |
| 31 | Remove redundant #ifndef NDEBUG | ❌ Not checked |
| 32 | C-style declarations → declare at point of use | ❌ Partial |
| 38 | clock() → steady_clock | ✅ Done (TimeManager + SearchStats) |
| 41 | Rook on open/semi-open file | ✅ Done (+20/+25 MG/EG open, +10/+15 semi-open) |
| 42 | Rook/queen on 7th rank | ✅ Done (+20/+30 MG/EG) |

### Additional fixes in v0.7.0 (not in original review)

- UCI bestmove 0000 bug: race condition in cmd_position + multipv override
- Time management abort flag: search continued past hard limit (~900 Elo fix)
- Skill UCI option (1-20) with eval noise for weakened play
- UCI_LimitStrength / UCI_Elo options
- Book usage scales with skill level
- Coach commands always run at full strength
- NPS reporting in test_positions()
- Bench tooling: elo search, fast-chess 1.8 compat, streaming output

### Remaining high-impact items (non-NNUE)

1. **SEE pruning in qsearch** — ✅ already done (missed in initial audit)
2. **Promotions in qsearch** — ✅ done in v0.7.0
3. **Easy move detection** — move instantly with one legal move or stable best move
4. **is_draw optimization** — scan only since last irreversible move
5. **Capture history** — track which captures cause cutoffs
6. **main.cpp refactor** — extract shared CLI config parsing
7. **MoveGenerator inlining** — helper functions (add_pawn_legal_attacks, etc.)
   are separate functions called from one place each. The compiler likely inlines
   them at -O3, but marking them `inline` or moving to a header could help if
   profiling shows move generation overhead. Low priority until profiled.

### NNUE improvements (deferred — needs GPU training infrastructure)

1. Incremental accumulator updates (~2x NPS)
2. SIMD vectorization (~4-8x forward pass)
3. Better training data (10M+ positions)
4. HalfKA architecture

### Current measured strength

| Evaluator | Elo (vs SF UCI_LimitStrength) | STS Score | WAC Score |
|-----------|-------------------------------|-----------|-----------|
| HCE       | ~2212                         | —         | 60.3% (181/300) |
