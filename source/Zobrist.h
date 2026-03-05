/*
 * File:   Zobrist.h
 *
 * Zobrist hashing with static key tables shared across all Board instances.
 */

#ifndef ZOBRIST_H
#define ZOBRIST_H

#include "Types.h"
#include "Constants.h"

class Board;

// https://www.chessprogramming.org/Zobrist_Hashing
class Zobrist
{
public:
    /// Initialize static key tables. Must be called once before any Board use.
    static void init();

    static U64 get_zobrist_key(const Board& board);
    static U64 get_pieces(U8 piece, int square) { return pieces_[piece][square]; }
    static U64 get_castling_rights(U8 rights) { return castling_rights_[rights]; }
    static U64 get_ep_square(U8 square) { return ep_square_[square]; }
    static U64 get_side() { return side_; }

private:
    static U64 pieces_[NUM_PIECES][NUM_SQUARES];
    static U64 castling_rights_[FULL_CASTLING_RIGHTS + 1];
    static U64 ep_square_[NUM_SQUARES + 1];  // +1 so NULL_SQUARE (64) index is safe
    static U64 side_;
    static bool initialized_;
};

#endif /* ZOBRIST_H */
