#include "Board.h"
#include "MoveGenerator.h"
#include "MoveList.h"

// https://www.chessprogramming.org/Negamax
// This function returns the best score
int Board::negamax(int depth)
{
    MoveList list;
    int i, n, bestvalue, value;
    Move_t move;
    nodes_visited_++;

    if (is_game_over())
    {
        if (MoveGenerator::in_check(*this, side_to_move()))
        {
            return -MATE_SCORE + search_ply_;
        }
        else
        {
            return DRAW_SCORE;
        }
    }

    // Leaf node
    if (depth == 0)
    {
        return side_relative_eval();
    }

    bestvalue = -MAX_SCORE;

    MoveGenerator::add_all_moves(list, *this, side_to_move());
    n = list.length();

    for (i = 0; i < n; i++)
    {
        move = list[i];
        do_move(move);
        value = -negamax(depth - 1);
        undo_move(move);
        searched_moves_++;
        bestvalue = max(value, bestvalue);
    }
    return bestvalue;
}

// This function returns the best move
Move_t Board::negamax_root(int depth)
{
    MoveList list;
    int i, n, bestvalue, value;
    Move_t move, bestmove = 0U;

    // Reset searched_moves
    searched_moves_ = 0;
    nodes_visited_ = 0;

    bestvalue = -MAX_SCORE;

    MoveGenerator::add_all_moves(list, *this, side_to_move());
    // cout << Output::movelist(list, *this);
    n = list.length();

    for (i = 0; i < n; i++)
    {
        move = list[i];
        do_move(move);
        value = -negamax(depth - 1);
        undo_move(move);
        searched_moves_++;
        cout << Output::move(move, *this) << " (" << dec << value << "), ";
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
    cout << "Best move: " << Output::move(bestmove, *this) << endl;
    return bestmove;
}
