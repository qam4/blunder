#include "Board.h"
#include "MoveGenerator.h"
#include "MoveList.h"

// https://www.chessprogramming.org/Minimax
// This function returns the best score
int Board::minimax(int depth, bool maximizing_player)
{
    MoveList list;
    int i, n, bestvalue, value;
    Move_t move;
    nodes_visited_++;

    if (is_game_over())
    {
        if (MoveGenerator::in_check(*this, side_to_move()))
        {
            if (maximizing_player == true)
            {
                return -MATE_SCORE + search_ply_;
            }
            else
            {
                return MATE_SCORE - search_ply_;
            }
        }
        else
        {
            return DRAW_SCORE;
        }
    }

    // Leaf node
    if (depth == 0)
    {
        return evaluator_.evaluate(*this);
    }

    if (maximizing_player == true)
    {
        bestvalue = -MAX_SCORE;

        MoveGenerator::add_all_moves(list, *this, side_to_move());
        n = list.length();

        for (i = 0; i < n; i++)
        {
            move = list[i];
            do_move(move);
            value = minimax(depth - 1, false);
            undo_move(move);
            searched_moves_++;
            bestvalue = max(value, bestvalue);
        }
        return bestvalue;
    }
    else
    {
        bestvalue = MAX_SCORE;

        MoveGenerator::add_all_moves(list, *this, side_to_move());
        n = list.length();

        for (i = 0; i < n; i++)
        {
            move = list[i];
            do_move(move);
            value = minimax(depth - 1, true);
            undo_move(move);
            searched_moves_++;
            bestvalue = min(value, bestvalue);
        }
        return bestvalue;
    }
}

// This function returns the best move
Move_t Board::minimax_root(int depth, bool maximizing_player)
{
    MoveList list;
    int i, n, bestvalue, value;
    Move_t move, bestmove = 0U;

    // Reset searched_moves
    searched_moves_ = 0;
    nodes_visited_ = 0;

    if (maximizing_player == true)
    {
        bestvalue = -MAX_SCORE;

        MoveGenerator::add_all_moves(list, *this, side_to_move());
        // cout << Output::movelist(list, *this);
        n = list.length();

        for (i = 0; i < n; i++)
        {
            move = list[i];
            do_move(move);
            value = minimax(depth - 1, false);
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
    }
    else
    {
        bestvalue = MAX_SCORE;

        MoveGenerator::add_all_moves(list, *this, side_to_move());
        // cout << Output::movelist(list, *this);
        n = list.length();

        for (i = 0; i < n; i++)
        {
            move = list[i];
            do_move(move);
            value = minimax(depth - 1, true);
            undo_move(move);
            searched_moves_++;
            cout << Output::move(move, *this) << " (" << dec << value << "), ";
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