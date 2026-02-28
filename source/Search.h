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

#include <cmath>
#include <ctime>

#define NO_PV 0   // Not a PV node
#define IS_PV 1
#define NO_NULL 0  // avoid doing null move twice in a row
#define DO_NULL 1

struct SearchStats
{
    int nodes_visited = 0;
    int hash_probes = 0;
    int hash_hits = 0;
    int beta_cutoffs = 0;
    int total_moves_searched = 0;
    clock_t start_time = 0;

    void reset()
    {
        nodes_visited = 0;
        hash_probes = 0;
        hash_hits = 0;
        beta_cutoffs = 0;
        total_moves_searched = 0;
        start_time = clock();
    }

    double elapsed_secs() const
    {
        return static_cast<double>(clock() - start_time) / CLOCKS_PER_SEC;
    }

    int nps() const
    {
        double secs = elapsed_secs();
        return (secs > 0.0) ? static_cast<int>(nodes_visited / secs) : 0;
    }

    double hash_hit_rate() const
    {
        return (hash_probes > 0) ? static_cast<double>(hash_hits) / hash_probes : 0.0;
    }

    double cutoff_rate() const
    {
        return (total_moves_searched > 0)
            ? static_cast<double>(beta_cutoffs) / total_moves_searched
            : 0.0;
    }

    double branching_factor(int depth) const
    {
        if (depth <= 0 || nodes_visited <= 0)
        {
            return 0.0;
        }
        return std::pow(static_cast<double>(nodes_visited), 1.0 / depth);
    }
};

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
    const SearchStats& get_stats() const { return stats_; }

    // Killer move accessors (for move scoring)
    Move_t get_killer(int ply, int slot) const { return killers_[ply][slot]; }

    // Verbose mode: log per-depth statistics during iterative deepening
    void set_verbose(bool v) { verbose_ = v; }

private:
    // Hash helpers (delegate to TT)
    int probe_hash(int depth, int alpha, int beta, Move_t& best_move);
    void record_hash(int depth, int val, int flags, Move_t best_move);

    Board& board_;
    Evaluator& evaluator_;
    TranspositionTable& tt_;
    PrincipalVariation pv_;
    TimeManager tm_;
    SearchStats stats_;
    bool verbose_ = false;

    int searched_moves_ = 0;
    int nodes_visited_ = 0;
    int max_search_ply_ = 0;
    Move_t search_best_move_ = 0;
    int search_best_score_ = 0;
    int follow_pv_ = 0;

    // Killer move table: two killer slots per ply
    Move_t killers_[MAX_SEARCH_PLY][2] = {};

    // History heuristic table: [side][from][to]
    int history_[2][64][64] = {};

    void store_killer(int ply, Move_t move);
    void score_killers(MoveList& list, int ply);
};

#endif /* SEARCH_H */
