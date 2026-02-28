/*
 * File:   PrincipalVariation.h
 *
 */

#ifndef PRINCIPAL_VARIATION_H
#define PRINCIPAL_VARIATION_H

#include "Types.h"
#include "Constants.h"
#include "Move.h"

class Board;
class MoveList;

class PrincipalVariation
{
public:
    PrincipalVariation();

    void reset();
    void store_move(int search_ply, Move_t move);
    void score_move(MoveList& list, int search_ply, Move_t best_move, int& follow_pv);
    void print(Board& board);
    Move_t get_best_move() const;
    int length() const;
    void set_length(int ply, int len);

private:
    Move_t pv_table_[MAX_SEARCH_PLY * MAX_SEARCH_PLY];
    int pv_length_[MAX_SEARCH_PLY];
};

#endif /* PRINCIPAL_VARIATION_H */
