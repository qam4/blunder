#include "Board.h"
#include "MoveGenerator.h"
#include "MoveList.h"
#include "PrincipalVariation.h"

// https://www.chessprogramming.org/Alpha-Beta
// http://www.seanet.com/~brucemo/topics/alphabeta.htm
// alpha: best score that can be forced by some means
// beta: worst-case scenario for the opponent
int Board::alphabeta(int alpha, int beta, int depth)
{
    MoveList list;
    int i, n, value;
    Move_t move, best_move = 0;

    pv_length[search_ply_] = search_ply_;

    // Check time left every 2048 moves
    if ((searched_moves_ & 2047) && is_search_time_over())
    {
        return 0;
    }

    // Check for draw
    if (is_draw())
    {
        value = DRAW_SCORE;
        return value;
    }

    int hash_flag = HASH_ALPHA;
    if ((value = probe_hash(depth, alpha, beta, best_move)) != UNKNOWN_SCORE)
    {
        return value;
    }

    // Leaf node
    if (depth == 0)
    {
        value = quiesce(alpha, beta);
        return value;
    }

    MoveGenerator::add_all_moves(list, *this, side_to_move());
    MoveGenerator::score_moves(list, *this);
    n = list.length();

    // sort PV move
    sort_pv_move(list, best_move);

    for (i = 0; i < n; i++)
    {
        searched_moves_++;
        list.sort_moves(i);
        move = list[i];
        do_move(move);
        value = -alphabeta(-beta, -alpha, depth - 1);
        undo_move(move);
        if (value >= beta)
        {
            // store hash entry with the score equal to beta
            record_hash(depth, beta, HASH_BETA, best_move);
            return beta;  // fail hard beta-cutoff
        }
        if (value > alpha)
        {
            hash_flag = HASH_EXACT;
            best_move = move;
            alpha = value;
            store_pv_move(move);
        }
    }

    // checkmate or stalemate
    if (n == 0)
    {
        if (MoveGenerator::in_check(*this, side_to_move()))
        {
            value = -MATE_SCORE + search_ply_;
        }
        else
        {
            value = DRAW_SCORE;
        }
        return value;
    }

    // store hash entry with the score equal to alpha
    record_hash(depth, alpha, hash_flag, best_move);
    return alpha;
}
