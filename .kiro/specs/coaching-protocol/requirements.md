# Requirements Document

## Introduction

This feature implements the engine side of the Coaching Protocol in Blunder. The Coaching Protocol extends UCI with custom `coach`-prefixed commands that return rich, structured evaluation data as JSON over the same stdin/stdout pipe. The engine must handle three commands (`coach ping`, `coach eval`, `coach compare`) while continuing to support all standard UCI commands. The client side (chess-coach) is implemented separately.

The authoritative protocol specification is #[[file:docs/coaching-protocol.md]].

## Glossary

- **UCI_Handler**: The UCI protocol handler (`UCI` class) that processes commands from stdin and sends responses to stdout.
- **Coach_Dispatcher**: The component within the UCI_Handler responsible for recognizing `coach`-prefixed commands and routing them to the appropriate handler.
- **Coach_Response**: A JSON payload wrapped in `BEGIN_COACH_RESPONSE` / `END_COACH_RESPONSE` markers, following the envelope structure defined in the protocol specification.
- **Response_Envelope**: The JSON wrapper containing `protocol`, `version`, `type`, and `data` fields as defined in Section 2.3 of the protocol specification.
- **Search_Engine**: The iterative deepening alpha-beta search (`Search` class) that finds the best move(s) for a given position.
- **Evaluator**: The evaluation component (`Evaluator` class and subclasses) that produces a centipawn score for a board position.
- **Position_Analyzer**: The component that inspects a board position to extract structured data: evaluation breakdown, hanging pieces, threats, pawn structure, king safety, tactics, and threat map.
- **Move_Comparator**: The component that compares a user's move against the engine's best move, computing eval drop, classification, NAG, and missed tactics.
- **FEN_String**: A Forsyth-Edwards Notation string describing a complete chess position (all 6 fields).
- **UCI_Move**: A move in UCI notation (e.g. `"e2e4"`, `"e7e8q"` for promotion).
- **Eval_Breakdown**: A decomposition of the overall evaluation into component scores: material, mobility, king_safety, and pawn_structure.
- **Threat_Map_Entry**: A per-square record of attacker/defender counts and net-attacked status.
- **NAG**: Numeric Annotation Glyph — a symbol (`!!`, `!`, `!?`, `?!`, `?`, `??`) classifying move quality.
- **Critical_Moment**: A position where the eval spread between the best and 3rd-best move exceeds 100 centipawns.

## Requirements

### Requirement 1: Command Routing

**User Story:** As a client developer, I want the engine to recognize `coach`-prefixed commands on stdin and route them to coaching handlers, so that coaching commands coexist with standard UCI commands on the same pipe.

#### Acceptance Criteria

1. WHEN the UCI_Handler receives a line starting with `coach`, THE Coach_Dispatcher SHALL parse the second token as the command name and route to the corresponding handler.
2. WHEN the UCI_Handler receives a standard UCI command (not prefixed with `coach`), THE UCI_Handler SHALL process the command using existing UCI logic with no change in behavior.
3. WHEN the Coach_Dispatcher receives a `coach` command with an unrecognized command name, THE Coach_Dispatcher SHALL respond with a Coach_Response containing an error envelope with code `"unknown_command"`.

### Requirement 2: Response Framing

**User Story:** As a client developer, I want all coaching responses wrapped in well-defined markers with a consistent JSON envelope, so that the client can reliably parse coaching data from the stdout stream.

#### Acceptance Criteria

1. THE Coach_Dispatcher SHALL wrap every coaching response in `BEGIN_COACH_RESPONSE` and `END_COACH_RESPONSE` markers, each on its own line with no leading or trailing whitespace.
2. THE Coach_Dispatcher SHALL emit the JSON payload as a single line (no embedded newlines) between the markers.
3. THE Coach_Dispatcher SHALL include the fields `protocol` (value `"coaching"`), `version` (value `"1.0.0"`), `type`, and `data` in every Response_Envelope.

### Requirement 3: Protocol Handshake (coach ping)

**User Story:** As a client developer, I want the engine to respond to `coach ping` with its identity and protocol version, so that the client can detect coaching support and negotiate compatibility.

#### Acceptance Criteria

1. WHEN the Coach_Dispatcher receives `coach ping`, THE Coach_Dispatcher SHALL respond with a Coach_Response of type `"pong"`.
2. THE `data` field of the pong response SHALL contain `status` (value `"ok"`), `engine_name` (value `"Blunder"`), and `engine_version` (the current build version string).

### Requirement 4: Position Evaluation (coach eval) — Search and Top Lines

**User Story:** As a client developer, I want the engine to perform a MultiPV search on a given FEN and return the top N principal variation lines, so that the client can show the user the best candidate moves with evaluations.

#### Acceptance Criteria

1. WHEN the Coach_Dispatcher receives `coach eval fen <FEN>`, THE Position_Analyzer SHALL parse the FEN_String using the existing FEN parser and set up the board position.
2. WHEN the `coach eval` command includes `multipv <N>`, THE Search_Engine SHALL perform a MultiPV search with N lines. WHEN `multipv` is omitted, THE Search_Engine SHALL default to 3 lines.
3. THE Position_Analyzer SHALL populate the `top_lines` array with one entry per PV line, each containing `depth`, `eval_cp` (in centipawns from side-to-move perspective), `moves` (in UCI notation), and `theme` (a short label describing the line's idea).
4. THE Position_Analyzer SHALL respond with a Coach_Response of type `"position_report"` containing the `fen` that was evaluated and the overall `eval_cp` from the side-to-move perspective.

### Requirement 5: Position Evaluation (coach eval) — Evaluation Breakdown

**User Story:** As a client developer, I want the engine to decompose its evaluation into component scores, so that the client can explain to the user which factors favor which side.

#### Acceptance Criteria

1. THE Position_Analyzer SHALL populate the `eval_breakdown` object with integer centipawn scores for `material`, `mobility`, `king_safety`, and `pawn_structure`.
2. THE sum of the `eval_breakdown` component scores SHALL approximately equal the overall `eval_cp` value.

### Requirement 6: Position Evaluation (coach eval) — Hanging Pieces

**User Story:** As a client developer, I want the engine to identify pieces that are attacked but not adequately defended, so that the client can warn the user about hanging material.

#### Acceptance Criteria

1. THE Position_Analyzer SHALL populate `hanging_pieces.white` and `hanging_pieces.black` arrays with entries for each piece that is attacked more times than it is defended by its own side.
2. Each hanging piece entry SHALL contain `square` (algebraic notation) and `piece` (one of `"pawn"`, `"knight"`, `"bishop"`, `"rook"`, `"queen"`).
3. WHEN no hanging pieces exist for a side, THE Position_Analyzer SHALL return an empty array for that side.

### Requirement 7: Position Evaluation (coach eval) — Threats

**User Story:** As a client developer, I want the engine to identify immediate tactical threats on the board, so that the client can highlight dangers to the user.

#### Acceptance Criteria

1. THE Position_Analyzer SHALL populate `threats.white` and `threats.black` arrays with detected tactical threats.
2. Each threat entry SHALL contain `type` (one of `"check"`, `"capture"`, `"fork"`, `"pin"`, `"skewer"`, `"discovered_attack"`), `source_square`, `target_squares`, and `description`.
3. WHEN no threats exist for a side, THE Position_Analyzer SHALL return an empty array for that side.

### Requirement 8: Position Evaluation (coach eval) — Pawn Structure

**User Story:** As a client developer, I want the engine to identify pawn structure features, so that the client can teach the user about positional weaknesses and strengths.

#### Acceptance Criteria

1. THE Position_Analyzer SHALL populate `pawn_structure.white` and `pawn_structure.black` objects, each containing `isolated`, `doubled`, and `passed` arrays.
2. Each array SHALL contain file letters (e.g. `"a"`, `"d"`) identifying files where the corresponding pawn feature exists.
3. WHEN no pawns exhibit a given feature for a side, THE Position_Analyzer SHALL return an empty array for that feature.

### Requirement 9: Position Evaluation (coach eval) — King Safety

**User Story:** As a client developer, I want the engine to assess king safety for both sides, so that the client can explain king vulnerability to the user.

#### Acceptance Criteria

1. THE Position_Analyzer SHALL populate `king_safety.white` and `king_safety.black` objects, each containing an integer `score` (in centipawns) and a `description` string.
2. THE `description` SHALL be a human-readable assessment of the king's safety (e.g. `"king safe behind pawns"`, `"king exposed, missing g-pawn shield"`).

### Requirement 10: Position Evaluation (coach eval) — Threat Map

**User Story:** As a client developer, I want per-square attack and defense counts, so that the client can visualize board control and identify vulnerable squares.

#### Acceptance Criteria

1. THE Position_Analyzer SHALL populate the `threat_map` array with entries for squares that are attacked or contain pieces.
2. Each Threat_Map_Entry SHALL contain `square`, `piece` (piece type string or `null`), `white_attackers`, `black_attackers`, `white_defenders`, `black_defenders`, and `net_attacked`.
3. THE `net_attacked` field SHALL be `true` when the piece on the square is attacked more times than defended by its own side.

### Requirement 11: Position Evaluation (coach eval) — Tactics Detection

**User Story:** As a client developer, I want the engine to detect tactical motifs in the position and PV lines, so that the client can highlight learning opportunities for the user.

#### Acceptance Criteria

1. THE Position_Analyzer SHALL populate the `tactics` array with detected tactical motifs.
2. Each tactic entry SHALL contain `type` (one of `"fork"`, `"pin"`, `"skewer"`, `"discovered_attack"`, `"back_rank_threat"`, `"overloaded_piece"`), `squares`, `pieces`, `in_pv` (boolean), and `description`.
3. WHEN no tactical motifs are detected, THE Position_Analyzer SHALL return an empty array.

### Requirement 12: Position Evaluation (coach eval) — Critical Moment Detection

**User Story:** As a client developer, I want the engine to flag positions where the choice of move matters significantly, so that the client can emphasize critical decision points to the user.

#### Acceptance Criteria

1. WHEN the eval spread between the best and 3rd-best PV line exceeds 100 centipawns, THE Position_Analyzer SHALL set `critical_moment` to `true` and `critical_reason` to a descriptive string.
2. WHEN the eval spread between the best and 3rd-best PV line is 100 centipawns or less, THE Position_Analyzer SHALL set `critical_moment` to `false` and `critical_reason` to `null`.
3. WHEN fewer than 3 PV lines are available, THE Position_Analyzer SHALL use the available lines for the spread calculation (best vs. last available line).


### Requirement 13: Move Comparison (coach compare) — Evaluation and Classification

**User Story:** As a client developer, I want the engine to compare a user's move against the best move and classify its quality, so that the client can give the user feedback on their move choices.

#### Acceptance Criteria

1. WHEN the Coach_Dispatcher receives `coach compare fen <FEN> move <MOVE>`, THE Move_Comparator SHALL search the position to find the best move and evaluate both the best move and the user's move.
2. THE Move_Comparator SHALL compute `eval_drop_cp` as `best_eval_cp - user_eval_cp` (always >= 0).
3. THE Move_Comparator SHALL classify the move as `"good"` (eval_drop_cp <= 30), `"inaccuracy"` (31-100), `"mistake"` (101-300), or `"blunder"` (> 300).
4. THE Move_Comparator SHALL assign a NAG symbol according to the thresholds defined in the protocol specification: `"!"` (eval_drop_cp <= 10), `"!?"` (11-30), `"?!"` (31-100), `"?"` (101-300), `"??"` (> 300).
5. WHEN the user's move equals the best move, THE Move_Comparator SHALL assign NAG `"!"` regardless of absolute eval.
6. THE Move_Comparator SHALL populate `best_move_idea` with a human-readable explanation of why the best move is strong.

### Requirement 14: Move Comparison (coach compare) — Refutation and Missed Tactics

**User Story:** As a client developer, I want the engine to show how the opponent punishes a bad move and what tactics were missed, so that the client can help the user learn from mistakes.

#### Acceptance Criteria

1. WHEN the classification is `"blunder"`, THE Move_Comparator SHALL populate `refutation_line` with the opponent's best response sequence (non-null, non-empty array of UCI_Moves).
2. WHEN the classification is not `"blunder"`, THE Move_Comparator SHALL set `refutation_line` to `null`.
3. THE Move_Comparator SHALL populate `missed_tactics` with tactical motifs present in the engine's best line that the user's move fails to exploit.
4. THE Move_Comparator SHALL include `top_lines` with the engine's top PV lines for context, using the same schema as the position_report `top_lines`.
5. THE Move_Comparator SHALL include `critical_moment` and `critical_reason` using the same logic as the position evaluation report.

### Requirement 15: Error Handling

**User Story:** As a client developer, I want the engine to respond with structured error messages for invalid inputs, so that the client can display meaningful feedback instead of silently failing.

#### Acceptance Criteria

1. IF the FEN_String provided to `coach eval` or `coach compare` cannot be parsed, THEN THE Coach_Dispatcher SHALL respond with a Coach_Response of type `"error"` with code `"invalid_fen"` and a descriptive message.
2. IF the UCI_Move provided to `coach compare` is not a legal move in the given position, THEN THE Coach_Dispatcher SHALL respond with a Coach_Response of type `"error"` with code `"invalid_move"` and a descriptive message.
3. IF an unexpected error occurs during processing of any coaching command, THEN THE Coach_Dispatcher SHALL respond with a Coach_Response of type `"error"` with code `"internal_error"` and a descriptive message.

### Requirement 16: UCI Coexistence

**User Story:** As a client developer, I want coaching commands to coexist with standard UCI commands without interference, so that the engine remains fully functional as a standard UCI engine.

#### Acceptance Criteria

1. THE UCI_Handler SHALL continue to process all standard UCI commands (`uci`, `isready`, `position`, `go`, `stop`, `setoption`, `ucinewgame`, `quit`) with identical behavior when coaching commands are present in the command stream.
2. THE Coach_Dispatcher SHALL process coaching commands synchronously — the engine SHALL complete the Coach_Response before reading the next command from stdin.
3. WHEN a coaching command is received while no search is in progress, THE Coach_Dispatcher SHALL process the command immediately.

### Requirement 17: JSON Serialization

**User Story:** As a client developer, I want the engine to produce valid, compact JSON for all coaching responses, so that the client can parse responses reliably.

#### Acceptance Criteria

1. THE Coach_Dispatcher SHALL produce valid JSON conforming to the schemas defined in the protocol specification for each response type (`"pong"`, `"position_report"`, `"comparison_report"`, `"error"`).
2. THE Coach_Dispatcher SHALL emit compact JSON with no pretty-printing (no indentation, no extra whitespace between tokens beyond what JSON requires).
3. FOR ALL valid Coach_Response JSON payloads, parsing the JSON and re-serializing in compact form SHALL produce an equivalent JSON string (round-trip property).
