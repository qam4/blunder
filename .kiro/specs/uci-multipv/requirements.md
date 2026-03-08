# Requirements Document

## Introduction

This feature adds MultiPV (multiple principal variations) support to the UCI protocol handler and alpha-beta search engine. MultiPV allows the engine to report the top N best lines during analysis, not just the single best move. This is a standard UCI feature used by chess GUIs for position analysis, where users want to see alternative candidate moves ranked by evaluation score.

## Glossary

- **UCI_Handler**: The UCI protocol handler (`UCI` class) that processes commands from the GUI and sends responses.
- **Search_Engine**: The iterative deepening alpha-beta search (`Search` class) that finds the best move(s) for a given position.
- **PV_Line**: A principal variation — a sequence of moves representing the best play from both sides, along with its evaluation score.
- **MultiPV_Count**: The number of principal variations the engine should find and report (1 = normal single-best mode).
- **Root_Move**: A legal move at the root position (depth 0) that serves as the starting move of a PV line.
- **Info_Line**: A UCI `info` output string sent to the GUI containing depth, score, PV, and other search statistics.
- **Excluded_Move**: A root move that the Search_Engine skips during a MultiPV iteration because it was already selected as a better PV line.

## Requirements

### Requirement 1: Advertise MultiPV Option

**User Story:** As a GUI developer, I want the engine to advertise MultiPV support during the UCI handshake, so that the GUI knows the engine supports multiple principal variations.

#### Acceptance Criteria

1. WHEN the UCI_Handler receives the "uci" command, THE UCI_Handler SHALL include an option line `option name MultiPV type spin default 1 min 1 max 256` in the response before "uciok".

### Requirement 2: Configure MultiPV Count

**User Story:** As a user, I want to set the number of principal variations via the `setoption` command, so that I can control how many alternative lines the engine reports.

#### Acceptance Criteria

1. WHEN the UCI_Handler receives `setoption name MultiPV value N` where N is an integer between 1 and 256, THE UCI_Handler SHALL store N as the active MultiPV_Count.
2. WHEN the UCI_Handler receives `setoption name MultiPV value N` where N is less than 1 or greater than 256, THE UCI_Handler SHALL clamp N to the nearest valid bound (1 or 256).
3. THE UCI_Handler SHALL use a default MultiPV_Count of 1 when no `setoption` command has been received for MultiPV.

### Requirement 3: Search Multiple Principal Variations

**User Story:** As a user, I want the engine to find the top N best lines during search, so that I can compare alternative candidate moves.

#### Acceptance Criteria

1. WHEN MultiPV_Count is greater than 1, THE Search_Engine SHALL perform MultiPV_Count separate searches at each iteration depth, where each search excludes the Root_Moves already selected as better PV_Lines in earlier iterations of the same depth.
2. WHEN MultiPV_Count equals 1, THE Search_Engine SHALL perform a single search identical to the current behavior with no performance overhead.
3. THE Search_Engine SHALL rank the resulting PV_Lines in descending order of evaluation score (best first) for each completed depth.
4. IF the number of legal Root_Moves is less than MultiPV_Count, THEN THE Search_Engine SHALL report only as many PV_Lines as there are legal Root_Moves.

### Requirement 4: Report MultiPV Info Lines

**User Story:** As a GUI developer, I want the engine to output correctly formatted MultiPV info lines, so that the GUI can display all principal variations to the user.

#### Acceptance Criteria

1. WHEN MultiPV_Count is greater than 1, THE UCI_Handler SHALL include the `multipv` field in each Info_Line, with values ranging from 1 (best) to MultiPV_Count.
2. THE UCI_Handler SHALL output one Info_Line per PV_Line per completed depth, each containing: depth, score cp, nodes, nps, time, multipv index, and the PV move sequence.
3. WHEN MultiPV_Count equals 1, THE UCI_Handler SHALL omit the `multipv` field from Info_Lines to maintain backward compatibility with GUIs that do not support MultiPV.

### Requirement 5: Best Move Selection with MultiPV

**User Story:** As a GUI developer, I want the engine to always send the overall best move as the `bestmove` response, so that the GUI can make the correct move in play mode.

#### Acceptance Criteria

1. THE UCI_Handler SHALL send the Root_Move from the highest-ranked PV_Line (multipv 1) as the `bestmove` response, regardless of MultiPV_Count.
2. WHEN the highest-ranked PV_Line contains at least two moves, THE UCI_Handler SHALL include the second move as the `ponder` move in the `bestmove` response.

### Requirement 6: MultiPV Interaction with Opening Book

**User Story:** As a user, I want the opening book to take precedence over MultiPV search when enabled, so that book moves are returned without unnecessary computation.

#### Acceptance Criteria

1. WHEN the opening book is enabled and has a move for the current position, THE UCI_Handler SHALL return the book move as `bestmove` without performing MultiPV search.

### Requirement 7: MultiPV Interaction with Time Management

**User Story:** As a user, I want time management to account for the additional work required by MultiPV search, so that the engine does not exceed its allocated time.

#### Acceptance Criteria

1. WHILE MultiPV_Count is greater than 1, THE Search_Engine SHALL check the time limit and abort flag between each PV iteration at a given depth.
2. IF the time limit is reached during a MultiPV search at a given depth, THEN THE Search_Engine SHALL stop the current depth and use the PV_Lines from the last fully completed depth.

### Requirement 8: MultiPV with MCTS Search

**User Story:** As a user, I want MultiPV to apply only to the alpha-beta search path, so that the MCTS search path remains unaffected.

#### Acceptance Criteria

1. WHEN the engine is in MCTS search mode, THE Search_Engine SHALL ignore the MultiPV_Count setting and return a single best move.
