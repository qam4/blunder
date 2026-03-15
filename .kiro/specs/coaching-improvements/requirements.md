# Requirements Document

## Introduction

This spec addresses six outstanding issues from the chess-coach integration with the Blunder engine's coaching protocol. The issues were documented in `docs/blunder-improvements.md` after field testing. Four issues (BLUNDER-002, BLUNDER-003 UCI-level, BLUNDER-004) have already been fixed in prior specs and are excluded. The remaining work covers: completing the eval breakdown (BLUNDER-001), fixing coaching-specific MultiPV population (BLUNDER-003), enriching threat descriptions (BLUNDER-005), implementing tactical motif detection (BLUNDER-006), making king safety descriptions position-aware (BLUNDER-007), and reducing threat map verbosity (BLUNDER-008).

## Glossary

- **Coaching_Protocol**: The `coach` command extension to UCI that returns structured JSON analysis via `BEGIN_COACH_RESPONSE` / `END_COACH_RESPONSE` markers.
- **Position_Report**: The JSON payload returned by `coach eval`, containing eval breakdown, threats, tactics, king safety, pawn structure, threat map, and top PV lines.
- **Eval_Breakdown**: A JSON object within the Position_Report that decomposes the overall centipawn evaluation into named sub-score components.
- **HCE**: The HandCraftedEvaluator class that computes tapered evaluation with sub-score accessors for material, mobility, king_safety, pawn_structure, tempo, and piece_bonuses.
- **CoachDispatcher**: The C++ class (`CoachDispatcher.cpp`) that routes `coach` subcommands and invokes search/analysis.
- **PositionAnalyzer**: The stateless C++ utility class that extracts structured analysis data (eval breakdown, threats, tactics, king safety, threat map) from a board position.
- **CoachJson**: The C++ namespace that serializes PositionReport and ComparisonReport structs into JSON for the coaching protocol.
- **PVLine**: A struct holding one principal variation result: a score and a sequence of moves.
- **Threat_Map**: A per-square array of attack/defense counts included in the Position_Report.
- **Tactic**: A detected tactical motif (fork, pin, skewer, discovered attack, back-rank threat, overloaded piece) with squares, pieces, and a human-readable description.
- **King_Safety**: A per-side assessment containing a centipawn score and a human-readable description of the king's safety.
- **UCI_Notation**: Standard Universal Chess Interface move format, e.g. `"e2e4"`, `"e7e8q"`.

## Requirements

### Requirement 1: Complete Eval Breakdown with Tempo and Piece Bonuses

**User Story:** As a chess-coach developer, I want the eval breakdown to include all HCE sub-scores so that the sum of breakdown components matches the overall eval_cp and the LLM receives accurate per-component data.

#### Acceptance Criteria

1. THE Eval_Breakdown SHALL include six named fields: `material`, `mobility`, `king_safety`, `pawn_structure`, `tempo`, and `piece_bonuses`.
2. WHEN the PositionAnalyzer computes an eval breakdown, THE PositionAnalyzer SHALL read the `tempo` value from the HCE (28 centipawns for side-to-move, negated for the opponent) and the `piece_bonuses` value from `get_piece_bonuses_score()`.
3. THE sum of `material + mobility + king_safety + pawn_structure + tempo + piece_bonuses` SHALL equal `eval_cp` within a tolerance of 2 centipawns (to account for integer rounding in tapered evaluation).
4. WHEN the CoachJson serializes the eval breakdown, THE CoachJson SHALL include `tempo` and `piece_bonuses` as integer fields in the `eval_breakdown` JSON object.
5. THE `material` field SHALL represent only PSQT material (total eval minus all named sub-scores), not a catch-all residual that silently absorbs tempo and piece_bonuses.

### Requirement 2: Fix Coaching MultiPV Population

**User Story:** As a chess-coach developer, I want `coach eval` to return fully populated PV lines for all requested MultiPV lines so that the LLM can compare alternative candidate moves.

#### Acceptance Criteria

1. WHEN `coach eval` is invoked with `multipv N`, THE CoachDispatcher SHALL return N PV lines where each line has a non-zero depth and a non-empty moves array (provided the position has at least N legal moves).
2. IF the position has fewer legal moves than the requested multipv count, THEN THE CoachDispatcher SHALL return one PV line per legal move, each with an independent eval and move sequence.
3. WHEN the search completes, THE CoachDispatcher SHALL verify that `get_multipv_results()` contains fully populated PVLine entries before passing them to the PositionAnalyzer.
4. IF any PVLine in the multipv results has an empty moves vector after search, THEN THE CoachDispatcher SHALL exclude that line from the Position_Report `top_lines` array rather than including a placeholder with `depth: 0` and `moves: []`.

### Requirement 3: Enriched Threat Descriptions with UCI Moves

**User Story:** As a chess-coach developer, I want threat descriptions to include the specific UCI move notation and the actual target square and piece so that the LLM can generate precise tactical explanations.

#### Acceptance Criteria

1. WHEN the PositionAnalyzer generates a threat of type `"check"`, THE Threat description SHALL include the UCI move that delivers the check (e.g. `"Bc4 can give check via c4f7"`) and the `target_squares` field SHALL list the actual target square of the checking move, not the king square.
2. WHEN the PositionAnalyzer generates a threat of type `"capture"`, THE Threat description SHALL include the UCI move notation for the capture (e.g. `"Nf3 can capture undefended Bd6 via f3d6"`).
3. THE Threat struct SHALL include a `uci_move` string field containing the UCI notation of the threatening move.
4. WHEN the CoachJson serializes a Threat, THE CoachJson SHALL include the `uci_move` field in the JSON output.

### Requirement 4: Tactical Motif Detection

**User Story:** As a chess-coach developer, I want the engine to detect forks, pins, skewers, and discovered attacks from the current position and PV lines so that the LLM receives labeled tactical motifs instead of an empty tactics array.

#### Acceptance Criteria

1. WHEN the current position contains a fork (a piece attacking two or more higher-value opponent pieces), THE PositionAnalyzer SHALL include a Tactic entry with `type: "fork"`, `in_pv: false`, and a description naming the forking piece and its targets.
2. WHEN the current position contains a pin (a sliding piece attacking through a less-valuable piece to a more-valuable piece behind it), THE PositionAnalyzer SHALL include a Tactic entry with `type: "pin"` and `in_pv: false`.
3. WHEN the current position contains a skewer (a sliding piece attacking a more-valuable piece with a less-valuable piece behind it), THE PositionAnalyzer SHALL include a Tactic entry with `type: "skewer"` and `in_pv: false`.
4. WHEN a PV line contains a move that creates a fork, pin, skewer, or discovered attack within the first 4 half-moves, THE PositionAnalyzer SHALL include a Tactic entry with `in_pv: true`.
5. WHEN the PositionAnalyzer detects a discovered attack (moving a piece reveals an attack from a slider behind it onto a higher-value target), THE PositionAnalyzer SHALL include a Tactic entry with `type: "discovered_attack"`.
6. THE PositionAnalyzer SHALL detect discovered attacks both on the current board and within PV lines (first 4 half-moves).

### Requirement 5: Position-Aware King Safety Descriptions

**User Story:** As a chess-coach developer, I want king safety descriptions to reflect the actual position (castled status, pawn storm proximity, open files) so that the LLM generates accurate coaching text instead of repeating static templates.

#### Acceptance Criteria

1. WHEN the king has castled (king is on g1/g8 for kingside or c1/c8 for queenside), THE King_Safety description SHALL mention the castling side (e.g. `"kingside castled, solid pawn shield"`).
2. WHEN the king remains on its starting square (e1/e8) and castling rights are still available, THE King_Safety description SHALL mention that the king is uncastled (e.g. `"king uncastled, still has castling rights"`).
3. WHEN the king remains on its starting square and castling rights are lost, THE King_Safety description SHALL mention the exposed status (e.g. `"king stuck in center, castling rights lost"`).
4. WHEN enemy pawns are advanced to the 4th rank or beyond on files adjacent to the king (pawn storm), THE King_Safety description SHALL mention the pawn storm threat.
5. WHEN there is an open file (no pawns of either color) adjacent to or on the king's file, THE King_Safety description SHALL mention the open file.
6. WHEN the pawn shield is intact and no open files or pawn storms threaten the king, THE King_Safety description SHALL describe the king as safe (e.g. `"kingside castled, solid pawn shield"`).

### Requirement 6: Filtered Threat Map with Summary

**User Story:** As a chess-coach developer, I want the threat map to include only tactically interesting squares and provide a summary field so that the JSON payload is smaller and the LLM focuses on relevant information.

#### Acceptance Criteria

1. THE Position_Report `threat_map` array SHALL include only squares where at least one of the following is true: (a) `net_attacked` is true, (b) the square is occupied and the attacker count from the opponent exceeds the defender count by the piece's own side, or (c) the square is a key central square (d4, d5, e4, e5) with at least one attacker from either side.
2. THE Position_Report SHALL include a `threat_map_summary` string field that lists the most important findings in plain English (e.g. `"Nf3 is undefended and attacked; d5 is contested by both sides"`).
3. WHEN no squares meet the filtering criteria, THE `threat_map` array SHALL be empty and the `threat_map_summary` SHALL be `"no significant threats on the board"`.
4. THE filtered threat map SHALL contain no more than 16 entries to keep the JSON payload compact.
5. IF more than 16 squares meet the filtering criteria, THEN THE PositionAnalyzer SHALL prioritize squares with occupied pieces that are net-attacked, then contested key squares, and discard the rest.
