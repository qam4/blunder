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

#include <chrono>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <vector>

constexpr int NO_PV = 0;   // Not a PV node
constexpr int IS_PV = 1;
constexpr int NO_NULL = 0;  // avoid doing null move twice in a row
constexpr int DO_NULL = 1;

struct SearchStats
{
    int nodes_visited = 0;
    int hash_probes = 0;
    int hash_hits = 0;
    int beta_cutoffs = 0;
    int total_moves_searched = 0;
    std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();

    void reset()
    {
        nodes_visited = 0;
        hash_probes = 0;
        hash_hits = 0;
        beta_cutoffs = 0;
        total_moves_searched = 0;
        start_time = std::chrono::steady_clock::now();
    }

    double elapsed_secs() const
    {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration<double>(now - start_time).count();
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

/// Holds one PV line result: the score and the sequence of moves.
struct PVLine {
    int score = -MAX_SCORE;
    std::vector<Move_t> moves;  // PV move sequence

    Move_t best_move() const { return moves.empty() ? Move_t(0U) : moves[0]; }
    Move_t ponder_move() const { return moves.size() >= 2 ? moves[1] : Move_t(0U); }
};

/// Skill-level noise generator for weakening play.
/// Uses a fast xorshift64 PRNG seeded per-search for determinism.
struct SkillLevel
{
    int level = 20;           // 1..20, 20 = full strength
    uint64_t prng_state = 0;  // xorshift64 state

    /// Seed the PRNG. Call once per search().
    void seed(uint64_t s)
    {
        prng_state = s ? s : 1;  // must be non-zero
    }

    /// Return a pseudo-random 64-bit value and advance state.
    uint64_t next()
    {
        uint64_t x = prng_state;
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        prng_state = x;
        return x;
    }

    /// Noise amplitude in centipawns: (20 - level) * 15.
    int noise_amplitude() const { return (20 - level) * 15; }

    /// Return a random offset in [-amplitude, +amplitude].
    int noise()
    {
        int amp = noise_amplitude();
        if (amp == 0)
        {
            return 0;
        }
        // Map next() to range [0, 2*amp] then shift to [-amp, +amp]
        int range = 2 * amp + 1;
        return static_cast<int>(next() % static_cast<uint64_t>(range)) - amp;
    }
};

class Search
{
public:
    explicit Search(Board& board);

    // Main entry point: iterative deepening search
    Move_t search(int depth,
                  int search_time = DEFAULT_SEARCH_TIME,
                  int max_nodes_visited = -1,
                  bool xboard = false,
                  int multipv_count = 1);

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

    // Access the MultiPV results from the last search
    const std::vector<PVLine>& get_multipv_results() const { return multipv_results_; }

    // Killer move accessors (for move scoring)
    Move_t get_killer(int ply, int slot) const { return killers_[ply][slot]; }

    // Verbose mode: log per-depth statistics during iterative deepening
    void set_verbose(bool v) { verbose_ = v; }

    // Output mode for iterative deepening info lines
    enum class OutputMode { NORMAL, XBOARD, UCI };
    void set_output_mode(OutputMode m) { output_mode_ = m; }

    // Abort mechanism for pondering: external code sets abort to stop search
    void set_abort(bool a) { abort_ = a; }
    bool is_aborted() const { return abort_; }

    // Pondering mode: search checks input_available() every 2048 nodes
    void set_pondering(bool p) { pondering_ = p; }
    bool is_pondering() const { return pondering_; }

    // Skill level: 1-20, 20 = full strength. Lower adds eval noise.
    void set_skill_level(int level) { skill_.level = level; }
    int get_skill_level() const { return skill_.level; }

    // Analysis mode: when true, skill noise is disabled regardless of level.
    void set_analysis_mode(bool a) { analysis_mode_ = a; }
    bool is_analysis_mode() const { return analysis_mode_; }

    // Advance the game seed so each game gets different noise patterns.
    void new_game_seed() { game_seed_counter_++; }

private:
    // Hash helpers (delegate to TT)
    int probe_hash(int depth, int alpha, int beta, Move_t& best_move);
    void record_hash(int depth, int val, int flags, Move_t best_move);

    Board& board_;
    PrincipalVariation pv_;
    TimeManager tm_;
    SearchStats stats_;
    bool verbose_ = false;
    OutputMode output_mode_ = OutputMode::NORMAL;
    bool abort_ = false;
    bool pondering_ = false;
    bool analysis_mode_ = false;

    SkillLevel skill_;
    uint64_t game_seed_counter_ = 0;

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

    // MultiPV state
    std::vector<Move_t> excluded_root_moves_;
    std::vector<PVLine> multipv_results_;
    std::vector<Move_t> extract_pv_moves() const;
};

#endif /* SEARCH_H */
