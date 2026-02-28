/*
 * File:   Search.cpp
 *
 * Search algorithms extracted from Board.
 * Contains: iterative deepening, alphabeta, quiesce, negamax, minimax.
 */

#include <algorithm>
#include <cstring>
#include <iomanip>

#include "Search.h"

#include "MoveGenerator.h"
#include "MoveList.h"

using std::cout;
using std::dec;
using std::endl;
using std::max;
using std::min;

Search::Search(Board& board, Evaluator& evaluator, TranspositionTable& tt)
    : board_(board)
    , evaluator_(evaluator)
    , tt_(tt)
{
}

int Search::probe_hash(int depth, int alpha, int beta, Move_t& best_move)
{
    return tt_.probe(board_.get_hash(), depth, alpha, beta, best_move);
}

void Search::record_hash(int depth, int val, int flags, Move_t best_move)
{
    tt_.record(board_.get_hash(), depth, val, flags, best_move);
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
                    list.set_score(i, static_cast<U16>(bonus));
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Iterative deepening search
// ---------------------------------------------------------------------------
Move_t Search::search(int depth,
                      int search_time /*=DEFAULT_SEARCH_TIME*/,
                      int max_nodes_visited /*=-1*/,
                      bool xboard /*=false*/)
{
    tm_.start(search_time, max_nodes_visited);
    board_.set_search_ply(0);
    pv_.reset();
    std::memset(killers_, 0, sizeof(killers_));
    std::memset(history_, 0, sizeof(history_));
    stats_.reset();

    Move_t last_best_move = 0U;
    searched_moves_ = 0;
    nodes_visited_ = 0;

    int alpha = -MAX_SCORE;
    int beta = MAX_SCORE;

    for (int current_depth = 1; current_depth <= depth; current_depth++)
    {
        follow_pv_ = 1;
        max_search_ply_ = 0;

        int value = alphabeta(alpha, beta, current_depth, IS_PV, DO_NULL);

        // Aspiration window
        if ((value <= alpha) || (value >= beta))
        {
            alpha = -MAX_SCORE;
            beta = MAX_SCORE;
            follow_pv_ = 1;
            value = alphabeta(alpha, beta, current_depth, IS_PV, DO_NULL);
        }
        else
        {
            alpha = value - ASPIRATION_WINDOW;
            beta = value + ASPIRATION_WINDOW;
        }
#ifndef NDEBUG
        assert(value <= MAX_SCORE);
        assert(value >= -MAX_SCORE);
#endif
        search_best_move_ = pv_.get_best_move();

        clock_t current_time = clock();
        int elapsed_csecs = int((100 * double(current_time - tm_.start_time())) / CLOCKS_PER_SEC);

        if (xboard)
        {
            cout << current_depth << " ";
            cout << value << " ";
            cout << elapsed_csecs << " ";
            cout << nodes_visited_ << " ";
            pv_.print(board_);
            cout << endl;
        }
        else
        {
            cout << "depth=" << current_depth;
            cout << ", search ply=" << max_search_ply_;
            cout << ", searched moves=" << searched_moves_;
            cout << ", time=" << double(elapsed_csecs / 100.0) << "s";
            cout << ", score=" << value;
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
        last_best_move = search_best_move_;
        search_best_score_ = value;

        if (abs(value) >= MATE_SCORE - MAX_SEARCH_PLY)
        {
            break;
        }

        if (tm_.should_stop(nodes_visited_))
        {
            break;
        }
    }

    return last_best_move;
}

// ---------------------------------------------------------------------------
// Alpha-Beta search
// ---------------------------------------------------------------------------
int Search::alphabeta(int alpha, int beta, int depth, int is_pv, int can_null)
{
    MoveList list;
    int i, n, value;
    Move_t move, best_move = 0U;
    int search_ply = board_.get_search_ply();
    int mate_value = MATE_SCORE - search_ply;
    int found_pv = 0;
    int in_check = 0;

    pv_.set_length(search_ply, search_ply);

    // Check time left every 2048 nodes
    if (((nodes_visited_ & 2047) == 0) && tm_.is_time_over(nodes_visited_))
    {
        return 0;
    }
    nodes_visited_++;
    stats_.nodes_visited++;

    // Check for draw
    if (board_.is_draw(true))
    {
        value = DRAW_SCORE;
        return value;
    }

    int hash_flag = HASH_ALPHA;
    stats_.hash_probes++;
    if ((value = probe_hash(depth, alpha, beta, best_move)) != UNKNOWN_SCORE)
    {
        stats_.hash_hits++;
        return value;
    }

    // Leaf node
    if (depth == 0)
    {
        value = quiesce(alpha, beta);
        return value;
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
    in_check = MoveGenerator::in_check(board_, board_.side_to_move());

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

    MoveGenerator::add_all_moves(list, board_, board_.side_to_move());
    MoveGenerator::score_moves(list, board_);
    score_killers(list, search_ply);
    n = list.length();

    // score PV move
    pv_.score_move(list, search_ply, best_move, follow_pv_);

    for (i = 0; i < n; i++)
    {
        list.sort_moves(i);
        move = list[i];
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
            value = -MATE_SCORE + search_ply;
        }
        else
        {
            value = DRAW_SCORE;
        }
        return value;
    }

    record_hash(depth, alpha, hash_flag, best_move);
    return alpha;
}

// ---------------------------------------------------------------------------
// Quiescence search
// ---------------------------------------------------------------------------
int Search::quiesce(int alpha, int beta)
{
    MoveList list;
    int i, n, value;
    Move_t move;
    int search_ply = board_.get_search_ply();

    pv_.set_length(search_ply, search_ply);

    // Check time left every 2048 nodes
    if (((nodes_visited_ & 2047) == 0) && tm_.is_time_over(nodes_visited_))
    {
        return 0;
    }
    nodes_visited_++;
    stats_.nodes_visited++;

    // Check for draw
    if (board_.is_draw(true))
    {
        return DRAW_SCORE;
    }

    int stand_pat = evaluator_.side_relative_eval(board_);

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

    MoveGenerator::add_loud_moves(list, board_, board_.side_to_move());
    MoveGenerator::score_moves(list, board_);
    n = list.length();

    // score PV move
    pv_.score_move(list, search_ply, 0, follow_pv_);

    for (i = 0; i < n; i++)
    {
        list.sort_moves(i);
        move = list[i];
        if (is_capture(move))
        {
            board_.do_move(move);
            value = -quiesce(-beta, -alpha);
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
    MoveList list;
    int i, n, bestvalue, value;
    Move_t move;
    int search_ply = board_.get_search_ply();
    nodes_visited_++;

    // Leaf node — return static eval without generating moves
    if (depth == 0)
    {
        return evaluator_.side_relative_eval(board_);
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

    bestvalue = -MAX_SCORE;

    MoveGenerator::add_all_moves(list, board_, board_.side_to_move());
    n = list.length();

    for (i = 0; i < n; i++)
    {
        move = list[i];
        board_.do_move(move);
        value = -negamax(depth - 1);
        board_.undo_move(move);
        searched_moves_++;
        bestvalue = max(value, bestvalue);
    }
    return bestvalue;
}

Move_t Search::negamax_root(int depth)
{
    MoveList list;
    int i, n, bestvalue, value;
    Move_t move, bestmove = 0U;

    searched_moves_ = 0;
    nodes_visited_ = 0;

    bestvalue = -MAX_SCORE;

    MoveGenerator::add_all_moves(list, board_, board_.side_to_move());
    n = list.length();

    for (i = 0; i < n; i++)
    {
        move = list[i];
        board_.do_move(move);
        value = -negamax(depth - 1);
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
    MoveList list;
    int i, n, bestvalue, value;
    Move_t move;
    int search_ply = board_.get_search_ply();
    nodes_visited_++;

    // Leaf node — return static eval without generating moves
    if (depth == 0)
    {
        return evaluator_.evaluate(board_);
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
        bestvalue = -MAX_SCORE;

        MoveGenerator::add_all_moves(list, board_, board_.side_to_move());
        n = list.length();

        for (i = 0; i < n; i++)
        {
            move = list[i];
            board_.do_move(move);
            value = minimax(depth - 1, false);
            board_.undo_move(move);
            searched_moves_++;
            bestvalue = max(value, bestvalue);
        }
        return bestvalue;
    }
    else
    {
        bestvalue = MAX_SCORE;

        MoveGenerator::add_all_moves(list, board_, board_.side_to_move());
        n = list.length();

        for (i = 0; i < n; i++)
        {
            move = list[i];
            board_.do_move(move);
            value = minimax(depth - 1, true);
            board_.undo_move(move);
            searched_moves_++;
            bestvalue = min(value, bestvalue);
        }
        return bestvalue;
    }
}

Move_t Search::minimax_root(int depth, bool maximizing_player)
{
    MoveList list;
    int i, n, bestvalue, value;
    Move_t move, bestmove = 0U;

    searched_moves_ = 0;
    nodes_visited_ = 0;

    if (maximizing_player == true)
    {
        bestvalue = -MAX_SCORE;

        MoveGenerator::add_all_moves(list, board_, board_.side_to_move());
        n = list.length();

        for (i = 0; i < n; i++)
        {
            move = list[i];
            board_.do_move(move);
            value = minimax(depth - 1, false);
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
        bestvalue = MAX_SCORE;

        MoveGenerator::add_all_moves(list, board_, board_.side_to_move());
        n = list.length();

        for (i = 0; i < n; i++)
        {
            move = list[i];
            board_.do_move(move);
            value = minimax(depth - 1, true);
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
