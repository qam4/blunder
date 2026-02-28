/*
 * File:   MoveList.cpp
 */

#include "MoveList.h"

bool MoveList::contains(Move move)
{
    for (int i = 0; i < size_; i++)
    {
        if (data_[i].move == move)
        {
            return true;
        }
    }
    return false;
}

bool MoveList::contains_duplicates()
{
    for (int i = 0; i < size_; i++)
    {
        for (int j = i + 1; j < size_; j++)
        {
            if (data_[i].move == data_[j].move)
            {
                return true;
            }
        }
    }
    return false;
}

bool MoveList::contains_valid_moves(const class Board& board, bool check_legal)
{
    for (int i = 0; i < size_; i++)
    {
        if (!is_valid_move(data_[i].move, board, check_legal))
        {
            return false;
        }
    }
    return true;
}

void MoveList::sort_moves(int current_index)
{
    // Selection sort by score
    for (int i = current_index; i < size_; i++)
    {
        if (data_[i].score > data_[current_index].score)
        {
            ScoredMove tmp = data_[current_index];
            data_[current_index] = data_[i];
            data_[i] = tmp;
        }
    }
}
