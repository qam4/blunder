/*
 * File:   Search.cpp
 *
 * Search algorithms extracted from Board.
 * Contains: iterative deepening, alphabeta, quiesce, negamax, minimax.
 */

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>

#include "Search.h"

#include "InputDetect.h"
#include "MoveGenerator.h"
#include "MoveList.h"
#include "ValidateMove.h"

using std::cout;
using std::dec;
using std::endl;
using std::max;
using std::min;

// Reverse futility pruning margin per depth (centipawns)
constexpr int RFP_MARGIN_PER_DEPTH = 120;

// Futility pruning margins indexed by depth (centipawns)
constexpr int FUTILITY_MARGIN[3] = { 0, 200, 500 };

// Late move pruning threshold: skip quiet moves after this many have been searched
constexpr int lmp_threshold(int depth)
{
    return 3 + depth * 4;
}

// Singular extension parameters
constexpr int SE_MIN_DEPTH = 8;
constexpr int SE_MARGIN = 50;

// Static member definitions for LMR lookup table
int Search::lmr_table_[MAX_SEARCH_PLY][64] = {};
bool Search::lmr_initialized_ = false;

void Search::init_lmr_table()
{
    if (lmr_initialized_)
    {
        return;
    }
    // Default: depth=0 or move_index=0 → reduction of 1
    for (int d = 0; d < MAX_SEARCH_PLY; d++)
    {
        lmr_table_[d][0] = 1;
    }
    for (int i = 0; i < 64; i++)
    {
        lmr_table_[0][i] = 1;
    }
    // Logarithmic formula for d >= 1, i >= 1
    for (int d = 1; d < MAX_SEARCH_PLY; d++)
    {
        for (int i = 1; i < 64; i++)
        {
            lmr_table_[d][i] = max(1, static_cast<int>(floor(log(d) * log(i) / 2.0)));
        }
    }
    lmr_initialized_ = true;
}

Search::Search(Board& board)
    : board_(board)
{
}

int Search::probe_hash(int depth,
                       int alpha,
                       int beta,
                       Move_t& best_move,
                       int* tt_depth_out,
                       int* tt_flags_out,
                       int* tt_value_out)
{
    return board_.get_tt().probe(
        board_.get_hash(), depth, alpha, beta, best_move, tt_depth_out, tt_flags_out, tt_value_out);
}

void Search::record_hash(int depth, int val, int flags, Move_t best_move)
{
    board_.get_tt().record(board_.get_hash(), depth, val, flags, best_move);
}

void Search::store_killer(int ply, Move_t move)
{
    // Don't store if it's already killer[0]
    if (killers_[ply][0] != move)
    {
        killers_[ply][1] = killers_[ply][0];
        killers_[ply][0] = move;
    }
}

void Search::score_quiet_moves(MoveList& list, int ply, Move_t prev_move)
{
    int n = list.length();
    int side = board_.side_to_move();

    // Look up the countermove for the previous move (if any)
    Move_t countermove = 0U;
    if (prev_move != 0U)
    {
        int prev_side = side ^ 1;  // side that made the previous move
        countermove = countermoves_[prev_side][move_from(prev_move)][move_to(prev_move)];
    }

    for (int i = 0; i < n; i++)
    {
        Move_t move = list[i];
        if (!is_capture(move) && !is_promotion(move))
        {
            if (move == killers_[ply][0])
            {
                list.set_score(i, 90);
            }
            else if (move == killers_[ply][1])
            {
                list.set_score(i, 80);
            }
            else if (countermove != 0U && move == countermove)
            {
                list.set_score(i, 70);
            }
            else
            {
                // History tiebreaker for quiet moves: scale into 6..69 range
                int h = history_[side][move_from(move)][move_to(move)];
                if (h > 0)
                {
                    // Map history to 6..69 (above default quiet=5, below countermove=70)
                    int bonus = 6 + (h * 63) / (h + 1000);
                    list.set_score(i, bonus);
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Iterative deepening search
// ---------------------------------------------------------------------------
// Helper: format a move in UCI coordinate notation
static std::string format_move_uci(Move_t m, const Board& board)
{
    Move mv(m);
    std::string s;
    if (mv.is_castle())
    {
        bool white = (board.side_to_move() == WHITE);
        if (mv.flags() & (KING_CASTLE >> FLAGS_SHIFT))
        {
            s = white ? "e1g1" : "e8g8";
        }
        else
        {
            s = white ? "e1c1" : "e8c8";
        }
    }
    else
    {
        int from = mv.from();
        int to = mv.to();
        s += static_cast<char>('a' + (from % 8));
        s += static_cast<char>('1' + (from / 8));
        s += static_cast<char>('a' + (to % 8));
        s += static_cast<char>('1' + (to / 8));
        if (mv.is_promotion())
        {
            U8 promo = mv.promote_to();
            switch (promo)
            {
                case QUEEN:
                    s += 'q';
                    break;
                case ROOK:
                    s += 'r';
                    break;
                case BISHOP:
                    s += 'b';
                    break;
                case KNIGHT:
                    s += 'n';
                    break;
                default:
                    s += 'q';
                    break;
            }
        }
    }
    return s;
}

Move_t Search::search(int depth,
                      int search_time /*=DEFAULT_SEARCH_TIME*/,
                      int max_nodes_visited /*=-1*/,
                      bool xboard /*=false*/,
                      int multipv_count /*=1*/)
{
    // If search_time is -1 and max_nodes is -1, assume allocate() was already
    // called by the caller (smart time management). Otherwise use legacy start().
    if (search_time != -1 || max_nodes_visited != -1)
    {
        tm_.start(search_time, max_nodes_visited);
    }

    // Initialize LMR lookup table once (static init guard)
    init_lmr_table();

    board_.set_search_ply(0);
    pv_.reset();
    abort_ = false;
    std::memset(killers_, 0, sizeof(killers_));
    std::memset(history_, 0, sizeof(history_));
    std::memset(countermoves_, 0, sizeof(countermoves_));
    stats_.reset();

    // Advance TT generation so stale entries from previous searches can be replaced
    board_.get_tt().new_generation();

    // Seed skill noise PRNG: deterministic per position + game, but varies
    // across games via game_seed_counter_.
    skill_.seed(board_.get_hash() ^ (game_seed_counter_ * 0x9E3779B97F4A7C15ULL));

    Move_t last_best_move = 0U;
    searched_moves_ = 0;
    nodes_visited_ = 0;

    // Count legal moves to cap MultiPV count
    MoveList legal_moves;
    MoveGenerator::add_all_moves(legal_moves, board_, board_.side_to_move());
    int num_legal_moves = legal_moves.length();
    int effective_multipv = min(multipv_count, num_legal_moves);
    if (effective_multipv < 1)
        effective_multipv = 1;

    // Initialize multipv_results_ for the effective count
    multipv_results_.clear();
    multipv_results_.resize(effective_multipv);

    // Easy move: if only one legal move, return it immediately (no search needed).
    // Skip for fixed-node/depth benchmarks where the search output matters.
    if (num_legal_moves == 1 && tm_.soft_limit() > 0)
    {
        search_best_move_ = legal_moves[0];
        search_best_score_ = 0;
        multipv_results_[0].score = 0;
        multipv_results_[0].moves = { legal_moves[0] };
        return legal_moves[0];
    }

    int alpha = -MAX_SCORE;
    int beta = MAX_SCORE;
    int best_move_stability = 0;  // consecutive iterations with same best move

    for (int current_depth = 1; current_depth <= depth; current_depth++)
    {
        // Age history table to prevent unbounded growth
        if (current_depth > 1)
        {
            for (int s = 0; s < 2; ++s)
            {
                for (int f = 0; f < 64; ++f)
                {
                    for (int t = 0; t < 64; ++t)
                    {
                        history_[s][f][t] /= 2;
                    }
                }
            }
        }

        // --- MultiPV loop ---
        excluded_root_moves_.clear();
        std::vector<PVLine> depth_results(effective_multipv);
        bool depth_completed = true;

        for (int pv_index = 0; pv_index < effective_multipv; pv_index++)
        {
            // For the first PV line, use aspiration windows; for subsequent
            // lines, use full window since we don't have a prior score.
            int pv_alpha = (pv_index == 0) ? alpha : -MAX_SCORE;
            int pv_beta = (pv_index == 0) ? beta : MAX_SCORE;

            // Reset abort flag before each PV iteration — a previous PV line
            // may have hit the hard time limit inside alphabeta, setting abort_.
            // Without this reset, subsequent PV lines return immediately with 0.
            abort_ = false;

            follow_pv_ = 1;
            max_search_ply_ = 0;

            int value = alphabeta(pv_alpha, pv_beta, current_depth, IS_PV, DO_NULL);

            // If search was aborted, stop and keep results from last fully completed depth
            if (abort_)
            {
                depth_completed = false;
                break;
            }

            // Aspiration window re-search (only for first PV line)
            if (pv_index == 0)
            {
                if ((value <= pv_alpha) || (value >= pv_beta))
                {
                    pv_alpha = -MAX_SCORE;
                    pv_beta = MAX_SCORE;
                    follow_pv_ = 1;
                    value = alphabeta(pv_alpha, pv_beta, current_depth, IS_PV, DO_NULL);

                    if (abort_)
                    {
                        depth_completed = false;
                        break;
                    }
                }
                else
                {
                    alpha = value - ASPIRATION_WINDOW;
                    beta = value + ASPIRATION_WINDOW;
                }
            }

            assert(value <= MAX_SCORE);
            assert(value >= -MAX_SCORE);

            // Store score + PV for this iteration
            depth_results[pv_index].score = value;
            depth_results[pv_index].moves = extract_pv_moves();

            // Add root move to exclusion set for subsequent PV iterations
            Move_t root_move = depth_results[pv_index].best_move();
            if (root_move != 0U)
            {
                excluded_root_moves_.push_back(root_move);
            }

            // Check time limit between PV iterations (not after the last one).
            // Skip this check entirely for MultiPV > 1 so all requested lines
            // complete at each depth. Time is still checked between depths
            // (via should_stop after the MultiPV loop) and inside alphabeta
            // (via is_time_over every 2048 nodes).
            if (effective_multipv == 1 && pv_index < effective_multipv - 1)
            {
                if (tm_.should_stop(nodes_visited_))
                {
                    depth_completed = false;
                    break;
                }
            }
        }

        // If depth was not fully completed, keep results from last completed depth
        if (!depth_completed)
        {
            break;
        }

        // Sort results by descending score
        std::sort(depth_results.begin(),
                  depth_results.end(),
                  [](const PVLine& a, const PVLine& b) { return a.score > b.score; });

        // Commit this depth's results
        multipv_results_ = depth_results;

        // Update search_best_move_ and search_best_score_ from top PV line
        search_best_move_ = multipv_results_[0].best_move();
        search_best_score_ = multipv_results_[0].score;

        clock_t current_time = clock();
        int elapsed_csecs = tm_.elapsed_us() / 10000;

        if (xboard)
        {
            cout << current_depth << " ";
            cout << multipv_results_[0].score << " ";
            cout << elapsed_csecs << " ";
            cout << nodes_visited_ << " ";
            pv_.print(board_);
            cout << endl;
        }
        else if (output_mode_ == OutputMode::UCI)
        {
            int elapsed_ms = elapsed_csecs * 10;

            // Output one info line per PV line
            for (int pv_idx = 0; pv_idx < effective_multipv; pv_idx++)
            {
                const PVLine& pvline = multipv_results_[pv_idx];
                cout << "info depth " << current_depth << " score cp " << pvline.score << " nodes "
                     << nodes_visited_ << " nps " << stats_.nps() << " time " << elapsed_ms;

                // Include multipv field only when multipv_count > 1
                if (multipv_count > 1)
                {
                    cout << " multipv " << (pv_idx + 1);
                }

                cout << " pv";
                Board copy = board_;
                for (const Move_t& m : pvline.moves)
                {
                    if (!is_valid_move(m, copy, false))
                    {
                        break;
                    }
                    cout << " " << format_move_uci(m, copy);
                    copy.do_move(m);
                }
                cout << endl;
            }
        }
        else
        {
            cout << "depth=" << current_depth;
            cout << ", search ply=" << max_search_ply_;
            cout << ", searched moves=" << searched_moves_;
            cout << ", time=" << double(elapsed_csecs / 100.0) << "s";
            cout << ", score=" << multipv_results_[0].score;
            cout << ", pv=";
            pv_.print(board_);
            cout << endl;

            if (verbose_)
            {
                cout << "  nodes=" << stats_.nodes_visited;
                cout << ", nps=" << stats_.nps();
                cout << ", hash=" << std::fixed << std::setprecision(1)
                     << (stats_.hash_hit_rate() * 100.0) << "%";
                cout << " (" << stats_.hash_hits << "/" << stats_.hash_probes << ")";
                cout << ", cutoffs=" << stats_.beta_cutoffs;
                cout << ", bf=" << std::setprecision(2) << stats_.branching_factor(current_depth);
                cout << std::defaultfloat << endl;
            }
        }
        // Instability detection: if best move changed, extend time
        if (last_best_move != 0U && search_best_move_ != last_best_move && current_depth >= 4)
        {
            tm_.extend_for_instability();
            best_move_stability = 0;
        }
        else if (last_best_move != 0U && search_best_move_ == last_best_move)
        {
            best_move_stability++;
        }

        last_best_move = search_best_move_;

        // Score-based time adjustment after a few depths
        if (current_depth >= 6)
        {
            tm_.adjust_for_score(search_best_score_);
        }

        if (abs(search_best_score_) >= MATE_SCORE - MAX_SEARCH_PLY)
        {
            break;
        }

        // Easy move: if best move has been stable for 5+ iterations at depth >= 8,
        // reduce the soft time limit to move faster. This saves time for harder
        // positions later. Only applies in single-PV mode.
        if (effective_multipv == 1 && best_move_stability >= 5 && current_depth >= 8)
        {
            tm_.reduce_for_easy_move();
        }

        if (tm_.should_stop(nodes_visited_))
        {
            break;
        }
    }

    // Clear exclusion set after search completes
    excluded_root_moves_.clear();

    // Fallback: if search found no move (e.g. time expired before depth 1
    // completed), pick the first legal move so we never return 0.
    if (last_best_move == 0U)
    {
        if (num_legal_moves > 0)
        {
            last_best_move = legal_moves[0];
        }
    }

    return last_best_move;
}

// ---------------------------------------------------------------------------
// Extract PV moves from the PV table into a vector
// ---------------------------------------------------------------------------
std::vector<Move_t> Search::extract_pv_moves() const
{
    std::vector<Move_t> moves;
    Board copy = board_;
    for (int i = 0; i < pv_.length(); i++)
    {
        Move_t m = pv_.get_move(i);
        if (!is_valid_move(m, copy, false))
        {
            break;
        }
        moves.push_back(m);
        copy.do_move(m);
    }
    return moves;
}

// ---------------------------------------------------------------------------
// Alpha-Beta search
// ---------------------------------------------------------------------------
int Search::alphabeta(int alpha, int beta, int depth, int is_pv, int can_null, Move_t prev_move)
{
    int search_ply = board_.get_search_ply();
    int mate_value = MATE_SCORE - search_ply;
    int found_pv = 0;

    pv_.set_length(search_ply, search_ply);

    // Check time left or abort flag every 2048 nodes
    if ((nodes_visited_ & 2047) == 0)
    {
        if (abort_)
        {
            return 0;
        }
        if (pondering_ && input_available())
        {
            abort_ = true;
            return 0;
        }
        if (tm_.is_time_over(nodes_visited_))
        {
            abort_ = true;
            return 0;
        }
    }
    nodes_visited_++;
    stats_.nodes_visited++;

    // Check for draw
    if (board_.is_draw(true))
    {
        return DRAW_SCORE;
    }

    int hash_flag = HASH_ALPHA;
    Move_t best_move = 0U;
    int tt_depth = 0;
    int tt_flags = 0;
    int tt_value = 0;
    stats_.hash_probes++;
    int value;
    if ((value = probe_hash(depth, alpha, beta, best_move, &tt_depth, &tt_flags, &tt_value))
        != UNKNOWN_SCORE)
    {
        // Don't use TT cutoff at the root when MultiPV is active — we need
        // to run the move loop so excluded_root_moves_ can filter out moves
        // that were already chosen as better PV lines.
        if (search_ply != 0 || excluded_root_moves_.empty())
        {
            stats_.hash_hits++;
            return value;
        }
    }

    // Leaf node
    if (depth == 0)
    {
        int q = quiesce(alpha, beta);
        // Apply skill noise in the main search leaf, not in quiescence itself,
        // so the engine still resolves tactical sequences cleanly.
        if (!analysis_mode_ && skill_.level < 20)
        {
            q += skill_.noise();
        }
        return q;
    }

    // mate distance pruning
    if (alpha < -mate_value)
    {
        alpha = -mate_value;
    }
    if (beta > mate_value - 1)
    {
        beta = mate_value - 1;
    }
    if (alpha >= beta)
    {
        return alpha;
    }

    // Are we in check?
    int in_check = MoveGenerator::in_check(board_, board_.side_to_move());

    // Check extension: extend search by 1 ply when in check
    int extension = in_check ? 1 : 0;

    // NULL move pruning
    U8 stm = board_.side_to_move();
    bool has_pieces = (board_.bitboard(KNIGHT + stm) | board_.bitboard(BISHOP + stm)
                       | board_.bitboard(ROOK + stm) | board_.bitboard(QUEEN + stm))
        != 0;
    if ((can_null) && (depth > 2) && !is_pv && !in_check && has_pieces)
    {
        int R = (depth > 6) ? 3 : 2;
        board_.do_null_move();
        value = -alphabeta(-beta, -beta + 1, depth - 1 - R, NO_PV, NO_NULL, 0U);
        board_.undo_null_move();

        if (value >= beta)
        {
            return beta;
        }
    }

    // Static eval for pruning decisions (declared here so futility pruning in
    // the move loop can reuse it without recomputing).
    int static_eval = -MAX_SCORE;
    bool static_eval_computed = false;

    // Reverse futility pruning: if static eval is far above beta at low depth,
    // return beta immediately (the position is so good we can skip searching).
    if (depth >= 1 && depth <= 3 && !in_check && !is_pv)
    {
        static_eval = board_.get_evaluator().side_relative_eval(board_);
        static_eval_computed = true;
        if (static_eval - depth * RFP_MARGIN_PER_DEPTH >= beta)
        {
            return beta;
        }
    }

    // Singular extensions: if the TT move is significantly better than all
    // alternatives, extend it by 1 ply to resolve it more deeply.
    Move_t singular_move = 0U;
    if (depth >= SE_MIN_DEPTH && search_ply > 0 && !singular_excluded_[search_ply]
        && best_move != 0U && (tt_flags == HASH_BETA || tt_flags == HASH_EXACT)
        && tt_depth >= depth - 3)
    {
        singular_excluded_[search_ply] = true;
        int se_beta = tt_value - SE_MARGIN;
        int se_value = alphabeta(se_beta - 1, se_beta, depth / 2, NO_PV, DO_NULL, prev_move);
        singular_excluded_[search_ply] = false;

        if (se_value < se_beta)
        {
            singular_move = best_move;
        }
    }

    MoveList list;
    MoveGenerator::add_all_moves(list, board_, board_.side_to_move());
    MoveGenerator::score_moves(list, board_);
    score_quiet_moves(list, search_ply, prev_move);
    int n = list.length();

    // score PV move
    pv_.score_move(list, search_ply, best_move, follow_pv_);

    int quiet_moves_searched = 0;

    for (int i = 0; i < n; i++)
    {
        list.sort_moves(i);
        Move_t move = list[i];

        // MultiPV: skip root moves that were already chosen as better PV lines
        if (search_ply == 0 && !excluded_root_moves_.empty())
        {
            bool excluded = false;
            for (Move_t ex : excluded_root_moves_)
            {
                if (move == ex)
                {
                    excluded = true;
                    break;
                }
            }
            if (excluded)
                continue;
        }

        // Singular extension: skip the TT move during verification search
        if (singular_excluded_[search_ply] && move == best_move)
        {
            continue;
        }

        bool is_quiet = !is_capture(move) && !is_promotion(move);
        bool is_killer_move =
            (move == killers_[search_ply][0]) || (move == killers_[search_ply][1]);

        // Futility pruning: skip quiet moves at low depths when static eval
        // plus a margin is still below alpha (the move is unlikely to raise alpha).
        // Exception: never prune moves that give check.
        if (depth <= 2 && !in_check && !is_pv && is_quiet)
        {
            if (!static_eval_computed)
            {
                static_eval = board_.get_evaluator().side_relative_eval(board_);
                static_eval_computed = true;
            }
            if (static_eval + FUTILITY_MARGIN[depth] <= alpha)
            {
                // Make the move to check if it gives check
                board_.do_move(move);
                bool gives_check = MoveGenerator::in_check(board_, board_.side_to_move());
                board_.undo_move(move);
                if (!gives_check)
                {
                    continue;
                }
            }
        }

        // Late move pruning: skip quiet moves after enough have been searched
        // at low depths, since they are unlikely to improve alpha.
        if (depth >= 1 && depth <= 3 && !in_check && !is_pv && is_quiet && !is_killer_move
            && quiet_moves_searched > lmp_threshold(depth))
        {
            continue;
        }

        board_.do_move(move);

        // Per-move extension: check extension + singular extension for TT move
        int move_extension = extension;
        if (singular_move != 0U && move == singular_move)
        {
            move_extension += 1;
        }

        // Late Move Reductions
        bool do_lmr = (i >= 3) && (depth >= 3) && is_quiet && !in_check && !is_killer_move;

        // Compute logarithmic LMR reduced depth from lookup table
        int lmr_reduced_depth = 1;  // default minimum
        if (do_lmr)
        {
            int reduction = lmr_table_[min(depth, MAX_SEARCH_PLY - 1)][min(i, 63)];
            lmr_reduced_depth = max(1, depth - 1 - reduction) + move_extension;
        }

        if (found_pv)
        {
            if (do_lmr)
            {
                value = -alphabeta(-alpha - 1, -alpha, lmr_reduced_depth, NO_PV, DO_NULL, move);
            }
            else
            {
                value = alpha + 1;  // ensure full-depth ZWS runs
            }
            if (value > alpha)
            {
                value = -alphabeta(
                    -alpha - 1, -alpha, depth - 1 + move_extension, NO_PV, DO_NULL, move);
                if ((value > alpha) && (value < beta))
                {
                    value =
                        -alphabeta(-beta, -alpha, depth - 1 + move_extension, IS_PV, DO_NULL, move);
                }
            }
        }
        else
        {
            if (do_lmr)
            {
                value = -alphabeta(-alpha - 1, -alpha, lmr_reduced_depth, NO_PV, DO_NULL, move);
                if (value > alpha)
                {
                    value =
                        -alphabeta(-beta, -alpha, depth - 1 + move_extension, is_pv, DO_NULL, move);
                }
            }
            else
            {
                value = -alphabeta(-beta, -alpha, depth - 1 + move_extension, is_pv, DO_NULL, move);
            }
        }
        board_.undo_move(move);
        searched_moves_++;
        stats_.total_moves_searched++;
        if (is_quiet)
        {
            quiet_moves_searched++;
        }
        if (value > alpha)
        {
            found_pv = 1;
            hash_flag = HASH_EXACT;
            best_move = move;
            alpha = value;
            pv_.store_move(search_ply, move);

            if (value >= beta)
            {
                stats_.beta_cutoffs++;
                if (!is_capture(move))
                {
                    store_killer(search_ply, move);
                    int side = board_.side_to_move();
                    history_[side][move_from(move)][move_to(move)] += depth * depth;

                    // Record countermove: this quiet move refutes the previous move
                    if (prev_move != 0U && !is_promotion(move))
                    {
                        int prev_side = side ^ 1;
                        countermoves_[prev_side][move_from(prev_move)][move_to(prev_move)] = move;
                    }
                }
                record_hash(depth, beta, HASH_BETA, best_move);
                return beta;
            }
        }
    }

    // checkmate or stalemate
    if (n == 0)
    {
        if (in_check)
        {
            return -MATE_SCORE + search_ply;
        }
        else
        {
            return DRAW_SCORE;
        }
    }

    record_hash(depth, alpha, hash_flag, best_move);
    return alpha;
}

// ---------------------------------------------------------------------------
// Quiescence search
// ---------------------------------------------------------------------------
int Search::quiesce(int alpha, int beta)
{
    int search_ply = board_.get_search_ply();

    pv_.set_length(search_ply, search_ply);

    // Check time left or abort flag every 2048 nodes
    if ((nodes_visited_ & 2047) == 0)
    {
        if (abort_)
        {
            return 0;
        }
        if (pondering_ && input_available())
        {
            abort_ = true;
            return 0;
        }
        if (tm_.is_time_over(nodes_visited_))
        {
            abort_ = true;
            return 0;
        }
    }
    nodes_visited_++;
    stats_.nodes_visited++;

    // Check for draw
    if (board_.is_draw(true))
    {
        return DRAW_SCORE;
    }

    int stand_pat = board_.get_evaluator().side_relative_eval(board_);

    if (search_ply > MAX_SEARCH_PLY - 1)
    {
        return stand_pat;
    }

    if (stand_pat >= beta)
    {
        return beta;
    }
    if (alpha < stand_pat)
    {
        alpha = stand_pat;
    }

    MoveList list;
    MoveGenerator::add_loud_moves(list, board_, board_.side_to_move());
    MoveGenerator::score_moves(list, board_);
    int n = list.length();

    // score PV move
    pv_.score_move(list, search_ply, 0, follow_pv_);

    for (int i = 0; i < n; i++)
    {
        list.sort_moves(i);
        Move_t move = list[i];
        if (is_capture(move))
        {
            // SEE pruning: skip captures that lose material
            if (MoveGenerator::see(board_, move) < 0)
            {
                continue;
            }

            board_.do_move(move);
            int value = -quiesce(-beta, -alpha);
            board_.undo_move(move);
            searched_moves_++;
            stats_.total_moves_searched++;
            if (value > alpha)
            {
                alpha = value;
                pv_.store_move(search_ply, move);
                if (value >= beta)
                {
                    stats_.beta_cutoffs++;
                    return beta;
                }
            }
        }
        else if (is_promotion(move))
        {
            // Search promotions (generated by add_loud_moves for push-promotions)
            board_.do_move(move);
            int value = -quiesce(-beta, -alpha);
            board_.undo_move(move);
            searched_moves_++;
            stats_.total_moves_searched++;
            if (value > alpha)
            {
                alpha = value;
                pv_.store_move(search_ply, move);
                if (value >= beta)
                {
                    stats_.beta_cutoffs++;
                    return beta;
                }
            }
        }
    }

    return alpha;
}

// ---------------------------------------------------------------------------
// NegaMax
// ---------------------------------------------------------------------------
int Search::negamax(int depth)
{
    int search_ply = board_.get_search_ply();
    nodes_visited_++;

    // Leaf node — return static eval without generating moves
    if (depth == 0)
    {
        return board_.get_evaluator().side_relative_eval(board_);
    }

    if (board_.is_game_over())
    {
        if (MoveGenerator::in_check(board_, board_.side_to_move()))
        {
            return -MATE_SCORE + search_ply;
        }
        else
        {
            return DRAW_SCORE;
        }
    }

    int bestvalue = -MAX_SCORE;

    MoveList list;
    MoveGenerator::add_all_moves(list, board_, board_.side_to_move());
    int n = list.length();

    for (int i = 0; i < n; i++)
    {
        Move_t move = list[i];
        board_.do_move(move);
        int value = -negamax(depth - 1);
        board_.undo_move(move);
        searched_moves_++;
        bestvalue = max(value, bestvalue);
    }
    return bestvalue;
}

Move_t Search::negamax_root(int depth)
{
    Move_t bestmove = 0U;

    searched_moves_ = 0;
    nodes_visited_ = 0;

    int bestvalue = -MAX_SCORE;

    MoveList list;
    MoveGenerator::add_all_moves(list, board_, board_.side_to_move());
    int n = list.length();

    for (int i = 0; i < n; i++)
    {
        Move_t move = list[i];
        board_.do_move(move);
        int value = -negamax(depth - 1);
        board_.undo_move(move);
        searched_moves_++;
        cout << Output::move(move, board_) << " (" << dec << value << "), ";
        if ((i % 4 == 3) || (i == n - 1))
        {
            cout << endl;
        }
        if (value > bestvalue)
        {
            bestvalue = value;
            bestmove = move;
        }
    }
    cout << "Best move: " << Output::move(bestmove, board_) << endl;
    return bestmove;
}

// ---------------------------------------------------------------------------
// MiniMax
// ---------------------------------------------------------------------------
int Search::minimax(int depth, bool maximizing_player)
{
    int search_ply = board_.get_search_ply();
    nodes_visited_++;

    // Leaf node — return static eval without generating moves
    if (depth == 0)
    {
        return board_.get_evaluator().evaluate(board_);
    }

    if (board_.is_game_over())
    {
        if (MoveGenerator::in_check(board_, board_.side_to_move()))
        {
            if (maximizing_player == true)
            {
                return -MATE_SCORE + search_ply;
            }
            else
            {
                return MATE_SCORE - search_ply;
            }
        }
        else
        {
            return DRAW_SCORE;
        }
    }

    if (maximizing_player == true)
    {
        int bestvalue = -MAX_SCORE;

        MoveList list;
        MoveGenerator::add_all_moves(list, board_, board_.side_to_move());
        int n = list.length();

        for (int i = 0; i < n; i++)
        {
            Move_t move = list[i];
            board_.do_move(move);
            int value = minimax(depth - 1, false);
            board_.undo_move(move);
            searched_moves_++;
            bestvalue = max(value, bestvalue);
        }
        return bestvalue;
    }
    else
    {
        int bestvalue = MAX_SCORE;

        MoveList list;
        MoveGenerator::add_all_moves(list, board_, board_.side_to_move());
        int n = list.length();

        for (int i = 0; i < n; i++)
        {
            Move_t move = list[i];
            board_.do_move(move);
            int value = minimax(depth - 1, true);
            board_.undo_move(move);
            searched_moves_++;
            bestvalue = min(value, bestvalue);
        }
        return bestvalue;
    }
}

Move_t Search::minimax_root(int depth, bool maximizing_player)
{
    Move_t bestmove = 0U;

    searched_moves_ = 0;
    nodes_visited_ = 0;

    if (maximizing_player == true)
    {
        int bestvalue = -MAX_SCORE;

        MoveList list;
        MoveGenerator::add_all_moves(list, board_, board_.side_to_move());
        int n = list.length();

        for (int i = 0; i < n; i++)
        {
            Move_t move = list[i];
            board_.do_move(move);
            int value = minimax(depth - 1, false);
            board_.undo_move(move);
            searched_moves_++;
            cout << Output::move(move, board_) << " (" << dec << value << "), ";
            if ((i % 4 == 3) || (i == n - 1))
            {
                cout << endl;
            }
            if (value > bestvalue)
            {
                bestvalue = value;
                bestmove = move;
            }
        }
    }
    else
    {
        int bestvalue = MAX_SCORE;

        MoveList list;
        MoveGenerator::add_all_moves(list, board_, board_.side_to_move());
        int n = list.length();

        for (int i = 0; i < n; i++)
        {
            Move_t move = list[i];
            board_.do_move(move);
            int value = minimax(depth - 1, true);
            board_.undo_move(move);
            searched_moves_++;
            cout << Output::move(move, board_) << " (" << dec << value << "), ";
            if ((i % 4 == 3) || (i == n - 1))
            {
                cout << endl;
            }
            if (value < bestvalue)
            {
                bestvalue = value;
                bestmove = move;
            }
        }
    }
    return bestmove;
}
