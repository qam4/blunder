/*
 * File:   See.cpp
 *
 */

#include "MoveGenerator.h"

U64 MoveGenerator::get_least_valuable_piece(const class Board& board,
                                            U64 attadef,
                                            const U8 side,
                                            U8& piece)
{
    //  return
    //  -  least valuable piece in a set
    //  - the piece by reference
    for (piece = WHITE_PAWN + side; piece <= WHITE_KING + side; piece += 2)
    {
        U64 subset = attadef & board.bitboards_[piece];
        if (subset)
        {
            return subset & -subset;  // the lowest set bit in subset
        }
    }
    return 0;  // empty set
}

U64 MoveGenerator::attacks_to(const class Board& board, U64 occupied, U8 to)
{
    const int diffs[2][2] = { { 7, 64 - 9 }, { 9, 64 - 7 } };
    const U64 pawn_file_mask[2] = { ~FILE_H, ~FILE_A };

    U64 to_bb = 1LL < to;
    U64 attackers = BB_EMPTY;

    // Knights
    U64 knights = board.bitboards_[KNIGHT];
    attackers |= KNIGHT_LOOKUP_TABLE[to] & knights;

    // Pawns
    U64 pawns = board.bitboards_[PAWN];
    for (int dir = 0; dir < 2; dir++)
    {
        for (int side = 0; side < 2; side++)
        {
            int diff = diffs[dir][side];
            U64 targets = circular_left_shift(to_bb, diff) & pawn_file_mask[dir];
            attackers |= targets & pawns;
        }
    }

    // Sliders
    U64 queens = board.bitboards_[QUEEN];
    U64 rooks = board.bitboards_[ROOK];
    U64 bishops = board.bitboards_[BISHOP];

    U64 diag_attackers = queens | bishops;
    U64 non_diag_attackers = queens | rooks;

    attackers |= rook_attacks(occupied, to) & non_diag_attackers;
    attackers |= bishop_attacks(occupied, to) & diag_attackers;

    return attackers;
}

U64 MoveGenerator::consider_xrays(U64 occupied, U8 from, U8 to)
{
    // Find bit set of hidden attackers
    U64 ray_mask = lines_along(from, to);
    U64 bishop_targets = bishop_attacks(occupied, from) & ray_mask;
    U64 rook_targets = rook_attacks(occupied, from) & ray_mask;
    return bishop_targets | rook_targets;
}

int MoveGenerator::see(const class Board& board, U8 from, U8 to, U8 side)
{
    int gain[32], d = 0;

    U64 may_xray = board.bitboards_[PAWN] | board.bitboards_[BISHOP] | board.bitboards_[ROOK]
        | board.bitboards_[QUEEN];
    U64 from_bb = 1ULL << from;
    U64 to_bb = 1ULL << to;
    U8 piece = board[from];
    U8 capture = board[to];

    U64 occupied = (board.bitboards_[WHITE] | board.bitboards_[BLACK]) ^ from_bb ^ to_bb;
    U8 attacker_side = !side;
    U64 attadef = attacks_to(board, occupied, to);
    gain[d] = piece_value[capture >> 1];
    do
    {
        d++;                                              // next depth and side
        gain[d] = piece_value[piece >> 1] - gain[d - 1];  // speculative store, if defended
        if (max(-gain[d - 1], gain[d]) < 0)
        {
            break;  // pruning does not influence the result
        }
        attadef ^= from_bb;   // reset bit in set to traverse
        occupied ^= from_bb;  // reset bit in temporary occupancy (for x-Rays)
        if (from_bb & may_xray)
        {
            attadef |= consider_xrays(occupied, from, to);
        }
        from_bb = get_least_valuable_piece(board, attadef, attacker_side, piece);
        from = bit_scan_forward(from_bb);
        attacker_side ^= 1;
    } while (from_bb);
    while (--d)
    {
        gain[d - 1] = -max(-gain[d - 1], gain[d]);
    }
    return gain[0];
}
