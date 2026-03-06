# Requirements Document

## Introduction

Port the tapered evaluation improvements from the `eval` branch into the current Blunder chess engine architecture. The `eval` branch introduced Stockfish-style tapered evaluation with separate middlegame (MG) and endgame (EG) piece-square tables, phased material values, a mobility term, and a tempo bonus. The branch is structurally obsolete (it reverts architecture changes), but the evaluation logic is solid. This feature upgrades `HandCraftedEvaluator` with the tapered eval approach while preserving the `Evaluator` abstract interface and leaving `NNUEEvaluator` untouched.

## Glossary

- **HandCraftedEvaluator**: The concrete `Evaluator` subclass in `source/Evaluator.cpp` that performs classical hand-tuned evaluation of chess positions.
- **Evaluator**: The abstract base class in `source/Evaluator.h` defining the `evaluate()` and `side_relative_eval()` interface used by `Search`.
- **NNUEEvaluator**: A separate `Evaluator` implementation for neural network evaluation; must remain unaffected.
- **Board**: The game state class providing bitboards, piece placement, side to move, and move generation access.
- **PSQT**: Piece-Square Table — a lookup table mapping each (piece, square) pair to a positional bonus.
- **Tapered_Evaluation**: An evaluation technique that blends separate middlegame and endgame scores using a game-phase factor derived from remaining non-pawn material.
- **Game_Phase**: A scalar value in the range [0, 128] representing how far the position has progressed from middlegame (128) toward endgame (0), computed from non-pawn material on the board.
- **MG**: Middlegame — the phase of the game where most pieces are still on the board.
- **EG**: Endgame — the phase of the game where few non-pawn pieces remain.
- **Midgame_Limit**: The total non-pawn material value (15258) at or above which the position is considered fully middlegame (Game_Phase = 128).
- **Endgame_Limit**: The total non-pawn material value (3915) at or below which the position is considered fully endgame (Game_Phase = 0).
- **Mobility**: A count of pseudo-legal or legal moves available to a side, used as an evaluation term.
- **Tempo_Bonus**: A small fixed bonus (+28 centipawns) awarded to the side to move, reflecting the advantage of having the initiative.
- **File_Symmetry**: The property that PSQT values for files E-H mirror files A-D, allowing tables to be stored in a compressed 4-column format and expanded at initialization.
- **NPS**: Nodes Per Second — the number of search tree nodes the engine evaluates per second, used to measure eval function speed.

## Requirements

### Requirement 1: Phased Material Values

**User Story:** As a chess engine developer, I want separate middlegame and endgame material values for each piece type, so that the evaluation reflects how piece values change between game phases.

#### Acceptance Criteria

1. THE HandCraftedEvaluator SHALL define separate MG and EG material values for each piece type (Pawn, Knight, Bishop, Rook, Queen, King).
2. THE HandCraftedEvaluator SHALL use the Stockfish-tuned material values from the eval branch: Pawn (124 MG, 206 EG), Knight (781 MG, 854 EG), Bishop (825 MG, 915 EG), Rook (1276 MG, 1380 EG), Queen (2538 MG, 2682 EG).
3. THE HandCraftedEvaluator SHALL store material values in an array indexed by piece type and game phase (MG or EG).

### Requirement 2: Dual-Phase Piece-Square Tables

**User Story:** As a chess engine developer, I want separate middlegame and endgame piece-square tables with Stockfish-tuned values, so that positional bonuses reflect the different strategic priorities in each game phase.

#### Acceptance Criteria

1. THE HandCraftedEvaluator SHALL define separate MG and EG PSQT bonus values for each piece type using the Stockfish-tuned values from the eval branch.
2. THE HandCraftedEvaluator SHALL store PSQT source data in a compressed 4-column File_Symmetry format (files A through D only).
3. THE HandCraftedEvaluator SHALL expand the compressed PSQT data into a full 64-square table for each piece type, each game phase, and each color at initialization time.
4. WHEN expanding PSQT data, THE HandCraftedEvaluator SHALL mirror file A values to file H, file B to file G, file C to file F, and file D to file E.
5. WHEN expanding PSQT data for black pieces, THE HandCraftedEvaluator SHALL vertically flip the square index (rank 1 maps to rank 8 and vice versa).
6. THE HandCraftedEvaluator SHALL combine material values and PSQT bonuses into the expanded table so that a single lookup yields the total value for a (phase, piece, square) triple.

### Requirement 3: Game Phase Calculation

**User Story:** As a chess engine developer, I want to compute a game phase score from the remaining non-pawn material, so that the evaluation can blend middlegame and endgame scores proportionally.

#### Acceptance Criteria

1. WHEN evaluating a position, THE HandCraftedEvaluator SHALL compute the total non-pawn material value on the board by summing the MG material values of all non-pawn, non-king pieces for both sides.
2. THE HandCraftedEvaluator SHALL compute Game_Phase as: `128 * (npm - Endgame_Limit) / (Midgame_Limit - Endgame_Limit)`, where npm is the total non-pawn material.
3. IF the total non-pawn material is less than or equal to Endgame_Limit, THEN THE HandCraftedEvaluator SHALL set Game_Phase to 0.
4. IF the total non-pawn material is greater than or equal to Midgame_Limit, THEN THE HandCraftedEvaluator SHALL set Game_Phase to 128.
5. THE HandCraftedEvaluator SHALL use Midgame_Limit = 15258 and Endgame_Limit = 3915.

### Requirement 4: Tapered Score Blending

**User Story:** As a chess engine developer, I want the final evaluation to blend middlegame and endgame scores using the game phase, so that the evaluation smoothly transitions between phases.

#### Acceptance Criteria

1. WHEN evaluating a position, THE HandCraftedEvaluator SHALL compute separate MG and EG scores by summing the expanded PSQT values (which include material) for all pieces on the board, with white pieces contributing positively and black pieces contributing negatively.
2. THE HandCraftedEvaluator SHALL compute the tapered evaluation as: `(mg_score * Game_Phase + eg_score * (128 - Game_Phase)) / 128`.
3. THE HandCraftedEvaluator SHALL return the tapered evaluation from the `evaluate()` method as a centipawn score from white's perspective.

### Requirement 5: Mobility Term

**User Story:** As a chess engine developer, I want a mobility bonus in the evaluation, so that positions with more available moves are scored higher.

#### Acceptance Criteria

1. WHEN evaluating a position, THE HandCraftedEvaluator SHALL compute the number of legal moves for white and the number of legal moves for black.
2. THE HandCraftedEvaluator SHALL add a mobility term of `20 * (white_legal_moves - black_legal_moves)` to the evaluation score.
3. THE HandCraftedEvaluator SHALL add the mobility term to the tapered score before returning the final evaluation.

### Requirement 6: Tempo Bonus

**User Story:** As a chess engine developer, I want a tempo bonus for the side to move, so that the evaluation accounts for the initiative advantage.

#### Acceptance Criteria

1. WHEN evaluating a position, THE HandCraftedEvaluator SHALL add a Tempo_Bonus of +28 centipawns if white is the side to move, or -28 centipawns if black is the side to move.
2. THE HandCraftedEvaluator SHALL add the Tempo_Bonus to the tapered score before returning the final evaluation.

### Requirement 7: PSQT Initialization

**User Story:** As a chess engine developer, I want the piece-square tables to be fully expanded at construction time, so that evaluation lookups during search are a single array access with no runtime expansion logic.

#### Acceptance Criteria

1. WHEN a HandCraftedEvaluator is constructed, THE HandCraftedEvaluator SHALL run a `psqt_init()` routine that populates the full expanded PSQT array.
2. THE HandCraftedEvaluator SHALL store the expanded PSQT in a three-dimensional array indexed by game phase (MG=0, EG=1), piece type, and square (0-63).
3. WHEN `psqt_init()` completes, THE HandCraftedEvaluator SHALL have a fully populated table such that `piece_square_table_[phase][piece][square]` returns the combined material + positional value for any valid (phase, piece, square) input.

### Requirement 8: Configurable Eval Terms

**User Story:** As a chess engine developer, I want to toggle expensive evaluation terms (like mobility) on or off, so that I can find the best speed/accuracy tradeoff for different time controls.

#### Acceptance Criteria

1. THE HandCraftedEvaluator SHALL accept a configuration struct (or flags) that controls which optional eval terms are enabled.
2. THE HandCraftedEvaluator SHALL support at minimum a toggle for the Mobility term (enabled/disabled), since it requires move generation for both sides and is the most expensive optional term.
3. THE HandCraftedEvaluator SHALL support a toggle for the Tempo_Bonus term (enabled/disabled).
4. WHEN an optional eval term is disabled, THE HandCraftedEvaluator SHALL skip its computation entirely (no branch cost beyond the flag check).
5. THE configuration SHALL be settable at construction time and modifiable at runtime (e.g., via UCI `setoption`).
6. THE default configuration SHALL have all eval terms enabled.

### Requirement 9: Strength Validation — Fixed-Depth and Fixed-Time

**User Story:** As a chess engine developer, I want to measure the playing strength impact of the tapered evaluation at both fixed depth and fixed time, so that I can distinguish eval quality improvements from speed-related regressions.

#### Acceptance Criteria

1. THE developer SHALL run WAC and STS benchmarks at a fixed depth (e.g., depth 8) to measure eval quality independent of speed. This isolates whether the tapered eval produces better scores per node.
2. THE developer SHALL run WAC and STS benchmarks at a fixed time to measure real-world playing strength. This captures the net effect of eval quality vs. NPS cost.
3. THE developer SHALL measure and record NPS (nodes per second) for both the old and new evaluation, so the speed cost of the new eval is quantified.
4. THE tapered evaluation SHALL NOT cause a WAC solve rate regression greater than 5% at fixed depth compared to the previous baseline.
5. THE tapered evaluation SHALL NOT cause an STS score regression greater than 50 points at fixed depth compared to the previous baseline.
6. IF the tapered evaluation shows a fixed-depth improvement but a fixed-time regression, THE developer SHALL experiment with disabling the Mobility term (Requirement 8) to determine if the speed/accuracy tradeoff improves.
7. IF cutechess-cli is available, THE developer SHOULD run SPRT matches at the target time control to estimate Elo difference.
8. THE developer SHALL log all benchmark results (fixed-depth scores, fixed-time scores, NPS) with the git commit hash for historical tracking.

### Requirement 10: Evaluator Interface Preservation

**User Story:** As a chess engine developer, I want the Evaluator interface to remain unchanged, so that Search, NNUEEvaluator, and all other consumers continue to work without modification.

#### Acceptance Criteria

1. THE HandCraftedEvaluator SHALL continue to implement the `Evaluator` abstract interface with `evaluate(const Board& board)` and `side_relative_eval(const Board& board)` methods.
2. THE HandCraftedEvaluator SHALL preserve the existing `side_relative_eval()` semantics: return the evaluation from the perspective of the side to move.
3. THE `Evaluator` base class, `NNUEEvaluator`, `Board`, and `Search` classes SHALL require zero modifications to their public interfaces.
4. THE `Board::get_evaluator()` method SHALL continue to return an `Evaluator&` reference that dispatches to either `HandCraftedEvaluator` or `NNUEEvaluator` based on NNUE availability.
