/*
 * File:   Zobrist.h
 *
 */

#ifndef ZOBRIST_H
#define ZOBRIST_H

#include "Types.h"
#include "Constants.h"
#include <random>

class Board;

// https://www.chessprogramming.org/Zobrist_Hashing
class Zobrist
{
public:
    Zobrist();
    U64 get_zobrist_key(const Board &board) const;

private:
    U64 rand64();

    U64 pieces[NUM_PIECES][NUM_SQUARES];  // [piece type][square]
    U64 castling_rights[FULL_CASTLING_RIGHTS + 1];
    U64 ep_square[NUM_SQUARES];
    U64 side;                             // side to move

    std::mt19937_64 gen;                  // Random generator
};

#endif /* ZOBRIST_H */
