/*
 * File:   PrincipalVariation.cpp
 *
 */

#include "PrincipalVariation.h"

#include "Board.h"
#include "MoveList.h"
#include "Output.h"

// Global variables
Move_t pv_table[MAX_SEARCH_PLY * MAX_SEARCH_PLY];
int pv_length[MAX_SEARCH_PLY];

void reset_pv_table()
{
    for (int i = 0; i < MAX_SEARCH_PLY * MAX_SEARCH_PLY; i++)
    {
        pv_table[i] = 0;
    }
    for (int i = 0; i < MAX_SEARCH_PLY; i++)
    {
        pv_length[i] = 0;
    }
}

void Board::print_pv()
{
    int adjust = 0;
    if ((side_to_move() == BLACK) && ((game_ply_ & 1) == 0))
    {
        // adjust game_ply if BLACK to play and game ply is even
        adjust = 1;
    }
    for (int i = 0; i < pv_length[0]; i++)
    {
        int game_ply = game_ply_ + adjust;
        if ((i == 0) || (game_ply & 1) == 0)
        {
            cout << (game_ply + 2) / 2 << ". ";
        }

        if ((i == 0) && ((game_ply & 1) == 1))
        {
            cout << "... ";
        }

        Move_t move = pv_table[i];
        cout << Output::move_san(move, *this) << " ";
        do_move(move);
    }
    for (int i = pv_length[0] - 1; i >= 0; i--)
    {
        Move_t move = pv_table[i];
        undo_move(move);
    }
}

// store PV move
void Board::store_pv_move(Move_t move)
{
    pv_table[search_ply_ * MAX_SEARCH_PLY + search_ply_] = move;
    for (int next_ply = search_ply_ + 1; next_ply < pv_length[search_ply_ + 1]; next_ply++)
    {
        pv_table[search_ply_ * MAX_SEARCH_PLY + next_ply] =
            pv_table[(search_ply_ + 1) * MAX_SEARCH_PLY + next_ply];
    }
    pv_length[search_ply_] = pv_length[search_ply_ + 1];
}

// score PV and best move
void Board::score_pv_move(class MoveList& list, Move_t best_move)
{
    // score hash table move
    for (int i = 0; i < list.length(); i++)
    {
        Move_t move = list[i];
        if (move == best_move)
        {
            assert(move != 0U);
            move_set_score(&move, 255U);
            list.set_move(i, move);
            return;
        }
    }

    // score PV move
    if (search_ply_ && follow_pv_)
    {
        follow_pv_ = 0;
        for (int i = 0; i < list.length(); i++)
        {
            Move_t move = list[i];
            if (move == pv_table[search_ply_])
            {
                follow_pv_ = 1;
                move_set_score(&move, 128);
                list.set_move(i, move);
            }
        }
    }
}
