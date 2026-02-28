/*
 * File:   PrincipalVariation.cpp
 *
 */

#include "PrincipalVariation.h"

#include "Board.h"
#include "MoveList.h"
#include "Output.h"

using std::cout;
using std::endl;

PrincipalVariation::PrincipalVariation()
{
    reset();
}

void PrincipalVariation::reset()
{
    for (int i = 0; i < MAX_SEARCH_PLY * MAX_SEARCH_PLY; i++)
    {
        pv_table_[i] = 0;
    }
    for (int i = 0; i < MAX_SEARCH_PLY; i++)
    {
        pv_length_[i] = 0;
    }
}

void PrincipalVariation::print(Board& board)
{
    int adjust = 0;
    if ((board.side_to_move() == BLACK) && ((board.get_game_ply() & 1) == 0))
    {
        // adjust game_ply if BLACK to play and game ply is even
        adjust = 1;
    }
    for (int i = 0; i < pv_length_[0]; i++)
    {
        int game_ply = board.get_game_ply() + adjust;
        if ((i == 0) || (game_ply & 1) == 0)
        {
            cout << (game_ply + 2) / 2 << ". ";
        }

        if ((i == 0) && ((game_ply & 1) == 1))
        {
            cout << "... ";
        }

        Move_t move = pv_table_[i];
        cout << Output::move_san(move, board) << " ";
        board.do_move(move);
    }
    for (int i = pv_length_[0] - 1; i >= 0; i--)
    {
        Move_t move = pv_table_[i];
        board.undo_move(move);
    }
}

// store PV move
void PrincipalVariation::store_move(int search_ply, Move_t move)
{
    pv_table_[search_ply * MAX_SEARCH_PLY + search_ply] = move;
    for (int next_ply = search_ply + 1; next_ply < pv_length_[search_ply + 1]; next_ply++)
    {
        pv_table_[search_ply * MAX_SEARCH_PLY + next_ply] =
            pv_table_[(search_ply + 1) * MAX_SEARCH_PLY + next_ply];
    }
    pv_length_[search_ply] = pv_length_[search_ply + 1];
}

// score PV and best move
void PrincipalVariation::score_move(MoveList& list,
                                    int search_ply,
                                    Move_t best_move,
                                    int& follow_pv)
{
    // score hash table move
    for (int i = 0; i < list.length(); i++)
    {
        Move move = list[i];
        if (move == best_move)
        {
            assert(move != 0U);
            list.set_score(i, 255U);
            return;
        }
    }

    // score PV move
    if (search_ply && follow_pv)
    {
        follow_pv = 0;
        for (int i = 0; i < list.length(); i++)
        {
            Move move = list[i];
            if (move == pv_table_[search_ply])
            {
                follow_pv = 1;
                list.set_score(i, 128U);
            }
        }
    }
}

Move_t PrincipalVariation::get_best_move() const
{
    return pv_table_[0];
}

Move_t PrincipalVariation::get_move(int index) const
{
    assert(index >= 0 && index < pv_length_[0]);
    return pv_table_[index];
}

int PrincipalVariation::length() const
{
    return pv_length_[0];
}

void PrincipalVariation::set_length(int ply, int len)
{
    pv_length_[ply] = len;
}
