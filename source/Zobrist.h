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
    U64 inline get_pieces(U8 piece, int square) const { return pieces_[piece][square]; };
    U64 inline get_castling_rights(U8 rights) const { return castling_rights_[rights]; };
    U64 inline get_ep_square(U8 square) const { return ep_square_[square]; };
    U64 inline get_side() const { return side_; };

private:
    U64 rand64();

    U64 pieces_[NUM_PIECES][NUM_SQUARES];  // [piece type][square]
    U64 castling_rights_[FULL_CASTLING_RIGHTS + 1];
    U64 ep_square_[NUM_SQUARES];
    U64 side_;                             // side to move

    std::mt19937_64 gen_;                  // Random generator
};

#endif /* ZOBRIST_H */
