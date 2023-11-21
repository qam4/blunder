/*
 * File:   See.cpp
 *
 */

#include "MoveGenerator.h"

const int MAX_GAINS_LENGTH = 32;

U64 MoveGenerator::get_least_valuable_piece(const class Board& board,
                                            U64 attadef,
                                            const U8 side,
                                            U8& piece)
{
    //  return
    //  - least valuable piece in a set
    //  - the piece by reference
    for (piece = (WHITE_PAWN | side); piece <= (WHITE_KING | side);
         piece = static_cast<U8>(piece + 2))
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

    U64 to_bb = 1ULL << to;
    U64 attackers = BB_EMPTY;

    // Kings
    U64 kings = board.bitboards_[WHITE_KING] | board.bitboards_[BLACK_KING];
    attackers |= KING_LOOKUP_TABLE[to] & kings;

    // Knights
    U64 knights = board.bitboards_[WHITE_KNIGHT] | board.bitboards_[BLACK_KNIGHT];
    attackers |= KNIGHT_LOOKUP_TABLE[to] & knights;

    // Pawns
    U64 pawns = board.bitboards_[WHITE_PAWN] | board.bitboards_[BLACK_PAWN];
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
    U64 queens = board.bitboards_[WHITE_QUEEN] | board.bitboards_[BLACK_QUEEN];
    U64 rooks = board.bitboards_[WHITE_ROOK] | board.bitboards_[BLACK_ROOK];
    U64 bishops = board.bitboards_[WHITE_BISHOP] | board.bitboards_[BLACK_BISHOP];

    U64 diag_attackers = queens | bishops;
    U64 non_diag_attackers = queens | rooks;

    attackers |= rook_attacks(occupied, to) & non_diag_attackers;
    attackers |= bishop_attacks(occupied, to) & diag_attackers;

    return attackers;
}

U64 MoveGenerator::consider_xrays(const class Board& board, U64 occupied, U8 to)
{
    // Find bit set of hidden attackers (only consider sliders)
    U64 attackers = BB_EMPTY;
    U64 queens = board.bitboards_[WHITE_QUEEN] | board.bitboards_[BLACK_QUEEN];
    U64 rooks = board.bitboards_[WHITE_ROOK] | board.bitboards_[BLACK_ROOK];
    U64 bishops = board.bitboards_[WHITE_BISHOP] | board.bitboards_[BLACK_BISHOP];

    U64 diag_attackers = queens | bishops;
    U64 non_diag_attackers = queens | rooks;

    attackers |= rook_attacks(occupied, to) & non_diag_attackers;
    attackers |= bishop_attacks(occupied, to) & diag_attackers;
    return attackers & occupied;
}

// TODO: handle en-assant captures
// TODO: handle promotions
int MoveGenerator::see(const class Board& board, Move_t move)
{
    int gain[MAX_GAINS_LENGTH], d = 0;
    cout << Output::board(board);

    U64 pawns = board.bitboards_[WHITE_PAWN] | board.bitboards_[BLACK_PAWN];
    U64 queens = board.bitboards_[WHITE_QUEEN] | board.bitboards_[BLACK_QUEEN];
    U64 rooks = board.bitboards_[WHITE_ROOK] | board.bitboards_[BLACK_ROOK];
    U64 bishops = board.bitboards_[WHITE_BISHOP] | board.bitboards_[BLACK_BISHOP];

    U64 may_xray = pawns | queens | rooks | bishops;
    U8 to = move_to(move);
    U8 from = move_from(move);
    U64 from_bb = 1ULL << from;
    U8 piece = board[from];
    U8 capture = board[to];
    cout << Output::piece(piece) << "x" << Output::piece(capture) << endl;

    U64 occupied = (board.bitboards_[WHITE] | board.bitboards_[BLACK]);
    U8 attacker_side = board.side_to_move();
    U64 attadef = attacks_to(board, occupied, to);  // attackers and defenders
    cout << "attadef:\n" << Output::bitboard(attadef) << endl;
    gain[d] = SEE_PIECE_VALUE[capture >> 1];
    cout << "gain[" << d << "]=" << gain[d] << endl;
    do
    {
        d++;  // next depth and side
#ifndef NDEBUG
        assert(d < MAX_GAINS_LENGTH);
#endif
        gain[d] = SEE_PIECE_VALUE[piece >> 1] - gain[d - 1];  // speculative store, if defended
        cout << Output::piece(piece) << "x" << endl;
        cout << "gain[" << d << "]=" << gain[d] << endl;
        attacker_side ^= 1;
        if (max(-gain[d - 1], gain[d]) < 0)
        {
            break;  // pruning does not influence the result
        }
        attadef ^= from_bb;   // reset bit in set to traverse
        occupied ^= from_bb;  // reset bit in temporary occupancy (for x-Rays)
        if (from_bb & may_xray)
        {
            attadef |= consider_xrays(board, occupied, to);
        }
        cout << "attadef:\n" << Output::bitboard(attadef) << endl;
        from_bb = get_least_valuable_piece(board, attadef, attacker_side, piece);
    } while (from_bb);
    while (--d)
    {
        gain[d - 1] = -max(-gain[d - 1], gain[d]);
        cout << "gain_[" << d << "]=" << gain[d] << endl;
    }
    cout << "gain_[" << d << "]=" << gain[d] << endl;
    return gain[0];
}
