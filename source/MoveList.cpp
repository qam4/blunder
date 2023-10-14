/*
 * File:   MoveList.cpp
 *
 */

#include "MoveList.h"

bool MoveList::contains(Move_t move)
{
    for (int i = 0; i < size_; i++)
    {
        if (data_[i] == move)
            return true;
    }
    return false;
}

bool MoveList::contains_duplicates()
{
    for (int i = 0; i < size_; i++)
    {
        for (int j = i + 1; j < size_; j++)
        {
            if (data_[i] == data_[j])
                return true;
        }
    }
    return false;
}

bool MoveList::contains_valid_moves(const class Board& board, bool check_legal)
{
    for (int i = 0; i < size_; i++)
    {
        if (!is_valid_move(data_[i], board, check_legal))
            return false;
    }
    return true;
}

void MoveList::sort_moves(int current_index)
{
    // Selection sort
    for (int i = current_index; i < size_; i++)
    {
        U16 current_score = move_score(data_[current_index]);
        U16 new_score = move_score(data_[i]);
        if (new_score > current_score)
        {
            Move_t tmp = data_[current_index];
            data_[current_index] = data_[i];
            data_[i] = tmp;
        }
    }
}
