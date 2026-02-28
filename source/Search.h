/*
 * File:   Search.h
 *
 * Search class owns the search algorithms (alphabeta, negamax, minimax,
 * quiesce) and search-specific state (PV, counters, time manager).
 * Board is reduced to game state + move mechanics.
 */

#ifndef SEARCH_H
#define SEARCH_H

#include "Board.h"
#include "Evaluator.h"
#include "PrincipalVariation.h"
#include "TimeManager.h"
#include "TranspositionTable.h"

#define NO_PV 0   // Not a PV node
#define IS_PV 1
#define NO_NULL 0  // avoid doing null move twice in a row
#define DO_NULL 1

class Search
{
public:
    Search(Board& board, Evaluator& evaluator, TranspositionTable& tt);

    // Main entry point: iterative deepening search
    Move_t search(int depth,
                  int search_time = DEFAULT_SEARCH_TIME,
                  int max_nodes_visited = -1,
                  bool xboard = false);

    // Algorithm implementations
    int alphabeta(int alpha, int beta, int depth, int is_pv, int can_null);
    int quiesce(int alpha, int beta);
    int negamax(int depth);
    Move_t negamax_root(int depth);
    int minimax(int depth, bool maximizing_player);
    Move_t minimax_root(int depth, bool maximizing_player);

    // Accessors
    int get_searched_moves() const { return searched_moves_; }
    int get_search_best_score() const { return search_best_score_; }
    PrincipalVariation& get_pv() { return pv_; }
    TimeManager& get_tm() { return tm_; }

private:
    // Hash helpers (delegate to TT)
    int probe_hash(int depth, int alpha, int beta, Move_t& best_move);
    void record_hash(int depth, int val, int flags, Move_t best_move);

    Board& board_;
    Evaluator& evaluator_;
    TranspositionTable& tt_;
    PrincipalVariation pv_;
    TimeManager tm_;

    int searched_moves_ = 0;
    int nodes_visited_ = 0;
    int max_search_ply_ = 0;
    Move_t search_best_move_ = 0;
    int search_best_score_ = 0;
    int follow_pv_ = 0;
};

#endif /* SEARCH_H */
