# Blunder Chess Engine — Architecture & Algorithms

This document describes the search algorithms, evaluation, and move ordering
techniques used in the Blunder chess engine.

## Overview

Blunder is a chess engine that communicates via the Xboard protocol. Its core
loop is: generate legal moves, search the game tree, and return the best move
found within the time budget. The engine supports two search algorithms:
alpha-beta pruning (default) with iterative deepening and move ordering, or
Monte Carlo Tree Search (MCTS), selectable via the `--mcts` flag.

```
Xboard protocol
  └─ Search
       ├─ [default] Alpha-Beta (iterative deepening)
       │    ├─ Aspiration windows
       │    ├─ Transposition Table (probe/record)
       │    ├─ Null Move Pruning
       │    ├─ Late Move Reductions
       │    ├─ Quiescence Search
       │    └─ Move Ordering
       │         ├─ PV / Hash move
       │         ├─ Captures (SEE + MVV-LVA)
       │         ├─ Killer moves
       │         ├─ History heuristic
       │         └─ Quiet moves
       │
       └─ [--mcts] Monte Carlo Tree Search
            ├─ Selection (UCB1)
            ├─ Expansion (uniform priors or DualHeadNetwork policy head)
            ├─ Leaf evaluation (Evaluator)
            └─ Backpropagation (negamax sign flip)

  Evaluation: hand-crafted (piece-square tables) or NNUE
```

## Search Algorithms

### Iterative Deepening

The engine searches depth 1, then depth 2, then depth 3, and so on until time
runs out. Each iteration reuses information from the previous one (via the
transposition table and PV), so the overhead of re-searching shallow depths is
small. This also gives the engine a best move at every depth, so it always has
something to play if time expires.

### Alpha-Beta Pruning

Alpha-beta is the core search algorithm. It maintains a window [alpha, beta]
representing the range of scores the current player can achieve. When a move
produces a score >= beta, the search "cuts off" — the opponent would never
allow this position, so there is no need to look at remaining moves.

The engine uses Principal Variation Search (PVS): after finding a move that
raises alpha, subsequent moves are searched with a zero-window (-alpha-1,
-alpha) first. If the zero-window search fails high, a full re-search is done.
This saves time because most moves don't improve alpha.

### Aspiration Windows

Instead of searching with the full (-infinity, +infinity) window, each
iterative deepening iteration uses a narrow window centered on the previous
iteration's score. If the search falls outside this window, a full-width
re-search is performed. This produces more cutoffs in the common case where
the score doesn't change dramatically between depths.

### Null Move Pruning

Before searching moves in a position, the engine tries "doing nothing" (passing
the turn to the opponent) and searching at reduced depth. If the opponent can't
even take advantage of a free move (score >= beta), the position is so good
that we can prune it. This is skipped when in check, at PV nodes, and when the
side to move has no pieces (to avoid zugzwang issues).

The reduction R is adaptive: R=3 when depth > 6, R=2 otherwise.

### Quiescence Search

At leaf nodes (depth 0), instead of returning the static evaluation directly,
the engine continues searching capture sequences until the position is "quiet."
This prevents the horizon effect — where the engine stops searching right
before a major exchange and gets a misleading evaluation.

The quiescence search uses a stand-pat score (static eval) as a lower bound
and only considers captures that might improve the position.

### Mate Distance Pruning

If we already know we can force checkmate in N moves, there is no point
searching branches that can only find a mate in N+1 or more moves. The alpha
and beta bounds are tightened based on the current search ply to prune these
irrelevant branches.

## Move Ordering

Move ordering is critical for alpha-beta efficiency. The better the move
ordering, the more branches get pruned, and the deeper the engine can search
in the same time. Moves are scored and sorted before being searched, with
higher-scored moves tried first.

The scoring priority (highest to lowest):

| Priority | Score | Description |
|----------|-------|-------------|
| PV / Hash move | 255 | The best move from a previous search or TT hit |
| PV follow | 128 | Move from the principal variation at this ply |
| Killer slot 0 | 90 | Best quiet move that caused a cutoff at this ply |
| Killer slot 1 | 80 | Second-best killer at this ply |
| Promotions | 70+ | Promotion bonus added to capture score |
| Good captures | 10-65 | MVV-LVA score, validated by SEE |
| History moves | 6-79 | Quiet moves scaled by historical success |
| Quiet moves | 5 | Default score for non-capture, non-killer moves |
| Bad captures | 0 | Captures where SEE is negative (losing material) |

### Killer Move Heuristic

When a quiet (non-capture) move causes a beta cutoff, it is stored as a
"killer" for the current search ply. The engine keeps two killer slots per ply.
When a new killer is stored, it shifts the previous killer[0] to killer[1].

The idea: if a quiet move refuted one branch at a given depth, it is likely to
refute sibling branches at the same depth too. By trying killers before other
quiet moves, the engine gets more cutoffs earlier.

Killers are cleared at the start of each new search (each `search()` call from
iterative deepening).

### History Heuristic

The history table is a `[side][from_square][to_square]` array of integers. Each
time a quiet move causes a beta cutoff, its history counter is incremented by
`depth * depth` (deeper cutoffs are weighted more heavily, since they represent
more significant pruning).

During move ordering, quiet moves that are not killers get a bonus based on
their history value, scaled into the range 6-79 using the formula:
`bonus = 6 + (history * 73) / (history + 1000)`. This ensures history moves
are ordered above default quiet moves (5) but below killers (80).

The history table is cleared at the start of each search.

### Static Exchange Evaluation (SEE)

SEE simulates a sequence of captures on a single square to determine whether a
capture is winning or losing material. For example, if a queen captures a pawn
defended by another pawn, SEE determines that the exchange loses material
(queen for pawn).

Captures with negative SEE are scored as "bad captures" (score 0) and tried
last. Good captures are scored using MVV-LVA (Most Valuable Victim, Least
Valuable Attacker) which prioritizes capturing high-value pieces with
low-value attackers.

## Late Move Reductions (LMR)

After the first 3 moves in a position have been searched at full depth, the
engine assumes remaining quiet moves are unlikely to be best. For these "late"
moves, at depth >= 3, the search depth is reduced by 1 ply. If the reduced
search returns a score that beats alpha (suggesting the move might actually be
good), the engine re-searches at full depth to get an accurate value.

Moves that are excluded from reduction:
- Captures and promotions (tactical moves)
- Killer moves (known to be good)
- Moves when in check (all moves are important when in check)

LMR allows the engine to search significantly deeper in the same time budget
by spending less effort on moves that are statistically unlikely to matter.

## Monte Carlo Tree Search (MCTS)

MCTS is an alternative search algorithm selectable via the `--mcts` CLI flag.
Instead of exhaustively searching the game tree with alpha-beta pruning, MCTS
builds a search tree incrementally by running many simulations, each consisting
of four phases: selection, expansion, evaluation, and backpropagation.

### Why MCTS?

Alpha-beta is strong with good evaluation and move ordering, but it requires
hand-tuned pruning heuristics and struggles in positions where tactical depth
matters less than strategic understanding. MCTS naturally balances exploration
and exploitation without needing move ordering or pruning heuristics, and it
pairs well with neural network evaluation (NNUE or future policy nets).

### The Four Phases

Each simulation proceeds as follows:

```
Root
 ├─ Selection:  Walk down the tree picking the child with highest UCB
 ├─ Expansion:  At a leaf, generate all legal moves as new children
 ├─ Evaluation: Score the leaf position using the Evaluator
 └─ Backprop:   Send the value back up the tree, flipping sign each level
```

**Selection** — Starting from the root, the algorithm descends the tree by
always picking the child with the highest Upper Confidence Bound (UCB1) score.
This balances visiting promising nodes (exploitation) with trying under-explored
nodes (exploration). Unvisited children get a UCB of +infinity, guaranteeing
they are tried at least once.

**Expansion** — When selection reaches a leaf node that has been visited before,
all legal moves from that position are generated and added as children. Each
child receives a uniform prior probability (1/N where N is the number of legal
moves). This prior slot is designed for a future policy network that would
assign non-uniform priors based on move quality.

**Evaluation** — The leaf position is evaluated using the engine's Evaluator
(hand-crafted or NNUE). The centipawn score is converted to a value in [-1, 1]
using `tanh(score / 400)`. This mapping gives roughly:

| Centipawns | Value |
|------------|-------|
| ±100 cp    | ±0.24 |
| ±300 cp    | ±0.64 |
| ±600 cp    | ±0.93 |

Terminal positions return exact values: 0.0 for draws, +1.0 for checkmate
(from the winner's perspective).

The value is negated before backpropagation because `side_relative_eval`
returns the score from the current side-to-move's perspective, but
backpropagation needs it from the perspective of the side that just moved.

**Backpropagation** — The evaluation result is propagated back up the tree from
the leaf to the root. At each node, the visit count is incremented and the
value is added to the node's running total. The value is negated at each level
(negamax convention) because what is good for one side is bad for the other.

### UCB Formula

The UCB score for a child node is:

```
UCB = (total_value / visits) + c_puct * prior * sqrt(parent_visits) / (1 + visits)
```

- First term (exploitation): average value of this node
- Second term (exploration): bonus for under-visited nodes, scaled by the prior
  and the exploration constant `c_puct`

The default `c_puct` is 1.41 (approximately √2), the theoretically optimal
value for UCB1. Higher values favor exploration; lower values favor
exploitation.

### Move Selection

After all simulations complete, the engine selects the root child with the
most visits (robust child selection), not the highest average value. Visit
count is more stable than average value because a node with few visits might
have a misleadingly high average due to sampling noise.

### Time Management

MCTS checks the TimeManager every 64 simulations. If time is up, the search
stops early and returns the best move found so far. This is coarser-grained
than alpha-beta's node-based time checks, but sufficient because individual
MCTS simulations are fast.

### Configuration

| Flag | Default | Description |
|------|---------|-------------|
| `--mcts` | off | Enable MCTS instead of alpha-beta |
| `--alphazero` | off | Enable MCTS with dual-head network (requires `--nnue`) |
| `--mcts-simulations` | 800 | Number of simulations per search |
| `--mcts-cpuct` | 1.41 | Exploration constant (UCB1) |

### Future Extensions

- **Pondering**: Run MCTS simulations during the opponent's thinking time.
- **Tree reuse**: Keep the subtree from the opponent's move between searches
  instead of rebuilding from scratch.

## Dual-Head Neural Network (AlphaZero Architecture)

The `DualHeadNetwork` class implements a dual-head neural network designed for
AlphaZero-style MCTS. Unlike the single-head NNUE evaluator (which only
predicts position value), this network has two output heads sharing a common
feature representation.

### Architecture

```
Input: 768 HalfKP features (6 piece types x 2 colors x 64 squares)
         |
    Shared Trunk
    768 -> 256 (ReLU) -> 128 (ReLU)
         |
    +----+----+
    |         |
 Value     Policy
  Head      Head
128->32   128->4096
 (ReLU)   (linear)
 32->1
 (tanh)
    |         |
 float     float[N]
 [-1,+1]   (probability per legal move)
```

- **Shared trunk**: Two fully-connected layers with ReLU activation that
  transform the 768 binary input features into a 128-dimensional
  representation shared by both heads.

- **Value head**: Predicts who is winning, outputting a scalar in [-1, +1]
  via tanh. +1 means the side to move is winning; -1 means losing.

- **Policy head**: Outputs logits for all 4096 possible from-to square pairs
  (64 x 64). For a given position, only the slots corresponding to legal
  moves are kept; the rest are masked to -infinity before softmax
  normalization produces a probability distribution over legal moves.

### Feature Encoding

Same 768-element binary feature vector as the NNUE evaluator. Each feature
corresponds to a (piece-color, square) pair. A feature is 1.0 if that piece
occupies that square, 0.0 otherwise.

### Policy Encoding

Moves are encoded as `from_square * 64 + to_square`, giving 4096 possible
slots. Promotions to different piece types share the same from-to slot (a
simplification that works well in practice since underpromotions are rare).

### Weight Format

Weights are stored as 32-bit floats in a binary file, layer by layer:
trunk1 weights/biases, trunk2 weights/biases, value1 weights/biases,
value2 weight/bias, policy weights/biases.

### Integration with MCTS

When a `DualHeadNetwork` is available, MCTS uses it in two ways:
- **Expansion**: The policy head provides non-uniform priors for child nodes,
  focusing search on moves the network considers promising.
- **Leaf evaluation**: The value head replaces the hand-crafted evaluator,
  giving a learned positional assessment.

Without a dual-head network, MCTS falls back to uniform priors and the
hand-crafted evaluator (or single-head NNUE).

## AlphaZero Iterative Training Loop

The engine supports an AlphaZero-style self-improvement cycle where the
neural network and MCTS search reinforce each other iteratively. This is
orchestrated by `scripts/alphazero_loop.py`.

### How It Works

```
┌─────────────────────────────────────────────────────────────┐
│  1. Self-play: MCTS uses current network to play games      │
│     - Policy head → move priors for tree exploration         │
│     - Value head → leaf node evaluation                      │
│     - Record (position, visit_distribution, game_outcome)    │
│                          ↓                                   │
│  2. Train: Update network to match MCTS's findings           │
│     - Policy head learns to predict visit distribution       │
│     - Value head learns to predict game outcome              │
│                          ↓                                   │
│  3. Evaluate: Cutechess match vs HandCrafted (optional)      │
│                          ↓                                   │
│  4. Repeat from step 1 with stronger network                 │
└─────────────────────────────────────────────────────────────┘
```

The first iteration uses MCTS with uniform priors (no network). Subsequent
iterations use `--alphazero --nnue <weights>` so the dual-head network's
policy head guides MCTS exploration and the value head evaluates leaf nodes.

### Why It Improves

Each iteration produces a stronger network because:
- Better policy priors → MCTS focuses simulations on promising moves →
  higher quality training data
- Better value estimates → MCTS evaluates positions more accurately →
  better move selection
- More data accumulates across iterations → network generalizes better

### Data Flow

```
Engine binary                    Python scripts
─────────────                    ──────────────
blunder --selfplay --mcts        train_alphazero.py
  --alphazero --nnue w.bin         --input data.bin
  → selfplay_iterN.bin             --output w.bin
                                   → alphazero_iterN.bin

                                 compare_nnue_vs_handcrafted.py
                                   --nnue w.bin --games 20
                                   → Elo estimate
```

The `SelfPlay` class detects when a `DualHeadNetwork` is available and
passes it to the MCTS constructor, which uses the policy head for child
node priors during expansion and the value head for leaf evaluation.

### Configuration

| Parameter | Default | Description |
|-----------|---------|-------------|
| `--iterations` | 5 | Number of generate→train→evaluate cycles |
| `--games` | 100 | Self-play games per iteration |
| `--simulations` | 400 | MCTS simulations per move |
| `--epochs` | 10 | Training epochs per iteration |
| `--eval-games` | 20 | Cutechess games (0 to skip) |

See `python scripts/alphazero_loop.py --help` for all options.

## Transposition Table

The transposition table (TT) is a hash table keyed by Zobrist hash of the
board position. It stores the best move, score, depth, and score type (exact,
lower bound, or upper bound) for previously searched positions.

When the engine encounters a position it has seen before at sufficient depth,
it can reuse the stored result instead of re-searching. The TT also provides
the "hash move" — the best move from a previous search of this position —
which is tried first in move ordering.

## Evaluation

The current evaluation is hand-crafted, using:
- Material counting (piece values)
- Piece-square tables (positional bonuses/penalties based on where pieces sit)

The evaluation is symmetric: `eval(position) == -eval(mirror(position))` for
color-mirrored positions. This is validated by automated tests.

## Draw Detection

- **Fifty-move rule**: Draw if 100 half-moves without a pawn push or capture
- **Threefold repetition**: Draw if the position has occurred 3 times (outside
  search). During search, twofold repetition is used instead — if a position
  repeats even once during the search tree, it is treated as a draw to avoid
  repetition loops.

## Board Representation

Blunder uses bitboards — 64-bit integers where each bit represents a square.
There is one bitboard per piece type per color, plus aggregate bitboards for
all white and all black pieces. Move generation uses magic bitboards for
sliding piece attacks (bishops, rooks, queens).

## Performance Optimization Opportunities

This section documents known optimization opportunities across the engine,
categorized by type. Items are ordered roughly by expected impact.

### Bitwise / Low-Level

1. **Replace rank-mask loops with shift arithmetic** — `eval_pawn_structure()`
   builds `front_mask` and `behind_mask` with `for (int r = ...)` loops.
   Replace with: `front_mask = ~((1ULL << (8 * (rank + 1))) - 1)` for white
   (single shift + complement), and the inverse for black. Same applies to
   `behind_mask`. Eliminates up to 7 iterations per pawn per side.

2. **Precompute front/behind span tables** — Build `FRONT_SPAN[side][sq]` and
   `BEHIND_SPAN[side][sq]` lookup tables (128 × 8 bytes) at init time. Passed
   pawn, backward pawn, and connected pawn detection become single AND
   operations instead of computing masks per pawn.

3. **Precompute pawn attack spans** — `friendly_pawn_attacks` and
   `enemy_pawn_attacks` are recomputed inside the per-pawn loop in
   `eval_pawn_structure()`. Hoist them outside the loop — they depend only on
   the full pawn bitboard, not individual squares.

4. **Precompute king shield masks** — `eval_king_safety()` builds
   `shield_mask` with nested loops per file per side. Replace with a
   `KING_SHIELD[side][king_sq]` lookup table (128 × 8 bytes) computed at init.

5. **Use `sq & 7` instead of `sq % 8` for file extraction** — The compiler
   likely optimizes this already, but explicit bitwise AND is clearer intent
   and guaranteed branchless.

6. **Use `sq >> 3` instead of `sq / 8` for rank extraction** — Same as above.

### Redundant Computation

7. **Compute `occupied` once in `evaluate()`** — Both `eval_king_safety()` and
   `eval_mobility()` independently loop over all 12 piece bitboards to compute
   the occupied bitboard. Compute it once in `evaluate()` and pass it as a
   parameter. Better yet, add a `Board::occupied()` accessor that maintains
   the aggregate bitboard incrementally in `do_move()`/`undo_move()`.

8. **Compute pawn attack masks once** — `eval_mobility()` and
   `eval_king_safety()` both need pawn attack masks. Compute once in
   `evaluate()` and pass down.

9. **PSQT loop iterates all 64 squares** — The main `evaluate()` loop checks
   `board[square]` for all 64 squares. Replace with bitboard iteration: for
   each piece type, iterate set bits of its bitboard. This visits only
   occupied squares (~16 in middlegame vs 64).

10. **`phase()` recomputes piece counts** — Called once per `evaluate()`, but
    the piece counts could be maintained incrementally in `do_move()`.

### Algorithmic

11. **Incremental pawn Zobrist hash** — Currently iterates all pawn squares to
    compute the pawn hash from scratch. Maintain it incrementally: XOR in/out
    when pawns move or are captured. This makes pawn hash table probes nearly
    free.

12. **Doubled pawn detection without per-file loop** — The doubled pawn check
    loops over all 8 files with `pop_count()`. Instead, detect doubled pawns
    with: `pop_count(pawns) - pop_count(pawns & ~(pawns - FILE_BB_MASK))` or
    similar bitwise tricks that count files with multiple pawns in one pass.

13. **Pawn hash table sizing** — Current `PAWN_HASH_SIZE = 16384` with modulo
    indexing. Use power-of-two size with bitmask indexing (`hash & (size - 1)`)
    to avoid the expensive modulo operation.

14. **TT probe in quiescence** — The quiescence search doesn't probe the TT.
    Adding TT probing in qsearch can significantly reduce the number of nodes
    evaluated, especially in tactical positions.

### Profiling

15. **`perf` profiling** — Run `perf record -g ./build/rel/blunder --test-positions ...`
    then `perf report` to identify actual hotspots before optimizing. Previous
    profiling sessions have been done this way. The `dev-prof` CMake preset
    builds with `-O3 -g -fno-omit-frame-pointer` for accurate call stacks.
    Focus optimization effort on functions that show up in the top 10 of
    `perf report` — don't guess.

16. **Cachegrind / callgrind** — `valgrind --tool=callgrind` gives
    instruction-level profiling with cache miss analysis. Useful for spotting
    TT and pawn hash table thrashing.

### Structural

17. **`Board::occupied()` accessor** — Add an aggregate occupied bitboard
    maintained incrementally. Eliminates the 12-bitboard OR loop that appears
    in multiple eval functions and in move generation.

18. **`Board::side_pieces(side)` accessor** — Similarly, maintain per-side
    aggregate bitboards. The `friendly` mask computation in `eval_mobility()`
    loops over 6 piece types per side.

19. **Eval caching** — For positions that differ by only one move, many eval
    components (especially pawn structure, which is already cached) don't
    change. Consider caching the full eval or sub-scores in the TT entry.

20. **Profile-guided optimization (PGO)** — Build with `-fprofile-generate`,
    run a benchmark, then rebuild with `-fprofile-use`. Typically yields 5-15%
    NPS improvement for free.

21. **Link-time optimization (LTO)** — Enable `-flto` in the release build.
    Allows cross-translation-unit inlining of hot functions like
    `pop_count()`, `bit_scan_forward()`, and magic bitboard lookups.
