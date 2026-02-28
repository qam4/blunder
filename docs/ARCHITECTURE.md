# Blunder Chess Engine — Architecture & Algorithms

This document describes the search algorithms, evaluation, and move ordering
techniques used in the Blunder chess engine.

## Overview

Blunder is a chess engine that communicates via the Xboard protocol. Its core
loop is: generate legal moves, order them, search the game tree with
alpha-beta pruning, and return the best move found within the time budget.

```
Xboard protocol
  └─ Search (iterative deepening)
       ├─ Alpha-Beta with aspiration windows
       │    ├─ Transposition Table (probe/record)
       │    ├─ Null Move Pruning
       │    ├─ Late Move Reductions
       │    └─ Quiescence Search
       ├─ Move Ordering
       │    ├─ PV / Hash move
       │    ├─ Captures (SEE + MVV-LVA)
       │    ├─ Killer moves
       │    ├─ History heuristic
       │    └─ Quiet moves
       └─ Evaluation (hand-crafted, piece-square tables)
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
