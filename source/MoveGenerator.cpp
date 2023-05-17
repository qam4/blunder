/*
 * File:   MoveGenerator.cpp
 *
 */

#include "MoveGenerator.h"

#include "LookupTables.h"

void MoveGenerator::add_moves(
    U8 from, U64 targets, class MoveList& list, const class Board& board, const U8 flags)
{
    while (targets)
    {
        U8 to = bit_scan_forward(targets);
        U8 capture = board[to];
        Move_t move = build_move_all(from, to, flags, capture);
        list.push(move);
        targets &= targets - 1;
    }
}

void MoveGenerator::add_moves_with_diff(int diff,
                                        U64 targets,
                                        class MoveList& list,
                                        const class Board& board,
                                        const U8 flags,
                                        const U8 extra_capture)
{
    while (targets)
    {
        U8 to = bit_scan_forward(targets);
        U8 from = static_cast<U8>(to - diff) % 64;
        U8 capture = board[to] | extra_capture;
        Move_t move = build_move_all(from, to, flags, capture);
        list.push(move);
        targets &= targets - 1;
    }
}

void MoveGenerator::add_promotions_with_diff(
    int diff, U64 targets, class MoveList& list, const class Board& board, const U8 side)
{
    U8 flags = side;
    while (targets)
    {
        U8 to = bit_scan_forward(targets);
        U8 from = static_cast<U8>(to - diff) % 64;
        U8 capture = board[to];
        Move_t move = build_move_all(from, to, flags, capture);
        list.push(move | (KNIGHT << 16));
        list.push(move | (BISHOP << 16));
        list.push(move | (ROOK << 16));
        list.push(move | (QUEEN << 16));
        targets &= targets - 1;
    }
}

// https://www.chessprogramming.org/Square_Attacked_By#In_Between
U64 MoveGenerator::in_between(U8 sq1, U8 sq2)
{
    const U64 m1 = C64(-1);
    const U64 a2a7 = C64(0x0001010101010100);
    const U64 b2g7 = C64(0x0040201008040200);
    const U64 h1b7 = C64(0x0002040810204080);
    U64 btwn, line, rank, file;

    btwn = (m1 << sq1) ^ (m1 << sq2);
    file = (sq2 & 7) - (sq1 & 7);
    rank = ((sq2 | 7) - sq1) >> 3;
    line = ((file & 7) - 1) & a2a7;            /* a2a7 if same file */
    line += 2 * (((rank & 7) - 1) >> 58);      /* b1g1 if same rank */
    line += (((rank - file) & 15) - 1) & b2g7; /* b2g7 if same diagonal */
    line += (((rank + file) & 15) - 1) & h1b7; /* h1b7 if same antidiag */
    line *= btwn & -btwn;                      /* mul acts like shift by smaller square */
    return line & btwn;                        /* return the bits on that line in-between */
}

// Using lookup table
U64 MoveGenerator::squares_between(U8 sq1, U8 sq2)
{
    return SQUARES_BETWEEN[sq1][sq2];
}

/**
 * Flip a bitboard vertically about the center ranks.
 * Rank 1 is mapped to rank 8 and vice versa.
 * @param x any bitboard
 * @return bitboard x flipped vertically
 */
U64 MoveGenerator::flip_vertical(U64 x)
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
U64 MoveGenerator::byteswap(U64 x)
{
#if 0
    //  __builtin_bswap64()
    return (__builtin_bswap64(x));
#endif
    return flip_vertical(x);
}

/**
 * Mirror a bitboard horizontally about the center files.
 * File a is mapped to file h and vice versa.
 * @param x any bitboard
 * @return bitboard x mirrored horizontally
 *
 * https://www.chessprogramming.org/Flipping_Mirroring_and_Rotating#MirrorHorizontally
 */
U64 MoveGenerator::mirror_horizontal(U64 x)
{
    const U64 k1 = C64(0x5555555555555555);
    const U64 k2 = C64(0x3333333333333333);
    const U64 k4 = C64(0x0f0f0f0f0f0f0f0f);
    x = ((x >> 1) & k1) | ((x & k1) << 1);
    x = ((x >> 2) & k2) | ((x & k2) << 2);
    x = ((x >> 4) & k4) | ((x & k4) << 4);
    return x;
}

// Sliding piece attacks
// https://www.chessprogramming.org/Sliding_Piece_Attacks

// Ray Masks (should be turned into lookup tables)
U64 MoveGenerator::rank_mask(int sq)
{
    return C64(0xff) << (sq & 56);
}

U64 MoveGenerator::file_mask(int sq)
{
    return C64(0x0101010101010101) << (sq & 7);
}

U64 MoveGenerator::diag_mask(int sq)
{
    const U64 maindia = C64(0x8040201008040201);
    int diag = 8 * (sq & 7) - (sq & 56);
    int nort = -diag & (diag >> 31);
    int sout = diag & (-diag >> 31);
    return (maindia >> sout) << nort;
}

U64 MoveGenerator::anti_diag_mask(int sq)
{
    const U64 maindia = C64(0x0102040810204080);
    int diag = 56 - 8 * (sq & 7) - (sq & 56);
    int nort = -diag & (diag >> 31);
    int sout = diag & (-diag >> 31);
    return (maindia >> sout) << nort;
}
// excluding the square bit:
U64 MoveGenerator::rank_mask_ex(int sq)
{
    return (C64(1) << sq) ^ rank_mask(sq);
}
U64 MoveGenerator::file_mask_ex(int sq)
{
    return (C64(1) << sq) ^ file_mask(sq);
}
U64 MoveGenerator::diag_mask_ex(int sq)
{
    return (C64(1) << sq) ^ diag_mask(sq);
}
U64 MoveGenerator::anti_diag_mask_ex(int sq)
{
    return (C64(1) << sq) ^ anti_diag_mask(sq);
}

#if 0
U64 MoveGenerator::rookMask    (int sq) {return rank_mask(sq)     | file_mask(sq);}
U64 MoveGenerator::bishopMask  (int sq) {return diagonalMask(sq) | anti_diag_mask(sq);}
U64 MoveGenerator::rookMaskEx  (int sq) {return rank_mask(sq)     ^ file_mask(sq);}
U64 MoveGenerator::bishopMaskEx(int sq) {return diagonalMask(sq) ^ anti_diag_mask(sq);}
#endif

// https://www.chessprogramming.org/Efficient_Generation_of_Sliding_Piece_Attacks
U64 MoveGenerator::diag_attacks(U64 occ, int sq)
{
    // lineAttacks = (o-2s) ^ (o'-2s')'
    //     with m=mask
    // lineAttacks=(((o&m)-2s) ^ ((o&m)'-2s')')&m
    U64 forward, reverse, slider, lineMask;

    lineMask = diag_mask_ex(sq);  // excludes square of slider
    slider = C64(1) << sq;        // singleBitboard[sqOfSlider]; // single bit 1 << sq, 2^sq

    forward = occ & lineMask;     // also performs the first subtraction by clearing the s in o
    reverse = byteswap(forward);  // o'-s'
    forward -= (slider);          // o -2s
    reverse -= byteswap(slider);  // o'-2s'
    forward ^= byteswap(reverse);
    forward &= lineMask;  // mask the line again
    return forward;
}

U64 MoveGenerator::anti_diag_attacks(U64 occ, int sq)
{
    // lineAttacks= (o-2s) ^ (o'-2s')'
    //     with m=mask
    // lineAttacks=(((o&m)-2s) ^ ((o&m)'-2s')')&m
    U64 forward, reverse, slider, lineMask;

    lineMask = anti_diag_mask_ex(sq);  // excludes square of slider
    slider = C64(1) << sq;             // singleBitboard[sqOfSlider]; // single bit 1 << sq, 2^sq

    forward = occ & lineMask;     // also performs the first subtraction by clearing the s in o
    reverse = byteswap(forward);  // o'-s'
    forward -= (slider);          // o -2s
    reverse -= byteswap(slider);  // o'-2s'
    forward ^= byteswap(reverse);
    forward &= lineMask;  // mask the line again
    return forward;
}

U64 MoveGenerator::file_attacks(U64 occ, int sq)
{
    // lineAttacks= (o-2s) ^ (o'-2s')'
    //     with m=mask
    // lineAttacks=(((o&m)-2s) ^ ((o&m)'-2s')')&m
    U64 forward, reverse, slider, lineMask;

    lineMask = file_mask_ex(sq);  // excludes square of slider
    slider = C64(1) << sq;        // singleBitboard[sqOfSlider]; // single bit 1 << sq, 2^sq

    forward = occ & lineMask;     // also performs the first subtraction by clearing the s in o
    reverse = byteswap(forward);  // o'-s'
    forward -= (slider);          // o -2s
    reverse -= byteswap(slider);  // o'-2s'
    forward ^= byteswap(reverse);
    forward &= lineMask;  // mask the line again
    return forward;
}

U64 MoveGenerator::rank_attacks(U64 occ, int sq)
{
    // lineAttacks= (o-2s) ^ (o'-2s')'
    //     with m=mask
    // lineAttacks=(((o&m)-2s) ^ ((o&m)'-2s')')&m
    U64 forward, reverse, slider, lineMask;

    lineMask = rank_mask_ex(sq);  // excludes square of slider
    slider = C64(1) << sq;        // singleBitboard[sqOfSlider]; // single bit 1 << sq, 2^sq

    forward = occ & lineMask;  // also performs the first subtraction by clearing the s in o
    reverse = mirror_horizontal(forward);  // o'-s'
    forward -= (slider);                   // o -2s
    reverse -= mirror_horizontal(slider);  // o'-2s'
    forward ^= mirror_horizontal(reverse);
    forward &= lineMask;  // mask the line again
    return forward;
}

void MoveGenerator::add_rook_moves(class MoveList& list, const class Board& board, const U8 side)
{
    U64 rooks = board.bitboards[ROOK | side];
    U64 occupied = board.bitboards[WHITE] | board.bitboards[BLACK];
    U64 friendly = board.bitboards[side];

#if 0
    cout << "rooks=0x" << hex << rooks<< endl;
    cout << Output::bitboard(rooks);
    cout << "occupied=0x" << hex << occupied<< endl;
    cout << Output::bitboard(occupied);
    cout << "friendly=0x" << hex << occupied<< endl;
    cout << Output::bitboard(friendly);
#endif

    while (rooks)
    {
        U8 from = bit_scan_forward(rooks);

        U64 targets;
        // Add file and rank attacks
        targets = file_attacks(occupied, from) + rank_attacks(occupied, from);
        targets &= ~(friendly);
        add_moves(from, targets, list, board, NO_FLAGS);

        rooks &= rooks - 1;
    }
    // cout << "found " << dec <<list.length() << " moves" << endl;
}

void MoveGenerator::add_bishop_moves(class MoveList& list, const class Board& board, const U8 side)
{
    U64 bishops = board.bitboards[BISHOP | side];
    U64 occupied = board.bitboards[WHITE] | board.bitboards[BLACK];
    U64 friendly = board.bitboards[side];  // own pieces

    while (bishops)
    {
        U8 from = bit_scan_forward(bishops);

        U64 targets;
        // Add diagonal and antidiagonal attacks
        targets = diag_attacks(occupied, from) + anti_diag_attacks(occupied, from);
        targets &= ~(friendly);
        add_moves(from, targets, list, board, NO_FLAGS);

        bishops &= bishops - 1;
    }
}

void MoveGenerator::add_queen_moves(class MoveList& list, const class Board& board, const U8 side)
{
    U64 queens = board.bitboards[QUEEN | side];
    U64 occupied = board.bitboards[WHITE] | board.bitboards[BLACK];
    U64 friendly = board.bitboards[side];  // own pieces

    while (queens)
    {
        U8 from = bit_scan_forward(queens);

        U64 targets;
        // Add diagonal, antidiagonal, file and rank attacks
        targets = diag_attacks(occupied, from) + anti_diag_attacks(occupied, from)
            + file_attacks(occupied, from) + rank_attacks(occupied, from);
        targets &= ~(friendly);
        add_moves(from, targets, list, board, NO_FLAGS);

        queens &= queens - 1;
    }
}

void MoveGenerator::add_pawn_pushes(class MoveList& list, const class Board& board, const U8 side)
{
    const int diffs[2] = { 8, 64 - 8 };
    const U64 promotions_mask[2] = { ROW_8, ROW_1 };
    const U64 start_row_plus_one_mask[2] = { ROW_3, ROW_6 };
    U64 pushes, double_pushes, promotions, pawns, free_squares;

    int diff = diffs[side];
    pawns = board.bitboards[PAWN | side];
    free_squares = ~(board.bitboards[WHITE] | board.bitboards[BLACK]);

    // ADD SINGLE PUSHES
    pushes = circular_left_shift(pawns, diff) & free_squares;
    add_moves_with_diff(diff, pushes & (~promotions_mask[side]), list, board, NO_FLAGS, 0);

    // ADD PROMOTIONS
    promotions = pushes & promotions_mask[side];
    add_promotions_with_diff(diff, promotions, list, board, side);

    // ADD DOUBLE PUSHES
    double_pushes =
        circular_left_shift(pushes & start_row_plus_one_mask[side], diff) & free_squares;
    add_moves_with_diff(diff + diff, double_pushes, list, board, PAWN_DOUBLE_PUSH, 0);
}

void MoveGenerator::add_pawn_attacks(class MoveList& list, const class Board& board, const U8 side)
{
    const int diffs[2][2] = { { 7, 64 - 9 }, { 9, 64 - 7 } };
    const U64 promotions_mask[2] = { ROW_8, ROW_1 };
    const U64 file_mask[2] = { ~FILE_H, ~FILE_A };
    U64 attacks, ep_attacks, promotions, targets, pawns, enemy;

    pawns = board.bitboards[PAWN | side];
    enemy = board.bitboards[!side];

    // CALCULATE ATTACKS FOR LEFT, RIGHT
    for (int dir = 0; dir < 2; dir++)
    {
        int diff = diffs[dir][side];
        targets = circular_left_shift(pawns, diff) & file_mask[dir];

        // ADD ATTACKS
        attacks = enemy & targets;
        add_moves_with_diff(diff, attacks & (~promotions_mask[side]), list, board, NO_FLAGS, 0);

        // ADD EP ATTACKS
        if (board.irrev.ep_square != NULL_SQUARE)
        {
            ep_attacks = targets & (1ULL << board.irrev.ep_square);
            add_moves_with_diff(diff, ep_attacks, list, board, EP_CAPTURE, PAWN | (!side));
        }

        // ADD PROMOTION ATTACKS
        promotions = attacks & promotions_mask[side];
        add_promotions_with_diff(diff, promotions, list, board, side);
    }
}

void MoveGenerator::add_knight_moves(class MoveList& list, const class Board& board, const U8 side)
{
    U64 knights = board.bitboards[KNIGHT | side];
    U64 non_friendly = ~board.bitboards[side];
    while (knights)
    {
        U8 from = bit_scan_forward(knights);
        U64 targets = KNIGHT_LOOKUP_TABLE[from] & non_friendly;
        add_moves(from, targets, list, board, NO_FLAGS);
        knights &= knights - 1;
    }
}

void MoveGenerator::add_king_moves(class MoveList& list, const class Board& board, const U8 side)
{
    U64 kings = board.bitboards[KING | side];
    U64 non_friendly = ~board.bitboards[side];
    while (kings)
    {
        U8 from = bit_scan_forward(kings);
        U64 targets = KING_LOOKUP_TABLE[from] & non_friendly;
        add_moves(from, targets, list, board, NO_FLAGS);
        kings &= kings - 1;
    }
}

void MoveGenerator::add_all_moves(class MoveList& list, const class Board& board, const U8 side)
{
    add_pawn_pushes(list, board, side);
    add_pawn_attacks(list, board, side);
    add_knight_moves(list, board, side);
    add_bishop_moves(list, board, side);
    add_rook_moves(list, board, side);
    add_queen_moves(list, board, side);
    add_king_moves(list, board, side);
#ifndef NDEBUG
    assert(list.contains_valid_moves(board));
#endif
}

void MoveGenerator::score_moves(class MoveList& list, const class Board& board)
{
    int n = list.length();

    for (int i = 0; i < n; i++)
    {
        Move_t move = list[i];
        U8 from = move_from(move);
        U8 to = move_to(move);
        U8 score = MVVLVA[board[to] >> 1][board[from] >> 1];

        move_add_score(&move, score);
        list.set_move(i, move);
    }
}

#define INDENT "    "
void MoveGenerator::generate_move_lookup_tables()
{
    cout << "GENERATING LOOKUP TABLES FOR KNIGHT MOVES" << endl;
    cout << "const U64 KNIGHT_LOOKUP_TABLE[64] = {" << endl << INDENT;
    for (int row = 0; row < 8; row++)
    {
        for (int file = 0; file < 8; file++)
        {
            int square = row * 8 + file;
            // calculate each one individually starting with up1 left2 and working clockwise
            U64 targets = 0ULL;
            if (file >= 2 && row <= 6)
            {
                targets |= 1ULL << (square + 8 - 2);
            }
            if (file >= 1 && row <= 5)
            {
                targets |= 1ULL << (square + 16 - 1);
            }
            if (file <= 6 && row <= 5)
            {
                targets |= 1ULL << (square + 16 + 1);
            }
            if (file <= 5 && row <= 6)
            {
                targets |= 1ULL << (square + 8 + 2);
            }
            if (file >= 2 && row >= 1)
            {
                targets |= 1ULL << (square - 8 - 2);
            }
            if (file >= 1 && row >= 2)
            {
                targets |= 1ULL << (square - 16 - 1);
            }
            if (file <= 6 && row >= 2)
            {
                targets |= 1ULL << (square - 16 + 1);
            }
            if (file <= 5 && row >= 1)
            {
                targets |= 1ULL << (square - 8 + 2);
            }
            printf("0x%016llXULL", targets);
            cout << ", ";
            if (file == 3)
                cout << endl << INDENT;
        }
        cout << endl << INDENT;
    }
    cout << "};" << endl;
    cout << "GENERATING LOOKUP TABLES FOR KING MOVES" << endl;
    cout << "const U64 KING_LOOKUP_TABLE[64] = {" << endl << INDENT;
    for (int row = 0; row < 8; row++)
    {
        for (int file = 0; file < 8; file++)
        {
            int square = row * 8 + file;
            // calculate each one individually starting with up1 left1 and working clockwise
            U64 targets = 0ULL;
            if (file >= 1 && row <= 6)
                targets |= 1ULL << (square + 7);
            if (row <= 6)
                targets |= 1ULL << (square + 8);
            if (file <= 6 && row <= 6)
                targets |= 1ULL << (square + 9);
            if (file <= 6)
                targets |= 1ULL << (square + 1);
            if (file <= 6 && row >= 1)
                targets |= 1ULL << (square - 7);
            if (row >= 1)
                targets |= 1ULL << (square - 8);
            if (file >= 1 && row >= 1)
                targets |= 1ULL << (square - 9);
            if (file >= 1)
                targets |= 1ULL << (square - 1);
            printf("0x%016llXULL", targets);
            cout << ", ";
            if (file == 3)
                cout << endl << INDENT;
        }
        cout << endl << INDENT;
    }
    cout << "};" << endl;
    cout << "GENERATING LOOKUP TABLES FOR SQUARES BETWEEN" << endl;
    cout << "const U64 SQUARES_BETWEEN[64][64] = {" << endl << INDENT;
    for (U8 from = 0; from < 64; from++)
    {
        cout << "{ ";
        for (U8 to = 0; to < 64; to++)
        {
            if (to % 4 == 0)
                cout << endl << INDENT << INDENT;
            U64 between = in_between(from, to);
            printf("0x%016llXULL, ", between);
        }
        cout << endl << INDENT << INDENT;
        cout << "},";
        cout << endl << INDENT;
    }
    cout << "};" << endl;
}

U64 MoveGenerator::get_checkers(const class Board& board, const U8 side)
{
    const int diffs[2][2] = { { 7, 64 - 9 }, { 9, 64 - 7 } };
    const U64 file_mask[2] = { ~FILE_H, ~FILE_A };
    U8 attacker_side = !side;

    U64 king = board.bitboards[KING | side];
    U64 occupied = board.bitboards[WHITE] | board.bitboards[BLACK];
    U8 king_sq = bit_scan_forward(king);

    U64 checkers = BB_EMPTY;

    // Knights
    U64 knights = board.bitboards[KNIGHT | attacker_side];
    checkers |= KNIGHT_LOOKUP_TABLE[king_sq] & knights;

    // Pawns
    U64 pawns = board.bitboards[PAWN | attacker_side];
    for (int dir = 0; dir < 2; dir++)
    {
        int diff = diffs[dir][side];
        U64 targets = circular_left_shift(king, diff) & file_mask[dir];
        checkers |= targets & pawns;
    }

    // Sliders
    U64 queens = board.bitboards[QUEEN | attacker_side];
    U64 rooks = board.bitboards[ROOK | attacker_side];
    U64 bishops = board.bitboards[BISHOP | attacker_side];

    U64 diag_attackers = queens | bishops;
    U64 non_diag_attackers = queens | rooks;

    checkers |=
        (file_attacks(occupied, king_sq) | rank_attacks(occupied, king_sq)) & non_diag_attackers;
    checkers |=
        (diag_attacks(occupied, king_sq) | anti_diag_attacks(occupied, king_sq)) & diag_attackers;

    return checkers;
}

MoveGenPreprocessing MoveGenerator::get_checkers_and_pinned(const class Board& board, const U8 side)
{
    const int diffs[2][2] = { { 7, 64 - 9 }, { 9, 64 - 7 } };
    const U64 file_mask[2] = { ~FILE_H, ~FILE_A };
    MoveGenPreprocessing mgp;
    U8 attacker_side = !side;

    U64 king = board.bitboards[KING | side];
    U64 occupied = board.bitboards[WHITE] | board.bitboards[BLACK];
    U64 friendly = board.bitboards[side];
    U8 king_sq = bit_scan_forward(king);

    U64 checkers = BB_EMPTY;
    U64 pinners = BB_EMPTY;
    U64 pinned = BB_EMPTY;

    // Pawns and Knights can only be checkers, not pinners
    // Knights
    U64 knights = board.bitboards[KNIGHT | attacker_side];
    checkers |= KNIGHT_LOOKUP_TABLE[king_sq] & knights;

    // Pawns
    U64 pawns = board.bitboards[PAWN | attacker_side];
    for (int dir = 0; dir < 2; dir++)
    {
        int diff = diffs[dir][side];
        U64 targets = circular_left_shift(king, diff) & file_mask[dir];
        checkers |= targets & pawns;
    }

    // Sliding pieces can be checkers or pinners depending on occupancy of intermediate squares
    U64 queens = board.bitboards[QUEEN | attacker_side];
    U64 rooks = board.bitboards[ROOK | attacker_side];
    U64 bishops = board.bitboards[BISHOP | attacker_side];

    U64 diag_attackers = queens | bishops;
    U64 non_diag_attackers = queens | rooks;

    U64 potential_king_attackers = BB_EMPTY;
    potential_king_attackers |=
        (file_mask_ex(king_sq) | rank_mask_ex(king_sq)) & non_diag_attackers;
    potential_king_attackers |=
        (diag_mask_ex(king_sq) | anti_diag_mask_ex(king_sq)) & diag_attackers;
    potential_king_attackers &= occupied;

    while (potential_king_attackers)
    {
        U8 from = bit_scan_forward(potential_king_attackers);
        U64 potentially_pinned = squares_between(from, king_sq) & occupied;
        U64 potentially_pinned_friendly = potentially_pinned & friendly;

        // If there are no pieces between the attacker and the king
        // then the attacker is giving check
        if (potentially_pinned == BB_EMPTY)
        {
            checkers |= 1ULL << (from);
        }
        // If there is a friendly piece between the attacker and the king
        // then it is pinned
        else if (pop_count(potentially_pinned_friendly) == 1)
        {
            pinned |= potentially_pinned_friendly;
            pinners |= 1ULL << (from);
        }
        potential_king_attackers &= potential_king_attackers - 1;
    }

    mgp.checkers = checkers;
    mgp.pinned = pinned;
    mgp.pinners = pinners;

    return mgp;
}
