/*
 * File:   MoveComparator.h
 *
 * Compares a user's move against the engine's best move, computing eval drop,
 * classification, NAG, refutation lines, and missed tactics.
 */

#ifndef MOVECOMPARATOR_H
#define MOVECOMPARATOR_H

#include <string>
#include <vector>

#include "Common.h"
#include "Board.h"
#include "Search.h"
#include "Move.h"
#include "PositionAnalyzer.h"

struct ComparisonReport {
    std::string fen;
    std::string user_move;      // UCI notation
    int user_eval_cp = 0;
    std::string best_move;      // UCI notation
    int best_eval_cp = 0;
    int eval_drop_cp = 0;
    std::string classification; // "good", "inaccuracy", "mistake", "blunder"
    std::string nag;            // "!!", "!", "!?", "?!", "?", "??"
    std::string best_move_idea;
    std::vector<std::string> refutation_line;  // empty if not blunder
    std::vector<Tactic> missed_tactics;
    std::vector<PVLine> top_lines;
    bool critical_moment = false;
    std::string critical_reason;
};

class MoveComparator {
public:
    /// Full comparison flow (stub — implemented in task 4.2)
    static ComparisonReport compare(Board& board, Search& search,
                                     Move_t user_move, int multipv_count = 3);

    /// Classification helpers (testable in isolation)
    static std::string classify(int eval_drop_cp);
    static std::string compute_nag(int eval_drop_cp, bool is_best_move);
    static std::string describe_best_move(const Board& board, Move_t best_move,
                                           const std::vector<Move_t>& pv_moves);
};

#endif /* MOVECOMPARATOR_H */
