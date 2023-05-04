/*
 * File:   MoveGenerator.h
 *
 */

#ifndef MOVEGENERATOR_H
#define MOVEGENERATOR_H

#include "Common.h"
#include "MoveList.h"
#include "Move.h"
#include "Board.h"
#include "Output.h"

// Move ordering table
// Move valuable victim, least valuable attacker
const U8 MVVLVA[NUM_PIECES / 2][NUM_PIECES / 2] =
{
    {0, 0, 0, 0, 0, 0, 0},        // victim = None, attacker None, P, K, B, R, Q, K
    {0, 15, 14, 13, 12, 11, 10},  // victim = pawn, attacker None, P, K, B, R, Q, K
    {0, 25, 24, 23, 22, 21, 20},  // victim = knight, attacker None, P, K, B, R, Q, K
    {0, 35, 34, 33, 32, 31, 30},  // victim = bishop, attacker None, P, K, B, R, Q, K
    {0, 45, 44, 43, 42, 41, 40},  // victim = rook, attacker None, P, K, B, R, Q, K
    {0, 55, 54, 53, 52, 51, 50},  // victim = queen, attacker None, P, K, B, R, Q, K
    {0, 65, 64, 63, 62, 61, 60},  // victim = king, attacker None, P, K, B, R, Q, K
};

// https://www.chessprogramming.org/BitScan
const U8 index64[64] = {
   63,  0, 58,  1, 59, 47, 53,  2,
   60, 39, 48, 27, 54, 33, 42,  3,
   61, 51, 37, 40, 49, 18, 28, 20,
   55, 30, 34, 11, 43, 14, 22,  4,
   62, 57, 46, 52, 38, 26, 32, 41,
   50, 36, 17, 19, 29, 10, 13, 21,
   56, 45, 25, 31, 35, 16,  9, 12,
   44, 24, 15,  8, 23,  7,  6,  5
};

/**
* bitScanForward
* @author Charles E. Leiserson
*         Harald Prokop
*         Keith H. Randall
* "Using de Bruijn Sequences to Index a 1 in a Computer Word"
* @param bb bitboard to scan
* @precondition bb != 0
* @return index (0..63) of least significant one bit
*/
U8 inline bit_scan_forward(U64 bb)
{
#ifndef NDEBUG
    assert(bb > 0);
#endif
   const U64 debruijn64 = C64(0x07EDD5E59A4E28C2);
   const U64 neg_bb = (0 - bb);
   return index64[((bb & neg_bb) * debruijn64) >> 58];
}


U64 inline circular_left_shift(U64 target, int shift)
{
#ifndef NDEBUG
    assert(shift >= 0);
    assert(shift <= 64);
#endif
    return (target << shift) | (target >> (64 - shift));
}

class MoveGenerator
{

public:
    static void add_rook_moves(class MoveList &list, const class Board &board, const U8 side);
    static void add_bishop_moves(class MoveList &list, const class Board &board, const U8 side);
    static void add_pawn_pushes(class MoveList &list, const class Board &board, const U8 side);
    static void add_pawn_attacks(class MoveList &list, const class Board &board, const U8 side);
    static void add_knight_moves(class MoveList &list, const class Board &board, const U8 side);
    static void add_queen_moves(class MoveList &list, const class Board &board, const U8 side);
    static void add_king_moves(class MoveList &list, const class Board &board, const U8 side);
    static void add_all_moves(class MoveList &list, const class Board &board, const U8 side);
    static void score_moves(class MoveList &list, const class Board &board);

    static void generate_move_lookup_tables();

private:
    // diff is positive number denoting fixed distance between from and to squares such that:
    // from = (to - diff) % 64
    // ie white pawn push diff = 56, black pawn push diff = 8
    // add_moves assumes moving to blank square
    static void add_moves(U8 from, U64 targets, class MoveList &list, const class Board &board, const U8 flags);
    static void add_moves_with_diff(int diff, U64 targets, class MoveList &list, const class Board &board, const U8 flags, const U8 extra_capture);
    static void add_promotions_with_diff(int diff, U64 targets, class MoveList &list, const class Board &board, const U8 flags);
    static U64 byteswap(U64 x);
    static U64 flipVertical(U64 x);
    static U64 mirrorHorizontal(U64 x);
    static U64 rankMask(int sq);
    static U64 fileMask(int sq);
    static U64 diagMask(int sq);
    static U64 antiDiagMask(int sq);
    static U64 rankMaskEx(int sq);
    static U64 fileMaskEx(int sq);
    static U64 diagMaskEx(int sq);
    static U64 antiDiagMaskEx(int sq);
    static U64 diagAttacks(U64 occ, int sq);
    static U64 antiDiagAttacks(U64 occ, int sq);
    static U64 fileAttacks(U64 occ, int sq);
    static U64 rankAttacks(U64 occ, int sq);
};

#endif /* MOVEGENERATOR_H */
