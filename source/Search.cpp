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

Search::Search(Board& board)
    : board_(board)
{
}

int Search::probe_hash(int depth, int alpha, int beta, Move_t& best_move)
{
    return board_.get_tt().probe(board_.get_hash(), depth, alpha, beta, best_move);
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

void Search::score_killers(MoveList& list, int ply)
{
    int n = list.length();
    int side = board_.side_to_move();
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
            else
            {
                // History tiebreaker for quiet moves: scale into 6..79 range
                int h = history_[side][move_from(move)][move_to(move)];
                if (h > 0)
                {
                    // Map history to 6..79 (above default quiet=5, below killer=80)
                    int bonus = 6 + (h * 73) / (h + 1000);
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
    board_.set_search_ply(0);
    pv_.reset();
    abort_ = false;
    std::memset(killers_, 0, sizeof(killers_));
    std::memset(history_, 0, sizeof(history_));
    stats_.reset();

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

    int alpha = -MAX_SCORE;
    int beta = MAX_SCORE;

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

            // Check time limit between PV iterations (not after the last one)
            if (pv_index < effective_multipv - 1)
            {
                if (tm_.should_stop(nodes_visited_) || tm_.is_time_over(nodes_visited_))
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
        int elapsed_csecs = int((100 * double(current_time - tm_.start_time())) / CLOCKS_PER_SEC);

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
int Search::alphabeta(int alpha, int beta, int depth, int is_pv, int can_null)
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
    stats_.hash_probes++;
    int value;
    if ((value = probe_hash(depth, alpha, beta, best_move)) != UNKNOWN_SCORE)
    {
        stats_.hash_hits++;
        return value;
    }

    // Leaf node
    if (depth == 0)
    {
        return quiesce(alpha, beta);
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

    // NULL move pruning
    U8 stm = board_.side_to_move();
    bool has_pieces = (board_.bitboard(KNIGHT + stm) | board_.bitboard(BISHOP + stm)
                       | board_.bitboard(ROOK + stm) | board_.bitboard(QUEEN + stm))
        != 0;
    if ((can_null) && (depth > 2) && !is_pv && !in_check && has_pieces)
    {
        int R = (depth > 6) ? 3 : 2;
        board_.do_null_move();
        value = -alphabeta(-beta, -beta + 1, depth - 1 - R, NO_PV, NO_NULL);
        board_.undo_null_move();

        if (value >= beta)
        {
            return beta;
        }
    }

    MoveList list;
    MoveGenerator::add_all_moves(list, board_, board_.side_to_move());
    MoveGenerator::score_moves(list, board_);
    score_killers(list, search_ply);
    int n = list.length();

    // score PV move
    pv_.score_move(list, search_ply, best_move, follow_pv_);

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

        bool is_quiet = !is_capture(move) && !is_promotion(move);
        bool is_killer_move =
            (move == killers_[search_ply][0]) || (move == killers_[search_ply][1]);

        board_.do_move(move);

        // Late Move Reductions
        bool do_lmr = (i >= 3) && (depth >= 3) && is_quiet && !in_check && !is_killer_move;

        if (found_pv)
        {
            if (do_lmr)
            {
                value = -alphabeta(-alpha - 1, -alpha, depth - 2, NO_PV, DO_NULL);
            }
            else
            {
                value = alpha + 1;  // ensure full-depth ZWS runs
            }
            if (value > alpha)
            {
                value = -alphabeta(-alpha - 1, -alpha, depth - 1, NO_PV, DO_NULL);
                if ((value > alpha) && (value < beta))
                {
                    value = -alphabeta(-beta, -alpha, depth - 1, IS_PV, DO_NULL);
                }
            }
        }
        else
        {
            if (do_lmr)
            {
                value = -alphabeta(-alpha - 1, -alpha, depth - 2, NO_PV, DO_NULL);
                if (value > alpha)
                {
                    value = -alphabeta(-beta, -alpha, depth - 1, is_pv, DO_NULL);
                }
            }
            else
            {
                value = -alphabeta(-beta, -alpha, depth - 1, is_pv, DO_NULL);
            }
        }
        board_.undo_move(move);
        searched_moves_++;
        stats_.total_moves_searched++;
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
