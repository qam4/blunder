#include "Board.h"
#include "MoveGenerator.h"
#include "MoveList.h"
#include "PrincipalVariation.h"

// https://www.chessprogramming.org/Quiescence_Search
int Board::quiesce(int alpha, int beta)
{
    MoveList list;
    int i, n, value;
    Move_t move, best_move = 0U;

    pv_length[search_ply_] = search_ply_;

    // Check time left every 2048 nodes
    if (((nodes_visited_ & 2047) == 0) && is_search_time_over())
    {
        return 0;
    }
    nodes_visited_++;

    // Check for draw
    if (is_draw())
    {
        return DRAW_SCORE;
    }

    int hash_flag = HASH_ALPHA;
#if TRANSPOSITION_TABLE_ENABLED
    int depth = 0;
    if ((value = probe_hash(depth, alpha, beta, best_move)) != UNKNOWN_SCORE)
    {
        return value;
    }
#endif  // TRANSPOSITION_TABLE_ENABLED

    int who2move = (side_to_move() == WHITE) ? 1 : -1;
    int stand_pat = who2move * evaluate();

    if (search_ply_ > MAX_SEARCH_PLY - 1)
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

    MoveGenerator::add_loud_moves(list, *this, side_to_move());
    MoveGenerator::score_moves(list, *this);
    n = list.length();

    // score PV move
    score_pv_move(list, 0);

    for (i = 0; i < n; i++)
    {
        list.sort_moves(i);
        move = list[i];
        if (!is_capture(move))
        {
            continue;
        }
        do_move(move);
        value = -quiesce(-beta, -alpha);
        undo_move(move);
        searched_moves_++;
        if (value > alpha)
        {
            hash_flag = HASH_EXACT;
            best_move = move;
            alpha = value;
            store_pv_move(move);
            if (value >= beta)
            {
#if TRANSPOSITION_TABLE_ENABLED
                // store hash entry with the score equal to beta
                record_hash(depth, beta, HASH_BETA, best_move);
#endif
                return beta;
            }
        }
    }

#if TRANSPOSITION_TABLE_ENABLED
    // store hash entry with the score equal to alpha
    record_hash(depth, alpha, hash_flag, best_move);
#endif  // TRANSPOSITION_TABLE_ENABLED
    return alpha;
}
