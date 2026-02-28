/*
 * File:   MoveList.h
 */

#ifndef MOVELIST_H
#define MOVELIST_H

#include "Common.h"
#include "Board.h"
#include "ValidateMove.h"

const int MAX_MOVELIST_LENGTH = 256;

class MoveList
{
  private:
    int size_;
    ScoredMove data_[MAX_MOVELIST_LENGTH];

  public:
    MoveList()
        : size_(0)
    {
    }
    void inline push(Move move)
    {
        assert(size_ < MAX_MOVELIST_LENGTH);
        data_[size_++] = ScoredMove(move);
    }
    Move inline pop()
    {
        assert(size_ > 0);
        return data_[--size_].move;
    }
    void set_move(int idx, Move move) { data_[idx].move = move; }
    void set_score(int idx, U16 score) { data_[idx].score = score; }
    U16 get_score(int idx) const { return data_[idx].score; }
    Move inline operator[](int idx) const { return data_[idx].move; }
    void inline reset() { size_ = 0; }
    int inline length() const { return size_; }
    bool contains(Move move);
    bool contains_duplicates();
    bool contains_valid_moves(const class Board& board, bool check_legal = false);
    void sort_moves(int current_index);
};

#endif /* MOVELIST_H */
