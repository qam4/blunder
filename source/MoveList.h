/*
 * File:   MoveList.h
 *
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
  Move_t data_[MAX_MOVELIST_LENGTH];

public:
  MoveList() { size_ = 0; }
  void inline push(Move_t move)
  {
    assert(size_ < MAX_MOVELIST_LENGTH);
    // cout << "move=" << hex << move << dec << endl;
    data_[size_++] = move;
  }
  Move_t inline pop()
  {
    assert(size_ > 0);
    return data_[--size_];
  }
  void set_move(const int idx, Move_t move)
  {
    assert(size_ > 0);
    data_[idx] = move;
  }
  void inline reset() { size_ = 0; };
  Move_t inline operator[](const int idx) const { return data_[idx]; };
  int inline length() const { return size_; };
  bool contains(Move_t move);
  bool contains_duplicates();
  bool contains_valid_moves(const class Board &board, bool check_legal = false);
  void sort_moves(int current_index);
};

#endif /* MOVELIST_H */
