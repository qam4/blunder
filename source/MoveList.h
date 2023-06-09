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
  int size;
  Move_t data[MAX_MOVELIST_LENGTH];

public:
  MoveList() { size = 0; }
  void inline push(Move_t move)
  {
#ifndef NDEBUG
    assert(size < MAX_MOVELIST_LENGTH);
#endif
    // cout << "move=" << hex << move << dec << endl;
    data[size++] = move;
  }
  Move_t inline pop()
  {
#ifndef NDEBUG
    assert(size > 0);
#endif
    return data[--size];
  }
  void set_move(const int idx, Move_t move)
  {
#ifndef NDEBUG
    assert(size > 0);
#endif
    data[idx] = move;
  }
  void inline reset() { size = 0; };
  Move_t inline operator[](const int idx) const { return data[idx]; };
  int inline length() const { return size; };
  bool contains(Move_t move);
  bool contains_duplicates();
  bool contains_valid_moves(const class Board &board, bool check_legal = false);
  void sort_moves(int current_index);
};

#endif /* MOVELIST_H */
