# Blunder Engine — Full Code Review

Date: 2026-03-01

This document is a thorough code review of the Blunder chess engine codebase.
Findings are organized by component, with concrete improvement suggestions
prioritized by estimated Elo / performance impact.

The overarching principle: **depth wins**. A simpler, faster engine that
searches 2–3 plies deeper will almost always beat a slower engine with a
fancier eval. Every cycle saved in eval, move generation, or data structure
overhead translates directly into deeper search and stronger play.

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
a `memory` command, but `set_memory_size` is a no-op. Allowing the user
to set TT size (e.g., 64MB, 256MB) is standard and can significantly
help in longer time controls.

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

**[LOW] Castling rights hashing uses a lookup table indexed by rights value**

`castling_rights_[FULL_CASTLING_RIGHTS + 1]` = 16 entries. This is fine
and actually slightly better than XORing individual right keys, since it
avoids multiple XOR operations when multiple rights change at once.

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

## 11. Xboard Protocol (Xboard.h / .cpp)

### What's good
- Clean command dispatch via `handlers_` map — extensible and readable.
- Pondering support with ponder hit/miss detection.
- Smart time allocation delegated to TimeManager.
- Opening book integration.
- NNUE integration.

### Issues and improvements

**[MEDIUM] No UCI protocol support**

UCI is the dominant protocol for modern chess GUIs (Arena, CuteChess,
Banksia, etc.). Adding UCI support would make the engine compatible with
a much wider ecosystem. This is a significant feature addition, not a
bug fix.

**[LOW] `set_memory_size` is a no-op**

The xboard `memory` command is received but ignored. Should resize the TT.

**[LOW] `MAXMOVES = 500` for game move history**

Theoretical maximum is 5949 half-moves. 500 is fine for practical games
but could overflow in pathological test cases.

---

## 12. Move Representation (Move.h)

### What's good
- Compact 24-bit encoding with from/to/capture/flags.
- Clean builder functions for different move types.
- `ScoredMove` separates sort score from move identity — good design.
- Backward-compatible free functions alongside the Move struct.

### Issues and improvements

**[LOW] Captured piece stored in the move**

This uses 4 bits in the move encoding. Some engines instead look up the
captured piece from the board during `do_move`, saving bits for other
flags. Not a problem at current encoding density.

---

## 13. Principal Variation (PrincipalVariation.h / .cpp)

### What's good
- Triangular PV table — standard and correct.
- PV move scoring for move ordering.
- Validates moves before printing (handles stale PV entries).

### Issues and improvements

**[LOW] PV table is `MAX_SEARCH_PLY * MAX_SEARCH_PLY` = 4096 entries**

This is 16KB (4096 × 4 bytes). Fine, but a triangular array would use
half the memory. Not a practical concern.

---

## 14. Code Quality and Architecture

**[GOOD]** Clean separation of concerns: Board (state), Search (algorithms),
Evaluator (scoring), MoveGenerator (move gen), TimeManager (timing).

**[GOOD]** Evaluator abstraction with virtual dispatch allows swapping
between HandCrafted and NNUE — clean design.

**[GOOD]** Comprehensive test suite covering perft, move generation, SEE,
board invariance, draw detection, evaluation symmetry, and search
properties.

**[MEDIUM]** `MoveGenerator` is entirely static methods. This is fine but
means all state (magic tables, lookup tables) is global. For future SMP
support, this is okay since the tables are read-only after init.

**[LOW]** `Common.h` includes `<iostream>` which gets pulled into every
translation unit. Consider removing it and including only where needed.

---

## Priority Summary (by estimated impact)

### Critical (highest NPS / Elo impact)
1. Incremental NNUE accumulator updates in `do_move` (stop calling `refresh`)
2. SIMD vectorization of NNUE forward pass

### High
3. Check extensions in search
4. SEE pruning in quiescence search
5. TT replacement strategy (depth-preferred or age-based)
6. Configurable TT size
7. History table aging/decay
8. Tapered eval for handcrafted evaluator (if used as fallback)

### Medium
9. Futility pruning
10. `is_draw` optimization (scan only since last irreversible move)
11. Promotions in qsearch loud moves
12. `adjust_for_score` repeated application fix
13. UCI protocol support
14. `pop_count` intrinsic
15. Pre-reserve NNUE accumulator stack
16. TT index via bitmask instead of modulo

### Low
17. `clock()` → `std::chrono::steady_clock`
18. Zobrist singleton
19. Easy move detection
20. TT prefetch
21. Remove `#ifdef MVVLVA` dead path
