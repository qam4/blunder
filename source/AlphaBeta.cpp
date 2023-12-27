#include "Board.h"
#include "MoveGenerator.h"
#include "MoveList.h"
#include "PrincipalVariation.h"

/*
    https://www.chessprogramming.org/Alpha-Beta
    http://www.seanet.com/~brucemo/topics/alphabeta.htm
    alpha: best score that can be forced by some means
    beta: worst-case scenario for the opponent
    How does it works ?
    We maintain both a lower bound and an upper bound (called Alpha and Beta.)
    At every round, you pick the move that maximizes the lower bound
    At every round, your opponent picks the move that minimizes the upper bound
    Example: the higher the score, the better for you.
    At the parent node, the opponent found a move of score 5 (alpha=0, beta=5)
    Your turn, you are trying to find the max score.
    3: 3 > alpha, you can use that as your new lower bound for the next nodes
    8: you would score at least 8 if the opponent plays that move
    But he won't play it, since he already found a better move to play (5)
    So if you find a score higher than beta, you return beta and don't look at the next nodes.
    If you do not find a node that raises alpha,
    It means you'd not play that because you found a better move earlier
    In this case, you return alpha, the earlier found lower bound
        MIN
       /   \
      5   MAX
         / | \
        3  8  4
*/
int Board::alphabeta(int alpha, int beta, int depth, int is_pv, int can_null)
{
    MoveList list;
    int i, n, value;
    Move_t move, best_move = 0U;
    int mate_value = MATE_SCORE - search_ply_;  // will be used in mate distance pruning
    int found_pv = 0;
    int in_check = 0;

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
    in_check = MoveGenerator::in_check(*this, side_to_move());

    // NULL move pruning
    // https://web.archive.org/web/20040427014629/http://brucemo.com/compchess/programming/nullmove.htm
    if ((can_null) && (depth > 2) && !is_pv && !in_check)
    {
        int R = 2;
        do_null_move();
        value = -alphabeta(-beta, -beta + 1, depth - 1 - R, NO_PV, NO_NULL);
        undo_null_move();

        if (value >= beta)
        {
            return beta;
        }
    }

    MoveGenerator::add_all_moves(list, *this, side_to_move());
    MoveGenerator::score_moves(list, *this);
    n = list.length();

    // score PV move
    score_pv_move(list, best_move);

    for (i = 0; i < n; i++)
    {
        list.sort_moves(i);
        move = list[i];
        do_move(move);
        if (found_pv)
        {
            // Principal Variation Search
            // https://web.archive.org/web/20040427015506/brucemo.com/compchess/programming/pvs.htm
            // Wait until a move is found that improves alpha, and then searches every move after
            // that with a zero window around alpha The idea is that if the move ordering is good,
            // the 1st move that raises alpha (left most) should be the best. So, we try to test
            // that by doing a search on the other nodes that just checks if the node raises alpha
            // or not.
            value = -alphabeta(-alpha - 1, -alpha, depth - 1, NO_PV, DO_NULL);
            if ((value > alpha) && (value < beta))  // Check for failure.
            {
                value = -alphabeta(-beta, -alpha, depth - 1, IS_PV, DO_NULL);
            }
        }
        else
        {
            value = -alphabeta(-beta, -alpha, depth - 1, is_pv, DO_NULL);
        }
        undo_move(move);
        searched_moves_++;
        if (value > alpha)
        {
            found_pv = 1;
            hash_flag = HASH_EXACT;
            best_move = move;
            alpha = value;
            store_pv_move(move);

            if (value >= beta)
            {
                // store hash entry with the score equal to beta
                record_hash(depth, beta, HASH_BETA, best_move);
                return beta;  // fail hard beta-cutoff
            }
        }
    }

    // checkmate or stalemate
    if (n == 0)
    {
        if (in_check)
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
