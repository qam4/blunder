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
#include "LookupTables.h"

// Move ordering table
// Move valuable victim, least valuable attacker
const U8 MVVLVA[NUM_PIECES / 2][NUM_PIECES / 2] =
{
    {0, 0, 0, 0, 0, 0, 0},        // victim = None, attacker None, P, N, B, R, Q, K
    {0, 15, 14, 13, 12, 11, 10},  // victim = pawn, attacker None, P, N, B, R, Q, K
    {0, 25, 24, 23, 22, 21, 20},  // victim = knight, attacker None, P, N, B, R, Q, K
    {0, 35, 34, 33, 32, 31, 30},  // victim = bishop, attacker None, P, N, B, R, Q, K
    {0, 45, 44, 43, 42, 41, 40},  // victim = rook, attacker None, P, N, B, R, Q, K
    {0, 55, 54, 53, 52, 51, 50},  // victim = queen, attacker None, P, N, B, R, Q, K
    {0, 65, 64, 63, 62, 61, 60},  // victim = king, attacker None, P, N, B, R, Q, K
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

const int SEE_PIECE_VALUE[NUM_PIECES / 2] = {
    0,
    100, // PAWN
    320, // KNIGHT
    330, // BISHOP
    500, // ROOK
    900, // QUEEN
    20000, // KING
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
    assert(bb > 0);
#if defined(__GNUC__)
    return static_cast<U8>(__builtin_ffsll(static_cast<long long>(bb)) - 1);
#else
   const U64 debruijn64 = C64(0x07EDD5E59A4E28C2);
   const U64 neg_bb = (0 - bb);
   return index64[((bb & neg_bb) * debruijn64) >> 58];
#endif
}

U64 inline circular_left_shift(U64 target, int shift)
{
    assert(shift >= 0);
    assert(shift <= 64);
    return (target << shift) | (target >> (64 - shift));
}

/**
 * Flip a bitboard vertically about the center ranks.
 * Rank 1 is mapped to rank 8 and vice versa.
 * @param x any bitboard
 * @return bitboard x flipped vertically
 */
U64 inline flip_vertical(U64 x)
{
    const U64 k1 = C64(0x00FF00FF00FF00FF);
    const U64 k2 = C64(0x0000FFFF0000FFFF);
    x = ((x >> 8) & k1) | ((x & k1) << 8);
    x = ((x >> 16) & k2) | ((x & k2) << 16);
    x = (x >> 32) | (x << 32);
    return x;
}

/**
 * Byte swap === flip vertical
 */
U64 inline byteswap(U64 x)
{
#if defined(__GNUC__)
    return (__builtin_bswap64(x));
#else
    return flip_vertical(x);
#endif
}

/**
 * Mirror a bitboard horizontally about the center files.
 * File a is mapped to file h and vice versa.
 * @param x any bitboard
 * @return bitboard x mirrored horizontally
 *
 * https://www.chessprogramming.org/Flipping_Mirroring_and_Rotating#MirrorHorizontally
 */
U64 inline mirror_horizontal(U64 x)
{
    const U64 k1 = C64(0x5555555555555555);
    const U64 k2 = C64(0x3333333333333333);
    const U64 k4 = C64(0x0f0f0f0f0f0f0f0f);
    x = ((x >> 1) & k1) +  2*(x & k1);
    x = ((x >> 2) & k2) +  4*(x & k2);
    x = ((x >> 4) & k4) + 16*(x & k4);
    return x;
}

// Using lookup table
U64 inline squares_between(U8 sq1, U8 sq2)
{
    return SQUARES_BETWEEN[sq1][sq2];
}

U64 inline lines_along(U8 sq1, U8 sq2)
{
    return LINES_ALONG[sq1][sq2];
}

U64 inline rank_mask(int sq)
{
    return RANK_MASK[sq];
}

U64 inline file_mask(int sq)
{
    return FILE_MASK[sq];
}

U64 inline diag_mask(int sq)
{
    return DIAG_MASK[sq];
}

U64 inline anti_diag_mask(int sq)
{
    return ANTI_DIAG_MASK[sq];
}

U64 inline rank_mask_ex(int sq)
{
    return RANK_MASK_EX[sq];
}

U64 inline file_mask_ex(int sq)
{
    return FILE_MASK_EX[sq];
}

U64 inline diag_mask_ex(int sq)
{
    return DIAG_MASK_EX[sq];
}

U64 inline anti_diag_mask_ex(int sq)
{
    return ANTI_DIAG_MASK_EX[sq];
}

U64 inline rook_mask(int sq)
{
    return ROOK_MASK[sq];
}

U64 inline bishop_mask(int sq)
{
    return BISHOP_MASK[sq];
}

U64 inline rook_mask_ex(int sq)
{
    return ROOK_MASK_EX[sq];
}

U64 inline bishop_mask_ex(int sq)
{
    return BISHOP_MASK_EX[sq];
}

struct MoveGenPreprocessing
{
    U64 checkers; // Opponent pieces giving check
    U64 pinned;   // Friendly pieces that are pinned
    U64 pinners;  // Opponent pieces pinning friendly pieces
};

namespace MoveGenerator
{
    // Public interface
    U64 get_checkers(const Board &board, U8 side);
    MoveGenPreprocessing get_checkers_and_pinned(const Board &board, U8 side);
    U64 get_king_danger_squares(const Board& board, U8 side, U64 king);
    U64 get_least_valuable_piece(const Board& board, U64 attadef, U8 side, U8 &piece);
    int see(const Board& board, Move_t move);
    void add_rook_moves(MoveList &list, const Board &board, U8 side);
    void add_bishop_moves(MoveList &list, const Board &board, U8 side);
    void add_pawn_pushes(MoveList &list, const Board &board, U8 side);
    void add_pawn_attacks(MoveList &list, const Board &board, U8 side);
    void add_knight_moves(MoveList &list, const Board &board, U8 side);
    void add_queen_moves(MoveList &list, const Board &board, U8 side);
    void add_king_moves(MoveList &list, const Board &board, U8 side);
    void add_pawn_legal_pushes(MoveList& list, const Board& board, U64 to_mask, U64 from_mask, U8 side);
    void add_pawn_legal_attacks(MoveList& list, const Board& board, U64 capture_mask, U64 push_mask, U64 from_mask, U8 side);
    void add_pawn_legal_moves(MoveList& list, const Board& board, U64 capture_mask, U64 push_mask, U64 from_mask, U8 side);
    void add_pawn_pin_ray_moves(MoveList& list, const Board& board, U64 capture_mask, U64 push_mask, U64 pinned_mask, U8 king_sq, U8 side);
    void add_pawn_pin_ray_attacks(MoveList& list, const Board& board, U64 capture_mask, U64 pinned_mask, U8 king_sq, U8 side);
    void add_slider_legal_moves(MoveList& list, const Board& board, U64 capture_mask, U64 push_mask, U64 pinned_mask, U8 king_sq, U8 side);
    void add_slider_legal_attacks(MoveList& list, const Board& board, U64 capture_mask, U64 pinned_mask, U8 king_sq, U8 side);
    void add_knight_legal_moves(MoveList& list, const Board& board, U64 capture_mask, U64 push_mask, U64 from_mask, U8 side);
    void add_knight_legal_attacks(MoveList& list, const Board& board, U64 capture_mask, U64 from_mask, U8 side);
    void add_king_legal_moves(MoveList& list, const Board& board, U64 capture_mask, U64 push_mask, U8 side);
    void add_king_legal_attacks(MoveList& list, const Board& board, U64 capture_mask, U8 side);
    void add_castles(MoveList& list, const Board& board, U64 attacks, U8 side);
    void add_all_moves(MoveList &list, const Board &board, U8 side);
    void add_loud_moves(MoveList &list, const Board &board, U8 side);
    bool in_check(const Board &board, U8 side);
    void score_moves(MoveList &list, const Board &board);

    void generate_move_lookup_tables();
    void init_magic_tables();

    // Internal helpers (used across translation units)
    void add_moves(U8 from, U64 targets, MoveList &list, const Board &board, U32 flags);
    void add_moves_with_diff(int diff, U64 targets, MoveList &list, const Board &board, U32 flags, U8 extra_capture);
    void add_promotions_with_diff(int diff, U64 targets, MoveList &list, const Board &board, U8 side);
    bool ep_move_discovers_check(const Board &board, U64 from_bb, U64 to_bb, U8 side);
    U64 rook_targets(U64 from, U64 occupied);
    U64 bishop_targets(U64 from, U64 occupied);
    U64 knight_targets(U64 from);
    U64 king_targets(U64 from);
    U64 pawn_targets(U64 from, U8 side);

    U64 squares_between_calc(U8 sq1, U8 sq2);
    U64 lines_along_calc(U8 sq1, U8 sq2);
    U64 rank_mask_calc(int sq);
    U64 file_mask_calc(int sq);
    U64 diag_mask_calc(int sq);
    U64 anti_diag_mask_calc(int sq);
    U64 rank_mask_ex_calc(int sq);
    U64 file_mask_ex_calc(int sq);
    U64 diag_mask_ex_calc(int sq);
    U64 anti_diag_mask_ex_calc(int sq);
    U64 rook_mask_calc(int sq);
    U64 rook_mask_ex_calc(int sq);
    U64 bishop_mask_calc(int sq);
    U64 bishop_mask_ex_calc(int sq);

    U64 diag_attacks(U64 occ, int sq);
    U64 anti_diag_attacks(U64 occ, int sq);
    U64 file_attacks(U64 occ, int sq);
    U64 rank_attacks(U64 occ, int sq);
    U64 rook_attacks(U64 occ, int sq);
    U64 bishop_attacks(U64 occ, int sq);

    U64 rook_attacks_slow(U64 occ, int sq);
    U64 bishop_attacks_slow(U64 occ, int sq);

    U64 diag_attacks_hyperbola(U64 occ, int sq);
    U64 anti_diag_attacks_hyperbola(U64 occ, int sq);
    U64 file_attacks_hyperbola(U64 occ, int sq);
    U64 rank_attacks_hyperbola(U64 occ, int sq);

    void init_rook_magic_table();
    void init_bishop_magic_table();
    U64 rook_attacks_magic(U64 occ, int sq);
    U64 bishop_attacks_magic(U64 occ, int sq);

    U64 attacks_to(const Board& board, U64 occupied, U8 to);
    U64 consider_xrays(const Board& board, U64 occupied, U8 to);
} // namespace MoveGenerator

#endif /* MOVEGENERATOR_H */
