#include "Board.h"
#include "MoveGenerator.h"
#include "MoveList.h"
#include "PrincipalVariation.h"

// https://www.chessprogramming.org/Quiescence_Search
int Board::quiesce(int alpha, int beta)
{
    MoveList list;
    int i, n, value;
    Move_t move;

    pv_length[search_ply_] = search_ply_;

    // Check time left every 2048 nodes
    if (((nodes_visited_ & 2047) == 0) && is_search_time_over())
    {
        return 0;
    }
    nodes_visited_++;

    // Check for draw
    if (is_draw(true))
    {
        return DRAW_SCORE;
    }

    int stand_pat = side_relative_eval();

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
        if (is_capture(move))
        {
            do_move(move);
            value = -quiesce(-beta, -alpha);
            undo_move(move);
            searched_moves_++;
            if (value > alpha)
            {
                alpha = value;
                store_pv_move(move);
                if (value >= beta)
                {
                    return beta;
                }
            }
        }
    }

    return alpha;
}
