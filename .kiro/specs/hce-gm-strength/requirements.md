# Requirements Document — HCE GM-Strength

## Introduction

Blunder's HandCraftedEvaluator (HCE) mode currently plays at approximately 2212 Elo. The chess-coach application relies on HCE mode for its coaching protocol, which provides structured eval breakdowns (material, mobility, king_safety, pawn_structure). NNUE is not viable for this use case because it cannot produce interpretable sub-scores.

This spec targets raising HCE-mode playing strength to 2500+ Elo (GM level) through search improvements, evaluation enhancements, and transposition table upgrades. All improvements are validated via SPRT testing between phases to confirm measurable Elo gains.

The work is organized into four phases:
- Phase 1: Search improvements (highest Elo/effort ratio, benefits any evaluator)
- Phase 2: HCE evaluation improvements (pawn structure, king safety, mobility, piece bonuses)
- Phase 3: Transposition table improvements (replacement scheme, sizing, aging)
- Phase 4: Miscellaneous fixes (history overflow, is_draw optimization)

## Glossary

- **Search**: The alpha-beta search engine in Search.h/Search.cpp that performs iterative deepening with PVS, null move pruning, LMR, killer moves, and history heuristic.
- **HCE**: The HandCraftedEvaluator in Evaluator.h/Evaluator.cpp that scores positions using tapered piece-square tables, mobility counting, and tempo bonus.
- **Quiescence_Search**: The quiesce() function in Search.cpp that extends search at leaf nodes by examining captures to avoid the horizon effect.
- **TT**: The TranspositionTable in TranspositionTable.h/TranspositionTable.cpp that caches search results keyed by Zobrist hash.
- **SEE**: Static Exchange Evaluation in See.cpp that evaluates material outcome of capture sequences on a single square.
- **SPRT**: Sequential Probability Ratio Test, a statistical method for determining whether one engine version is stronger than another with controlled error rates.
- **LMR**: Late Move Reductions, a technique that reduces search depth for moves searched late in the move list.
- **PVS**: Principal Variation Search, a variant of alpha-beta that uses zero-window searches after finding a PV move.
- **Pawn_Hash_Table**: A dedicated hash table that caches pawn structure evaluation, since pawn structure changes infrequently.
- **King_Zone**: The set of squares surrounding a king (typically the king square plus its 8 neighbors) used for king safety evaluation.
- **Coaching_Protocol**: The protocol used by chess-coach that expects eval breakdowns with material, mobility, king_safety, and pawn_structure sub-scores from the HCE.

## Requirements

### Requirement 1: Check Extensions

**User Story:** As a chess engine developer, I want the search to extend by one ply when the side to move is in check, so that the engine does not miss forced mates and tactical sequences involving checks.

#### Acceptance Criteria

1. WHEN the side to move is in check, THE Search SHALL extend the search depth by 1 ply (search at `depth - 1 + 1` instead of `depth - 1`).
2. WHEN the side to move is not in check, THE Search SHALL search at the normal depth (`depth - 1`).
3. THE Search SHALL apply check extensions in the alphabeta function for all node types (PV and non-PV).
4. THE Search SHALL NOT apply check extensions in the Quiescence_Search (quiescence already handles captures after checks).

### Requirement 2: Futility Pruning

**User Story:** As a chess engine developer, I want the search to skip quiet moves at low depths when the static evaluation plus a margin is below alpha, so that the engine prunes unpromising branches and searches deeper.

#### Acceptance Criteria

1. WHEN depth is 1 and the static evaluation plus 200 centipawns is less than or equal to alpha, THE Search SHALL skip quiet non-capture, non-promotion moves (futility pruning).
2. WHEN depth is 2 and the static evaluation plus 500 centipawns is less than or equal to alpha, THE Search SHALL skip quiet non-capture, non-promotion moves (extended futility pruning).
3. WHILE the side to move is in check, THE Search SHALL NOT apply futility pruning.
4. THE Search SHALL NOT apply futility pruning at PV nodes.
5. THE Search SHALL still search captures, promotions, and moves that give check even when futility pruning conditions are met.

### Requirement 3: Reverse Futility Pruning

**User Story:** As a chess engine developer, I want the search to return beta early at low depths when the static evaluation minus a margin exceeds beta, so that clearly winning positions are resolved quickly.

#### Acceptance Criteria

1. WHEN depth is 1 to 3 and the static evaluation minus (depth × 120 centipawns) is greater than or equal to beta, THE Search SHALL return beta without searching moves (reverse futility pruning).
2. WHILE the side to move is in check, THE Search SHALL NOT apply reverse futility pruning.
3. THE Search SHALL NOT apply reverse futility pruning at PV nodes.

### Requirement 4: SEE Pruning in Quiescence Search

**User Story:** As a chess engine developer, I want the quiescence search to skip captures with negative SEE, so that the engine avoids wasting time on losing capture sequences.

#### Acceptance Criteria

1. WHEN a capture move has a SEE value less than 0, THE Quiescence_Search SHALL skip that move without searching it.
2. THE Quiescence_Search SHALL still search all captures with SEE value greater than or equal to 0.
3. THE Quiescence_Search SHALL evaluate SEE before calling do_move for each capture, to avoid the cost of making and unmaking losing captures.

### Requirement 5: Late Move Pruning

**User Story:** As a chess engine developer, I want the search to skip remaining quiet moves at low depths after a threshold number of moves have been searched, so that the engine prunes more aggressively at shallow depths.

#### Acceptance Criteria

1. WHEN depth is 1 to 3 and more than (3 + depth × 4) quiet moves have been searched at the current node, THE Search SHALL skip remaining quiet non-capture, non-promotion moves.
2. WHILE the side to move is in check, THE Search SHALL NOT apply late move pruning.
3. THE Search SHALL NOT apply late move pruning at PV nodes.
4. THE Search SHALL NOT prune killer moves via late move pruning.

### Requirement 6: Logarithmic LMR

**User Story:** As a chess engine developer, I want LMR to use a logarithmic reduction formula instead of a fixed reduction of 1, so that later moves at higher depths are reduced more aggressively.

#### Acceptance Criteria

1. THE Search SHALL compute LMR reduction as `floor(log(depth) × log(move_index) / 2.0)` with a minimum reduction of 1.
2. THE Search SHALL pre-compute LMR reductions in a lookup table indexed by depth and move index at engine startup.
3. WHEN the reduced-depth search returns a value above alpha, THE Search SHALL re-search at full depth (existing PVS re-search behavior).
4. THE Search SHALL NOT apply LMR to captures, promotions, killer moves, or moves when in check (preserving existing exclusion criteria).

### Requirement 7: Countermove Heuristic

**User Story:** As a chess engine developer, I want the search to track which move refutes each previous move, so that move ordering improves through countermove suggestions.

#### Acceptance Criteria

1. WHEN a quiet move causes a beta cutoff, THE Search SHALL record that move as the countermove for the previous move's (from_square, to_square) pair.
2. THE Search SHALL store countermoves in a `[2][64][64]` table indexed by `[side][previous_from][previous_to]`.
3. THE Search SHALL score countermoves at 70 in move ordering (below killer moves at 80/90, above history moves).
4. THE Search SHALL clear the countermove table at the start of each new search.

### Requirement 8: Singular Extensions

**User Story:** As a chess engine developer, I want the search to extend the TT move by one ply when it is significantly better than all alternatives, so that critical moves are resolved more deeply.

#### Acceptance Criteria

1. WHEN a TT move exists with HASH_BETA or HASH_EXACT flag and TT depth is at least (current depth - 3), THE Search SHALL perform a verification search at (depth / 2) excluding the TT move.
2. WHEN the verification search returns a value at least (TT value - 50 centipawns) below the TT value, THE Search SHALL extend the TT move by 1 ply.
3. THE Search SHALL NOT apply singular extensions at depth less than 8.
4. THE Search SHALL NOT apply singular extensions at root nodes (search ply 0).
5. THE Search SHALL NOT apply singular extensions recursively (a node already extended by singular extension does not trigger another).


### Requirement 9: Pawn Structure Evaluation

**User Story:** As a chess engine developer, I want the HCE to evaluate pawn structure (passed, isolated, doubled, backward, and connected pawns), so that the engine understands positional pawn play and provides pawn_structure sub-scores to the Coaching_Protocol.

#### Acceptance Criteria

1. THE HCE SHALL assign a tapered bonus to passed pawns, increasing with rank advancement (larger bonus for pawns closer to promotion).
2. THE HCE SHALL assign a tapered penalty to isolated pawns (pawns with no friendly pawns on adjacent files).
3. THE HCE SHALL assign a tapered penalty to doubled pawns (two or more friendly pawns on the same file).
4. THE HCE SHALL assign a tapered penalty to backward pawns (pawns that cannot advance without being captured by an enemy pawn, and have no friendly pawn behind them on adjacent files).
5. THE HCE SHALL assign a tapered bonus to connected pawns (pawns defended by or adjacent to a friendly pawn).
6. THE HCE SHALL cache pawn structure evaluation in a Pawn_Hash_Table keyed by a pawn-only Zobrist hash to avoid redundant computation.
7. THE HCE SHALL expose the pawn structure sub-score separately for consumption by the Coaching_Protocol.

### Requirement 10: King Safety Evaluation

**User Story:** As a chess engine developer, I want the HCE to evaluate king safety based on pawn shield, open files near the king, and enemy attacks, so that the engine understands king vulnerability and provides king_safety sub-scores to the Coaching_Protocol.

#### Acceptance Criteria

1. THE HCE SHALL assign a tapered bonus for pawn shield quality (friendly pawns on the 2nd and 3rd ranks in front of the castled king).
2. THE HCE SHALL assign a tapered penalty for open or semi-open files adjacent to the king.
3. THE HCE SHALL assign a tapered penalty proportional to the number of enemy piece attacks on the King_Zone.
4. THE HCE SHALL compute king safety only during the middlegame phase (when game phase value is above 40 out of 128).
5. THE HCE SHALL expose the king safety sub-score separately for consumption by the Coaching_Protocol.

### Requirement 11: Mobility Evaluation Improvement

**User Story:** As a chess engine developer, I want the HCE to compute mobility using attack bitboards per piece type instead of full legal move generation, so that mobility evaluation is faster and more granular.

#### Acceptance Criteria

1. THE HCE SHALL compute mobility as the count of pseudo-legal destination squares per piece, using precomputed attack bitboards (knight lookup, bishop/rook magic bitboards).
2. THE HCE SHALL exclude squares occupied by friendly pieces and squares controlled by enemy pawns from the mobility count.
3. THE HCE SHALL apply per-piece-type mobility weights: knights and bishops weighted higher than rooks and queens.
4. THE HCE SHALL use tapered mobility bonuses (separate middlegame and endgame weights).
5. THE HCE SHALL replace the current full legal move generation approach (`MoveGenerator::add_all_moves` in evaluate()) with the bitboard-based approach.
6. THE HCE SHALL expose the mobility sub-score separately for consumption by the Coaching_Protocol.

### Requirement 12: Bishop Pair Bonus

**User Story:** As a chess engine developer, I want the HCE to award a bonus when a side has both bishops, so that the engine values the bishop pair advantage.

#### Acceptance Criteria

1. WHEN a side has two or more bishops, THE HCE SHALL add a tapered bonus of 30 centipawns (middlegame) and 50 centipawns (endgame) to that side's evaluation.
2. THE HCE SHALL detect the bishop pair using popcount of the bishop bitboard for each side.

### Requirement 13: Rook on Open and Semi-Open Files

**User Story:** As a chess engine developer, I want the HCE to award bonuses for rooks on open and semi-open files, so that the engine values rook activity on files without pawns.

#### Acceptance Criteria

1. WHEN a rook is on a file with no pawns of either color, THE HCE SHALL add a tapered bonus of 20 centipawns (middlegame) and 25 centipawns (endgame) for that rook (open file).
2. WHEN a rook is on a file with no friendly pawns but at least one enemy pawn, THE HCE SHALL add a tapered bonus of 10 centipawns (middlegame) and 15 centipawns (endgame) for that rook (semi-open file).

### Requirement 14: Rook and Queen on 7th Rank

**User Story:** As a chess engine developer, I want the HCE to award bonuses for rooks and queens on the 7th rank, so that the engine values piece penetration.

#### Acceptance Criteria

1. WHEN a white rook or queen is on the 7th rank (rank index 6) and the black king is on the 8th rank (rank index 7), THE HCE SHALL add a tapered bonus of 20 centipawns (middlegame) and 30 centipawns (endgame).
2. WHEN a black rook or queen is on the 2nd rank (rank index 1) and the white king is on the 1st rank (rank index 0), THE HCE SHALL add a tapered bonus of 20 centipawns (middlegame) and 30 centipawns (endgame).

### Requirement 15: TT Depth-Preferred Replacement Scheme

**User Story:** As a chess engine developer, I want the TT to use a depth-and-age-aware replacement scheme instead of always-replace, so that deep, valuable entries are preserved.

#### Acceptance Criteria

1. WHEN recording a new TT entry, THE TT SHALL replace the existing entry if the new entry's depth is greater than or equal to the existing entry's depth.
2. WHEN recording a new TT entry, THE TT SHALL replace the existing entry if the existing entry's generation differs from the current search generation (stale entry).
3. WHEN recording a new TT entry and the existing entry has greater depth and is from the current generation, THE TT SHALL keep the existing entry.
4. THE TT SHALL store a generation field in each HASHE entry.

### Requirement 16: Configurable TT Size

**User Story:** As a chess engine developer, I want the TT size to be configurable via the UCI Hash option and Xboard memory command, so that users can allocate appropriate memory for their hardware.

#### Acceptance Criteria

1. WHEN the UCI handler receives a `setoption name Hash value N` command, THE TT SHALL resize to N megabytes.
2. WHEN the Xboard handler receives a `memory N` command, THE TT SHALL resize to N megabytes.
3. THE TT SHALL default to 64 megabytes if no size is specified.
4. THE TT SHALL accept sizes from 1 to 4096 megabytes.

### Requirement 17: TT Aging

**User Story:** As a chess engine developer, I want the TT to use a generation counter so that stale entries from previous searches are preferentially replaced.

#### Acceptance Criteria

1. WHEN a new search begins (ucinewgame, xboard new, or each iterative deepening root call), THE TT SHALL increment its generation counter.
2. THE TT SHALL store the current generation in each HASHE entry when recording.
3. THE TT SHALL use the generation field in the replacement decision as specified in Requirement 15.
4. THE TT generation counter SHALL wrap around using modular arithmetic (8-bit counter, wrapping at 256).

### Requirement 18: History Table Aging

**User Story:** As a chess engine developer, I want the history table values to be aged between iterative deepening iterations, so that history scores do not overflow or saturate from unbounded depth×depth increments.

#### Acceptance Criteria

1. WHEN a new iterative deepening iteration begins (depth > 1), THE Search SHALL halve all history table values (integer division by 2).
2. THE Search SHALL apply history aging before the search at each new depth, not after.

### Requirement 19: is_draw Optimization

**User Story:** As a chess engine developer, I want the is_draw function to scan only back to the last irreversible move instead of the full game history, so that draw detection is faster in long games.

#### Acceptance Criteria

1. THE Board SHALL compute the scan start index as `max(0, game_ply - half_move_count)` when checking for repetition.
2. THE Board SHALL step by 2 when scanning for repetition (only same-side-to-move positions can repeat).
3. THE Board SHALL preserve correct threefold repetition detection (3 occurrences outside search, 2 during search).

### Requirement 20: SPRT Validation Between Phases

**User Story:** As a chess engine developer, I want each phase of improvements to be validated via SPRT testing against the previous version, so that regressions are caught and Elo gains are confirmed.

#### Acceptance Criteria

1. WHEN a phase of improvements is complete, THE developer SHALL run an SPRT test using fast-chess with bounds [-5, 5] Elo at the 95% confidence level.
2. THE SPRT test SHALL use a time control of 10+0.1 (10 seconds base, 0.1 second increment) for a balance of speed and accuracy.
3. IF the SPRT test fails (new version is weaker), THEN THE developer SHALL investigate and fix regressions before proceeding to the next phase.
4. THE developer SHALL record SPRT results (Elo gain, LOS, number of games) in a results log for each phase.
