/*
 * File:   MoveGenerator.cpp
 *
 */

#include "MoveGenerator.h"

#include "LookupTables.h"

void MoveGenerator::add_moves(
    U8 from, U64 targets, class MoveList& list, const class Board& board, const U32 flags)
{
    while (targets)
    {
        U8 to = bit_scan_forward(targets);
        U8 capture = board[to];
        Move_t move = build_move_all(from, to, capture, flags);
        list.push(move);
        targets &= targets - 1;
    }
}

void MoveGenerator::add_moves_with_diff(int diff,
                                        U64 targets,
                                        class MoveList& list,
                                        const class Board& board,
                                        const U32 flags,
                                        const U8 extra_capture)
{
    while (targets)
    {
        U8 to = bit_scan_forward(targets);
        U8 from = static_cast<U8>(to - diff) % 64;
        U8 capture = board[to] | extra_capture;
        Move_t move = build_move_all(from, to, capture, flags);
        list.push(move);
        targets &= targets - 1;
    }
}

void MoveGenerator::add_promotions_with_diff(
    int diff, U64 targets, class MoveList& list, const class Board& board, const U8 side)
{
    U32 flags = static_cast<U32>(side << FLAGS_SHIFT);
    while (targets)
    {
        U8 to = bit_scan_forward(targets);
        U8 from = static_cast<U8>(to - diff) % 64;
        U8 capture = board[to];
        Move_t move = build_move_all(from, to, capture, flags);
        list.push(move | (KNIGHT << FLAGS_SHIFT));
        list.push(move | (BISHOP << FLAGS_SHIFT));
        list.push(move | (ROOK << FLAGS_SHIFT));
        list.push(move | (QUEEN << FLAGS_SHIFT));
        targets &= targets - 1;
    }
}

// https://www.chessprogramming.org/Square_Attacked_By#In_Between
U64 MoveGenerator::squares_between_calc(U8 sq1, U8 sq2)
{
    const U64 m1 = 0 - C64(1);
    const U64 a2a7 = C64(0x0001010101010100);
    const U64 b2g7 = C64(0x0040201008040200);
    const U64 h1b7 = C64(0x0002040810204080);
    U64 btwn, neg_btwn, line, rank, file;

    btwn = (m1 << sq1) ^ (m1 << sq2);
    neg_btwn = 0 - btwn;
    file = (sq2 & C64(7)) - (sq1 & C64(7));
    rank = ((sq2 | C64(7)) - sq1) >> 3;
    line = ((file & 7) - 1) & a2a7;            /* a2a7 if same file */
    line += 2 * (((rank & 7) - 1) >> 58);      /* b1g1 if same rank */
    line += (((rank - file) & 15) - 1) & b2g7; /* b2g7 if same diagonal */
    line += (((rank + file) & 15) - 1) & h1b7; /* h1b7 if same antidiag */
    line *= btwn & neg_btwn;                   /* mul acts like shift by smaller square */
    return line & btwn;                        /* return the bits on that line in-between */
}

U64 MoveGenerator::lines_along_calc(U8 sq1, U8 sq2)
{
    if (sq1 == sq2)
    {
        return 0ULL;
    }
    int file = (sq2 & 7) - (sq1 & 7);
    int row = (sq2 >> 3) - (sq1 >> 3);

    // Same rank: if their rank distance is zero
    // https://www.chessprogramming.org/Ranks#Two_Squares_on_a_Rank
    if (row == 0)
    {
        return rank_mask(sq1);
    }
    // Same file: if their file distance is zero
    // https://www.chessprogramming.org/Files#TwoSquares
    if (file == 0)
    {
        return file_mask(sq1);
    }
    // Same diagonal: if their file distance equals the rank distance
    // https://www.chessprogramming.org/Diagonals#TwoSquares
    if (row == file)
    {
        return diag_mask(sq1);
    }
    // Same anti-diagonal: if sum of file and rank distance is zero
    // https://www.chessprogramming.org/Anti-Diagonals#TwoSquares
    if (row + file == 0)
    {
        return anti_diag_mask(sq1);
    }
    return 0ULL;
}

// Sliding piece attacks
// https://www.chessprogramming.org/Sliding_Piece_Attacks

// Ray Masks (should be turned into lookup tables)
U64 MoveGenerator::rank_mask_calc(int sq)
{
    return C64(0xff) << (sq & 56);
}

U64 MoveGenerator::file_mask_calc(int sq)
{
    return C64(0x0101010101010101) << (sq & 7);
}

U64 MoveGenerator::diag_mask_calc(int sq)
{
    const U64 maindia = C64(0x8040201008040201);
    int diag = 8 * (sq & 7) - (sq & 56);
    int nort = -diag & (diag >> 31);
    int sout = diag & (-diag >> 31);
    return (maindia >> sout) << nort;
}

U64 MoveGenerator::anti_diag_mask_calc(int sq)
{
    const U64 maindia = C64(0x0102040810204080);
    int diag = 56 - 8 * (sq & 7) - (sq & 56);
    int nort = -diag & (diag >> 31);
    int sout = diag & (-diag >> 31);
    return (maindia >> sout) << nort;
}

// excluding the square bit:
U64 MoveGenerator::rank_mask_ex_calc(int sq)
{
    return (C64(1) << sq) ^ rank_mask_calc(sq);
}

U64 MoveGenerator::file_mask_ex_calc(int sq)
{
    return (C64(1) << sq) ^ file_mask_calc(sq);
}

U64 MoveGenerator::diag_mask_ex_calc(int sq)
{
    return (C64(1) << sq) ^ diag_mask_calc(sq);
}

U64 MoveGenerator::anti_diag_mask_ex_calc(int sq)
{
    return (C64(1) << sq) ^ anti_diag_mask_calc(sq);
}

U64 MoveGenerator::rook_mask_calc(int sq)
{
    return rank_mask_calc(sq) | file_mask_calc(sq);
}

U64 MoveGenerator::bishop_mask_calc(int sq)
{
    return diag_mask_calc(sq) | anti_diag_mask_calc(sq);
}

U64 MoveGenerator::rook_mask_ex_calc(int sq)
{
    return rank_mask_calc(sq) ^ file_mask_calc(sq);
}

U64 MoveGenerator::bishop_mask_ex_calc(int sq)
{
    return diag_mask_calc(sq) ^ anti_diag_mask_calc(sq);
}

// https://www.chessprogramming.org/Efficient_Generation_of_Sliding_Piece_Attacks
U64 MoveGenerator::diag_attacks(U64 occ, int sq)
{
    return diag_attacks_hyperbola(occ, sq);
}

U64 MoveGenerator::anti_diag_attacks(U64 occ, int sq)
{
    return anti_diag_attacks_hyperbola(occ, sq);
}

U64 MoveGenerator::file_attacks(U64 occ, int sq)
{
    return file_attacks_hyperbola(occ, sq);
}

U64 MoveGenerator::rank_attacks(U64 occ, int sq)
{
    return rank_attacks_hyperbola(occ, sq);
}

U64 MoveGenerator::rook_attacks_slow(U64 occ, int sq)
{
    return file_attacks(occ, sq) + rank_attacks(occ, sq);
}

U64 MoveGenerator::bishop_attacks_slow(U64 occ, int sq)
{
    return diag_attacks(occ, sq) + anti_diag_attacks(occ, sq);
}

U64 MoveGenerator::rook_attacks(U64 occ, int sq)
{
    return rook_attacks_magic(occ, sq);
}

U64 MoveGenerator::bishop_attacks(U64 occ, int sq)
{
    return bishop_attacks_magic(occ, sq);
}

bool MoveGenerator::ep_move_discovers_check(const class Board& board,
                                            U64 from_bb,
                                            U64 to_bb,
                                            const U8 side)
{
    U64 occupied = (board.bitboards_[WHITE] | board.bitboards_[BLACK]) ^ from_bb ^ to_bb;
    U8 attacker_side = !side;
    U64 queens = board.bitboards_[QUEEN | attacker_side];
    U64 rooks = board.bitboards_[ROOK | attacker_side];
    U64 non_diag_attackers = queens | rooks;

    U64 kings = board.bitboards_[KING | side];
#ifndef NDEBUG
    assert(pop_count(kings) == 1);
#endif
    U8 king_sq = bit_scan_forward(kings);

    return ((rank_attacks(occupied, king_sq) & non_diag_attackers) != BB_EMPTY);
}

void MoveGenerator::add_rook_moves(class MoveList& list, const class Board& board, const U8 side)
{
    U64 rooks = board.bitboards_[ROOK | side];
    U64 occupied = board.bitboards_[WHITE] | board.bitboards_[BLACK];
    U64 non_friendly = ~board.bitboards_[side];

#if 0
    cout << "rooks=0x" << hex << rooks<< endl;
    cout << Output::bitboard(rooks);
    cout << "occupied=0x" << hex << occupied<< endl;
    cout << Output::bitboard(occupied);
    cout << "non_friendly=0x" << hex << non_friendly<< endl;
    cout << Output::bitboard(non_friendly);
    cout << dec;
#endif

    while (rooks)
    {
        U8 from = bit_scan_forward(rooks);

        U64 targets;
        // Add file and rank attacks
        targets = rook_attacks(occupied, from);
        targets &= non_friendly;
        add_moves(from, targets, list, board, NO_FLAGS);

        rooks &= rooks - 1;
    }
    // cout << "found " << dec <<list.length() << " moves" << endl;
}

void MoveGenerator::add_bishop_moves(class MoveList& list, const class Board& board, const U8 side)
{
    U64 bishops = board.bitboards_[BISHOP | side];
    U64 occupied = board.bitboards_[WHITE] | board.bitboards_[BLACK];
    U64 non_friendly = ~board.bitboards_[side];

    while (bishops)
    {
        U8 from = bit_scan_forward(bishops);

        U64 targets;
        // Add diagonal and antidiagonal attacks
        targets = bishop_attacks(occupied, from);
        targets &= non_friendly;
        add_moves(from, targets, list, board, NO_FLAGS);

        bishops &= bishops - 1;
    }
}

void MoveGenerator::add_queen_moves(class MoveList& list, const class Board& board, const U8 side)
{
    U64 queens = board.bitboards_[QUEEN | side];
    U64 occupied = board.bitboards_[WHITE] | board.bitboards_[BLACK];
    U64 non_friendly = ~board.bitboards_[side];

    while (queens)
    {
        U8 from = bit_scan_forward(queens);

        U64 targets;
        // Add diagonal, antidiagonal, file and rank attacks
        targets = bishop_attacks(occupied, from) + rook_attacks(occupied, from);
        targets &= non_friendly;
        add_moves(from, targets, list, board, NO_FLAGS);

        queens &= queens - 1;
    }
}

void MoveGenerator::add_pawn_pushes(class MoveList& list, const class Board& board, const U8 side)
{
    const int diffs[2] = { 8, 64 - 8 };
    const U64 promotions_mask[2] = { ROW_8, ROW_1 };
    const U64 start_row_plus_one_mask[2] = { ROW_3, ROW_6 };
    U64 pushes, double_pushes, promotions, pawns, empty_squares;

    int diff = diffs[side];
    pawns = board.bitboards_[PAWN | side];
    empty_squares = ~(board.bitboards_[WHITE] | board.bitboards_[BLACK]);

    // ADD SINGLE PUSHES
    pushes = circular_left_shift(pawns, diff) & empty_squares;
    add_moves_with_diff(diff, pushes & (~promotions_mask[side]), list, board, NO_FLAGS, 0);

    // ADD PROMOTIONS
    promotions = pushes & promotions_mask[side];
    add_promotions_with_diff(diff, promotions, list, board, side);

    // ADD DOUBLE PUSHES
    double_pushes =
        circular_left_shift(pushes & start_row_plus_one_mask[side], diff) & empty_squares;
    add_moves_with_diff(diff + diff, double_pushes, list, board, PAWN_DOUBLE_PUSH, 0);
}

void MoveGenerator::add_pawn_attacks(class MoveList& list, const class Board& board, const U8 side)
{
    const int diffs[2][2] = { { 7, 64 - 9 }, { 9, 64 - 7 } };
    const U64 promotions_mask[2] = { ROW_8, ROW_1 };
    const U64 pawn_file_mask[2] = { ~FILE_H, ~FILE_A };
    U64 attacks, ep_attacks, promotions, targets, pawns, enemy;

    pawns = board.bitboards_[PAWN | side];
    enemy = board.bitboards_[!side];

    // CALCULATE ATTACKS FOR LEFT, RIGHT
    for (int dir = 0; dir < 2; dir++)
    {
        int diff = diffs[dir][side];
        targets = circular_left_shift(pawns, diff) & pawn_file_mask[dir];

        // ADD ATTACKS
        attacks = enemy & targets;
        add_moves_with_diff(diff, attacks & (~promotions_mask[side]), list, board, NO_FLAGS, 0);

        // ADD EP ATTACKS
        if (board.irrev_.ep_square != NULL_SQUARE)
        {
            ep_attacks = targets & (1ULL << board.irrev_.ep_square);
            add_moves_with_diff(diff, ep_attacks, list, board, EP_CAPTURE, PAWN | (!side));
        }

        // ADD PROMOTION ATTACKS
        promotions = attacks & promotions_mask[side];
        add_promotions_with_diff(diff, promotions, list, board, side);
    }
}

void MoveGenerator::add_knight_moves(class MoveList& list, const class Board& board, const U8 side)
{
    U64 knights = board.bitboards_[KNIGHT | side];
    U64 non_friendly = ~board.bitboards_[side];
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
    U64 kings = board.bitboards_[KING | side];
    U64 non_friendly = ~board.bitboards_[side];
    while (kings)
    {
        U8 from = bit_scan_forward(kings);
        U64 targets = KING_LOOKUP_TABLE[from] & non_friendly;
        add_moves(from, targets, list, board, NO_FLAGS);
        kings &= kings - 1;
    }
}

void MoveGenerator::add_pawn_legal_pushes(
    class MoveList& list, const class Board& board, U64 to_mask, U64 from_mask, const U8 side)
{
    const int diffs[2] = { 8, 64 - 8 };
    const U64 promotions_mask[2] = { ROW_8, ROW_1 };
    const U64 start_row_plus_one_mask[2] = { ROW_3, ROW_6 };
    U64 pushes, single_pushes, double_pushes, promotions, pawns, empty_squares;

    int diff = diffs[side];
    pawns = board.bitboards_[PAWN | side] & from_mask;
    empty_squares = ~(board.bitboards_[WHITE] | board.bitboards_[BLACK]);

    // ADD SINGLE PUSHES
    // Don't apply to_mask here to avoid masking double pushes
    pushes = circular_left_shift(pawns, diff) & empty_squares;
    single_pushes = pushes & (~promotions_mask[side]);
    add_moves_with_diff(diff, single_pushes & to_mask, list, board, NO_FLAGS, 0);

    // ADD PROMOTIONS
    promotions = pushes & promotions_mask[side];
    add_promotions_with_diff(diff, promotions & to_mask, list, board, side);

    // ADD DOUBLE PUSHES
    double_pushes =
        circular_left_shift(pushes & start_row_plus_one_mask[side], diff) & empty_squares;
    add_moves_with_diff(diff + diff, double_pushes & to_mask, list, board, PAWN_DOUBLE_PUSH, 0);
}

void MoveGenerator::add_pawn_legal_attacks(class MoveList& list,
                                           const class Board& board,
                                           U64 capture_mask,
                                           U64 push_mask,
                                           U64 from_mask,
                                           const U8 side)
{
    const int diffs[2][2] = { { 7, 64 - 9 }, { 9, 64 - 7 } };
    const U64 promotions_mask[2] = { ROW_8, ROW_1 };
    const U64 pawn_file_mask[2] = { ~FILE_H, ~FILE_A };
    U64 attacks, ep_attacks, promotions, targets, pawns;

    pawns = board.bitboards_[PAWN | side] & from_mask;

    // CALCULATE ATTACKS FOR LEFT, RIGHT
    for (int dir = 0; dir < 2; dir++)
    {
        int diff = diffs[dir][side];
        targets = circular_left_shift(pawns, diff) & pawn_file_mask[dir];

        // ADD ATTACKS
        attacks = targets & capture_mask;
        add_moves_with_diff(diff, attacks & (~promotions_mask[side]), list, board, NO_FLAGS, 0);

        // ADD EP ATTACKS
        if (board.irrev_.ep_square != NULL_SQUARE)
        {
            ep_attacks = targets & (1ULL << board.irrev_.ep_square);
            while (ep_attacks)
            {
                U8 to = bit_scan_forward(ep_attacks);
                U64 to_bb = 1ULL << to;
                U8 from = static_cast<U8>(to - diff) % 64;
                // along_row_with_col
                // returns a square at the same row as "from", and the same col as "to"
                U8 captured_sq = (from & 56) | (to & 7);
                U64 capture_sq_bb = 1ULL << captured_sq;

                // can only make ep capture if moving to push_mask, or capturing on capture mask
                if ((to_bb & push_mask) | (capture_sq_bb & capture_mask))
                {
                    // ensure that there is no discovered check
                    U64 from_bb = 1ULL << from;
                    if (!ep_move_discovers_check(board, from_bb, capture_sq_bb, side))
                    {
                        add_moves_with_diff(
                            diff, ep_attacks, list, board, EP_CAPTURE, PAWN | (!side));
                    }
                }
                ep_attacks &= ep_attacks - 1;
            }
        }

        // ADD PROMOTION ATTACKS
        promotions = attacks & promotions_mask[side];
        add_promotions_with_diff(diff, promotions, list, board, side);
    }
}

void MoveGenerator::add_pawn_legal_moves(class MoveList& list,
                                         const class Board& board,
                                         U64 capture_mask,
                                         U64 push_mask,
                                         U64 from_mask,
                                         const U8 side)
{
    add_pawn_legal_pushes(list, board, push_mask, from_mask, side);
    add_pawn_legal_attacks(list, board, capture_mask, push_mask, from_mask, side);
}

void MoveGenerator::add_pawn_pin_ray_moves(class MoveList& list,
                                           const class Board& board,
                                           U64 capture_mask,
                                           U64 push_mask,
                                           U64 pinned_mask,
                                           U8 king_sq,
                                           const U8 side)
{
    // Generates pawn moves along pin rays
    const int pushes_diffs[2] = { 8, 64 - 8 };
    const int attacks_diffs[2][2] = { { 7, 64 - 9 }, { 9, 64 - 7 } };
    const U64 pawn_file_mask[2] = { ~FILE_H, ~FILE_A };
    const U64 promotions_mask[2] = { ROW_8, ROW_1 };
    const U64 start_row_plus_one_mask[2] = { ROW_3, ROW_6 };
    U64 pushes, single_pushes, double_pushes, promotions, pawns, empty_squares, movers;
    pawns = board.bitboards_[PAWN | side];
    empty_squares = ~(board.bitboards_[WHITE] | board.bitboards_[BLACK]);
    movers = pawns & pinned_mask;

    // exit early if no pinned pawns
    if (movers == BB_EMPTY)
    {
        return;
    }

    int diff = pushes_diffs[side];
    U64 can_push = movers & file_mask(king_sq);
    U64 king_diags = bishop_mask_ex(king_sq);
    U64 can_capture = movers & king_diags;

    // For pinned pawns, only possible moves are those along the king file
    // ADD SINGLE PUSHES
    pushes = circular_left_shift(can_push, diff) & empty_squares;
    single_pushes = pushes & (~promotions_mask[side]);
    add_moves_with_diff(diff, single_pushes & push_mask, list, board, NO_FLAGS, 0);

    // ADD DOUBLE PUSHES
    double_pushes =
        circular_left_shift(pushes & start_row_plus_one_mask[side], diff) & empty_squares;
    add_moves_with_diff(diff + diff, double_pushes & push_mask, list, board, PAWN_DOUBLE_PUSH, 0);

    // CALCULATE ATTACKS FOR LEFT, RIGHT
    for (int dir = 0; dir < 2; dir++)
    {
        diff = attacks_diffs[dir][side];
        U64 targets = circular_left_shift(can_capture, diff) & pawn_file_mask[dir];

        // ADD ATTACKS
        U64 attacks = targets & capture_mask & king_diags;
        add_moves_with_diff(diff, attacks & (~promotions_mask[side]), list, board, NO_FLAGS, 0);

        // ADD EP ATTACKS
        if (board.irrev_.ep_square != NULL_SQUARE)
        {
            U64 ep_attacks = targets & (1ULL << board.irrev_.ep_square) & king_diags;
            while (ep_attacks)
            {
                U8 to = bit_scan_forward(ep_attacks);
                U64 to_bb = 1ULL << to;
                U8 from = static_cast<U8>(to - diff) % 64;
                // along_row_with_col
                // returns a square at the same row as "from", and the same col as "to"
                U8 captured_sq = (from & 56) | (to & 7);
                U64 capture_sq_bb = 1ULL << captured_sq;

                // can only make ep capture if moving along king_diags, or capturing on capture mask
                if ((to_bb & king_diags) | (capture_sq_bb & capture_mask))
                {
                    // ensure that there is no discovered check
                    U64 from_bb = 1ULL << from;
                    if (!ep_move_discovers_check(board, from_bb, capture_sq_bb, side))
                    {
                        add_moves_with_diff(
                            diff, ep_attacks, list, board, EP_CAPTURE, PAWN | (!side));
                    }
                }
                ep_attacks &= ep_attacks - 1;
            }
        }

        // ADD PROMOTION ATTACKS
        promotions = attacks & promotions_mask[side];
        add_promotions_with_diff(diff, promotions, list, board, side);
    }
}

void MoveGenerator::add_pawn_pin_ray_attacks(class MoveList& list,
                                             const class Board& board,
                                             U64 capture_mask,
                                             U64 pinned_mask,
                                             U8 king_sq,
                                             const U8 side)
{
    // Generates pawn moves along pin rays
    const int attacks_diffs[2][2] = { { 7, 64 - 9 }, { 9, 64 - 7 } };
    const U64 pawn_file_mask[2] = { ~FILE_H, ~FILE_A };
    const U64 promotions_mask[2] = { ROW_8, ROW_1 };
    U64 promotions, pawns, movers;
    pawns = board.bitboards_[PAWN | side];
    movers = pawns & pinned_mask;

    // exit early if no pinned pawns
    if (movers == BB_EMPTY)
    {
        return;
    }

    U64 king_diags = bishop_mask_ex(king_sq);
    U64 can_capture = movers & king_diags;

    // CALCULATE ATTACKS FOR LEFT, RIGHT
    for (int dir = 0; dir < 2; dir++)
    {
        int diff = attacks_diffs[dir][side];
        U64 targets = circular_left_shift(can_capture, diff) & pawn_file_mask[dir];

        // ADD ATTACKS
        U64 attacks = targets & capture_mask & king_diags;
        add_moves_with_diff(diff, attacks & (~promotions_mask[side]), list, board, NO_FLAGS, 0);

        // ADD EP ATTACKS
        if (board.irrev_.ep_square != NULL_SQUARE)
        {
            U64 ep_attacks = targets & (1ULL << board.irrev_.ep_square) & king_diags;
            while (ep_attacks)
            {
                U8 to = bit_scan_forward(ep_attacks);
                U64 to_bb = 1ULL << to;
                U8 from = static_cast<U8>(to - diff) % 64;
                // along_row_with_col
                // returns a square at the same row as "from", and the same col as "to"
                U8 captured_sq = (from & 56) | (to & 7);
                U64 capture_sq_bb = 1ULL << captured_sq;

                // can only make ep capture if moving along king_diags, or capturing on capture mask
                if ((to_bb & king_diags) | (capture_sq_bb & capture_mask))
                {
                    // ensure that there is no discovered check
                    U64 from_bb = 1ULL << from;
                    if (!ep_move_discovers_check(board, from_bb, capture_sq_bb, side))
                    {
                        add_moves_with_diff(
                            diff, ep_attacks, list, board, EP_CAPTURE, PAWN | (!side));
                    }
                }
                ep_attacks &= ep_attacks - 1;
            }
        }

        // ADD PROMOTION ATTACKS
        promotions = attacks & promotions_mask[side];
        add_promotions_with_diff(diff, promotions, list, board, side);
    }
}

void MoveGenerator::add_slider_legal_moves(class MoveList& list,
                                           const class Board& board,
                                           U64 capture_mask,
                                           U64 push_mask,
                                           U64 pinned_mask,
                                           U8 king_sq,
                                           const U8 side)
{
    U64 occupied = board.bitboards_[WHITE] | board.bitboards_[BLACK];
    U64 queens = board.bitboards_[QUEEN | side];
    U64 rooks = board.bitboards_[ROOK | side];
    U64 bishops = board.bitboards_[BISHOP | side];
    U64 diag_attackers = queens | bishops;
    U64 non_diag_attackers = queens | rooks;

    // non-pinned attackers can move freely
    U64 attackers = non_diag_attackers & (~pinned_mask);
    while (attackers)
    {
        U8 from = bit_scan_forward(attackers);
        // Add file and rank attacks
        U64 targets = rook_attacks(occupied, from);
        add_moves(from, targets & capture_mask, list, board, NO_FLAGS);
        add_moves(from, targets & push_mask, list, board, NO_FLAGS);
        attackers &= attackers - 1;
    }

    // When a piece is pinned, it can only move towards or away from the pinner,
    // it can’t leave the line between the attacking piece and the king
    attackers = non_diag_attackers & pinned_mask;
    while (attackers)
    {
        U8 from = bit_scan_forward(attackers);
        // Add file and rank attacks along the line between the king and slider
        U64 ray_mask = lines_along(from, king_sq);
        U64 targets = rook_attacks(occupied, from) & ray_mask;
        add_moves(from, targets & capture_mask, list, board, NO_FLAGS);
        add_moves(from, targets & push_mask, list, board, NO_FLAGS);
        attackers &= attackers - 1;
    }

    attackers = diag_attackers & (~pinned_mask);
    while (attackers)
    {
        U8 from = bit_scan_forward(attackers);
        // Add diag and anti-diag attacks
        U64 targets = bishop_attacks(occupied, from);
        add_moves(from, targets & capture_mask, list, board, NO_FLAGS);
        add_moves(from, targets & push_mask, list, board, NO_FLAGS);
        attackers &= attackers - 1;
    }

    attackers = diag_attackers & pinned_mask;
    while (attackers)
    {
        U8 from = bit_scan_forward(attackers);
        // Add diag and anti-diag attacks along the line between the king and slider
        U64 ray_mask = lines_along(from, king_sq);
        U64 targets = bishop_attacks(occupied, from) & ray_mask;
        add_moves(from, targets & capture_mask, list, board, NO_FLAGS);
        add_moves(from, targets & push_mask, list, board, NO_FLAGS);
        attackers &= attackers - 1;
    }
}

void MoveGenerator::add_slider_legal_attacks(class MoveList& list,
                                             const class Board& board,
                                             U64 capture_mask,
                                             U64 pinned_mask,
                                             U8 king_sq,
                                             const U8 side)
{
    U64 occupied = board.bitboards_[WHITE] | board.bitboards_[BLACK];
    U64 queens = board.bitboards_[QUEEN | side];
    U64 rooks = board.bitboards_[ROOK | side];
    U64 bishops = board.bitboards_[BISHOP | side];
    U64 diag_attackers = queens | bishops;
    U64 non_diag_attackers = queens | rooks;

    // non-pinned attackers can move freely
    U64 attackers = non_diag_attackers & (~pinned_mask);
    while (attackers)
    {
        U8 from = bit_scan_forward(attackers);
        // Add file and rank attacks
        U64 targets = rook_attacks(occupied, from);
        add_moves(from, targets & capture_mask, list, board, NO_FLAGS);
        attackers &= attackers - 1;
    }

    // When a piece is pinned, it can only move towards or away from the pinner,
    // it can’t leave the line between the attacking piece and the king
    attackers = non_diag_attackers & pinned_mask;
    while (attackers)
    {
        U8 from = bit_scan_forward(attackers);
        // Add file and rank attacks along the line between the king and slider
        U64 ray_mask = lines_along(from, king_sq);
        U64 targets = rook_attacks(occupied, from) & ray_mask;
        add_moves(from, targets & capture_mask, list, board, NO_FLAGS);
        attackers &= attackers - 1;
    }

    attackers = diag_attackers & (~pinned_mask);
    while (attackers)
    {
        U8 from = bit_scan_forward(attackers);
        // Add diag and anti-diag attacks
        U64 targets = bishop_attacks(occupied, from);
        add_moves(from, targets & capture_mask, list, board, NO_FLAGS);
        attackers &= attackers - 1;
    }

    attackers = diag_attackers & pinned_mask;
    while (attackers)
    {
        U8 from = bit_scan_forward(attackers);
        // Add diag and anti-diag attacks along the line between the king and slider
        U64 ray_mask = lines_along(from, king_sq);
        U64 targets = bishop_attacks(occupied, from) & ray_mask;
        add_moves(from, targets & capture_mask, list, board, NO_FLAGS);
        attackers &= attackers - 1;
    }
}

void MoveGenerator::add_knight_legal_moves(class MoveList& list,
                                           const class Board& board,
                                           U64 capture_mask,
                                           U64 push_mask,
                                           U64 from_mask,
                                           const U8 side)
{
    U64 knights = board.bitboards_[KNIGHT | side] & from_mask;
    while (knights)
    {
        U8 from = bit_scan_forward(knights);
        U64 capture_targets = KNIGHT_LOOKUP_TABLE[from] & capture_mask;
        U64 push_targets = KNIGHT_LOOKUP_TABLE[from] & push_mask;
        add_moves(from, capture_targets, list, board, NO_FLAGS);
        add_moves(from, push_targets, list, board, NO_FLAGS);
        knights &= knights - 1;
    }
}

void MoveGenerator::add_knight_legal_attacks(
    class MoveList& list, const class Board& board, U64 capture_mask, U64 from_mask, const U8 side)
{
    U64 knights = board.bitboards_[KNIGHT | side] & from_mask;
    while (knights)
    {
        U8 from = bit_scan_forward(knights);
        U64 capture_targets = KNIGHT_LOOKUP_TABLE[from] & capture_mask;
        add_moves(from, capture_targets, list, board, NO_FLAGS);
        knights &= knights - 1;
    }
}

void MoveGenerator::add_king_legal_moves(
    class MoveList& list, const class Board& board, U64 capture_mask, U64 push_mask, const U8 side)
{
    U64 kings = board.bitboards_[KING | side];
    while (kings)
    {
        U8 from = bit_scan_forward(kings);
        U64 capture_targets = KING_LOOKUP_TABLE[from] & capture_mask;
        U64 push_targets = KING_LOOKUP_TABLE[from] & push_mask;
        add_moves(from, capture_targets, list, board, NO_FLAGS);
        add_moves(from, push_targets, list, board, NO_FLAGS);
        kings &= kings - 1;
    }
}

void MoveGenerator::add_king_legal_attacks(class MoveList& list,
                                           const class Board& board,
                                           U64 capture_mask,
                                           const U8 side)
{
    U64 kings = board.bitboards_[KING | side];
    while (kings)
    {
        U8 from = bit_scan_forward(kings);
        U64 capture_targets = KING_LOOKUP_TABLE[from] & capture_mask;
        add_moves(from, capture_targets, list, board, NO_FLAGS);
        kings &= kings - 1;
    }
}

void MoveGenerator::add_castles(class MoveList& list,
                                const class Board& board,
                                U64 attacks,
                                const U8 side)
{
    // squares that must be empty
    const U64 CASTLE_BLOCKING_SQUARES[2][2] = {
        {
            (1ULL << B1) + (1ULL << C1) + (1ULL << D1),  // WHITE QS = B1 + C1 + D1
            (1ULL << B8) + (1ULL << C8) + (1ULL << D8),  // BLACK QS = B8 + C8 + D8
        },
        {
            (1ULL << F1) + (1ULL << G1),  // WHITE KS = F1 + G1
            (1ULL << F8) + (1ULL << G8),  // BLACK KS = F8 + G8
        }
    };

    // squares that must be not attacked for a castle to take place
    const U64 KING_SAFE_SQUARES[2][2] = {
        {
            (1ULL << C1) + (1ULL << D1) + (1ULL << E1),  // WHITE QS = C1 + D1 + E1
            (1ULL << C8) + (1ULL << D8) + (1ULL << E8),  // BLACK QS = C8 + D8 + E8
        },
        {
            (1ULL << E1) + (1ULL << F1) + (1ULL << G1),  // WHITE KS = E1 + F1 + G1
            (1ULL << E8) + (1ULL << F8) + (1ULL << G8),  // BLACK KS = E8 + F8 + G8
        }
    };

    const U8 CASTLING_RIGHTS[2][2] = { { WHITE_QUEEN_SIDE, BLACK_QUEEN_SIDE },
                                       { WHITE_KING_SIDE, BLACK_KING_SIDE } };

    U8 rights = board.castling_rights();
    U64 occupied = board.bitboards_[WHITE] | board.bitboards_[BLACK];

    for (int castle_side = 0; castle_side < 2; castle_side++)
    {
        // NOTE: should not need to check king and rook pos since
        // should not be able to castle once these are moved
        U8 castling_right = CASTLING_RIGHTS[castle_side][side];
        U64 blockers = CASTLE_BLOCKING_SQUARES[castle_side][side];
        U64 king_safe = KING_SAFE_SQUARES[castle_side][side];

        if ((castling_right & rights) == 0)
        {
            continue;
        }

        if ((occupied & blockers) || (attacks & king_safe))
        {
            continue;
        }

        Move_t move = build_castle((castle_side == 0) ? QUEEN_CASTLE : KING_CASTLE);
        list.push(move);
    }
}

U64 MoveGenerator::rook_targets(U64 from, U64 occupied)
{
    U64 targets = BB_EMPTY;
    while (from)
    {
        U8 sq = bit_scan_forward(from);

        // Add file and rank attacks
        targets |= rook_attacks(occupied, sq);
        from &= from - 1;
    }
    return targets;
}

U64 MoveGenerator::bishop_targets(U64 from, U64 occupied)
{
    U64 targets = BB_EMPTY;
    while (from)
    {
        U8 sq = bit_scan_forward(from);

        // Add diagonal and antidiagonal attacks
        targets |= bishop_attacks(occupied, sq);
        from &= from - 1;
    }
    return targets;
}

U64 MoveGenerator::knight_targets(U64 from)
{
    U64 targets = BB_EMPTY;
    while (from)
    {
        U8 sq = bit_scan_forward(from);
        targets |= KNIGHT_LOOKUP_TABLE[sq];
        from &= from - 1;
    }
    return targets;
}

U64 MoveGenerator::king_targets(U64 from)
{
    U64 targets = BB_EMPTY;
    while (from)
    {
        U8 sq = bit_scan_forward(from);
        targets |= KING_LOOKUP_TABLE[sq];
        from &= from - 1;
    }
    return targets;
}

U64 MoveGenerator::pawn_targets(U64 from, U8 side)
{
    const int diffs[2][2] = { { 7, 64 - 9 }, { 9, 64 - 7 } };
    const U64 pawn_file_mask[2] = { ~FILE_H, ~FILE_A };
    U64 targets = BB_EMPTY;

    for (int dir = 0; dir < 2; dir++)
    {
        int diff = diffs[dir][side];
        targets |= circular_left_shift(from, diff) & pawn_file_mask[dir];
    }
    return targets;
}

bool MoveGenerator::in_check(const class Board& board, const U8 side)
{
    U64 checkers = get_checkers(board, side);
    int king_attacks_count = pop_count(checkers);
    return (king_attacks_count > 0);
}

void MoveGenerator::add_all_moves(class MoveList& list, const class Board& board, const U8 side)
{
    U64 kings = board.bitboards_[KING | side];
#ifndef NDEBUG
    assert(pop_count(kings) == 1);
#endif

    // We always need legal king moves
    U64 attacked_squares = get_king_danger_squares(board, side, kings);

    U8 king_sq = bit_scan_forward(kings);

    MoveGenPreprocessing mgp = get_checkers_and_pinned(board, side);
    U64 checkers = mgp.checkers;
    U64 pinned = mgp.pinned;
    U64 pinners = mgp.pinners;
    int king_attacks_count = pop_count(checkers);

    // cout << "board\n" << Output::board(board) << endl;
    // cout << "attacked_squares\n" << Output::bitboard(attacked_squares) << endl;
    // cout << "checkers\n" << Output::bitboard(checkers) << endl;
    // cout << "pinned\n" << Output::bitboard(pinned) << endl;
    // cout << "pinners\n" << Output::bitboard(pinners) << endl;

    // capture_mask and push_mask represent squares our pieces are allowed to move to or capture,
    // respectively. The difference between the two is only important for pawn EP captures
    // Since push_mask is used to block a pin, we ignore push_mask when calculating king moves
    U64 enemy = board.bitboards_[!side];
    U64 empty_squares = ~(board.bitboards_[WHITE] | board.bitboards_[BLACK]);

    U64 capture_mask = enemy;  // set of squares a piece can capture on
    U64 king_capture_mask = enemy & (~attacked_squares);
    U64 push_mask = empty_squares;  // set of squares a piece can move to
    U64 king_push_mask = empty_squares & (~attacked_squares);

    if (king_attacks_count > 1)
    {
        // multiple attackers... only solutions are king moves
        add_king_legal_moves(list, board, king_capture_mask, king_push_mask, side);
        return;
    }
    else if (king_attacks_count == 1)
    {
        // if only one attacker, we can try attacking the attacker with
        // our other pieces.
        capture_mask = checkers;
#ifndef NDEBUG
        assert(pop_count(checkers) == 1);
#endif
        U8 checker_sq = bit_scan_forward(checkers);
        U8 checker = board[checker_sq];

        if (is_piece_slider(checker))
        {
            // If the piece giving check is a slider, we can additionally attempt
            // to block the sliding piece
            push_mask &= squares_between(king_sq, checker_sq);
        }
        else
        {
            // If we are in check by a jumping piece (aka a knight) then
            // there are no valid non-captures to avoid check
            push_mask = BB_EMPTY;
        }
    }

    // generate moves for pinned and unpinned sliders
    add_slider_legal_moves(list, board, capture_mask, push_mask, pinned, king_sq, side);

    // generate moves for non-pinned knights (pinned knights can't move)
    add_knight_legal_moves(list, board, capture_mask, push_mask, ~pinned, side);

    // generate moves for unpinned pawns
    add_pawn_legal_moves(list, board, capture_mask, push_mask, ~pinned, side);

    // generate moves for pinned pawns
    // pinned pawn captures can only include pinners
    add_pawn_pin_ray_moves(list, board, capture_mask & pinners, push_mask, pinned, king_sq, side);

    if (king_attacks_count == 0)
    {
        // Not in check so can generate castles
        // impossible for castles to be affected by pins
        // so we don't need to consider pins here
        add_castles(list, board, attacked_squares, side);
    }

    add_king_legal_moves(list, board, king_capture_mask, king_push_mask, side);

    // return (king_attacks_count > 0)

#ifndef NDEBUG
    assert(list.contains_valid_moves(board, true));
#endif
}

/// Adds 'loud' legal moves to the move list.
///
/// Loud moves are defined as:
/// * Captures
/// * Check evasions
void MoveGenerator::add_loud_moves(class MoveList& list, const class Board& board, const U8 side)
{
    U64 kings = board.bitboards_[KING | side];
#ifndef NDEBUG
    assert(pop_count(kings) == 1);
#endif

    // We always need legal king moves
    U64 attacked_squares = get_king_danger_squares(board, side, kings);

    U8 king_sq = bit_scan_forward(kings);

    MoveGenPreprocessing mgp = get_checkers_and_pinned(board, side);
    U64 checkers = mgp.checkers;
    U64 pinned = mgp.pinned;
    U64 pinners = mgp.pinners;
    int king_attacks_count = pop_count(checkers);

    // cout << "board\n" << Output::board(board) << endl;
    // cout << "attacked_squares\n" << Output::bitboard(attacked_squares) << endl;
    // cout << "checkers\n" << Output::bitboard(checkers) << endl;
    // cout << "pinned\n" << Output::bitboard(pinned) << endl;
    // cout << "pinners\n" << Output::bitboard(pinners) << endl;

    // capture_mask and push_mask represent squares our pieces are allowed to move to or capture,
    // respectively. The difference between the two is only important for pawn EP captures
    // Since push_mask is used to block a pin, we ignore push_mask when calculating king moves
    U64 enemy = board.bitboards_[!side];
    U64 empty_squares = ~(board.bitboards_[WHITE] | board.bitboards_[BLACK]);

    U64 capture_mask = enemy;  // set of squares a piece can capture on
    U64 king_capture_mask = enemy & (~attacked_squares);
    U64 push_mask = empty_squares;  // set of squares a piece can move to
    U64 king_push_mask = empty_squares & (~attacked_squares);

    if (king_attacks_count > 1)
    {
        // multiple attackers... only solutions are king moves
        add_king_legal_moves(list, board, king_capture_mask, king_push_mask, side);
        return;
    }
    else if (king_attacks_count == 1)
    {
        // if only one attacker, we can try attacking the attacker with
        // our other pieces.
        capture_mask = checkers;
#ifndef NDEBUG
        assert(pop_count(checkers) == 1);
#endif
        U8 checker_sq = bit_scan_forward(checkers);
        U8 checker = board[checker_sq];

        if (is_piece_slider(checker))
        {
            // If the piece giving check is a slider, we can additionally attempt
            // to block the sliding piece
            push_mask &= squares_between(king_sq, checker_sq);
        }
        else
        {
            // If we are in check by a jumping piece (aka a knight) then
            // there are no valid non-captures to avoid check
            push_mask = BB_EMPTY;
        }

        // When in check evaluate moves in this order
        // 1. king moves
        // 2. knight moves
        // 3. sliding piece moves
        // 4. pawn moves
        // No need for pawn pin ray moves since cannot move a pinned piece if in check
        add_king_legal_moves(list, board, king_capture_mask, king_push_mask, side);
        add_knight_legal_moves(list, board, capture_mask, push_mask, ~pinned, side);
        add_slider_legal_moves(list, board, capture_mask, push_mask, pinned, king_sq, side);
        add_pawn_legal_moves(list, board, capture_mask, push_mask, ~pinned, side);
    }
    else
    {
        // When not in check evaluate moves in this order
        // 1. pawn captures
        // 2. pawn pin-ray captures
        // 3. knight captures
        // 4. sliding piece captures
        // 5. king captures
        add_pawn_legal_attacks(list, board, capture_mask, push_mask, ~pinned, side);
        add_pawn_pin_ray_attacks(list, board, capture_mask & pinners, pinned, king_sq, side);
        add_knight_legal_attacks(list, board, capture_mask, ~pinned, side);
        add_slider_legal_attacks(list, board, capture_mask, pinned, king_sq, side);
        add_king_legal_attacks(list, board, king_capture_mask, side);
    }
#ifndef NDEBUG
    assert(list.contains_valid_moves(board, true));
#endif
}

#ifdef MVVLVA
void MoveGenerator::score_moves(class MoveList& list, const class Board& board)
{
    int n = list.length();

    for (int i = 0; i < n; i++)
    {
        Move_t move = list[i];

        if (is_castle(move))
        {
            continue;
        }
        U8 from = move_from(move);
        U8 to = move_to(move);
        U8 score = MVVLVA[board[to] >> 1][board[from] >> 1];
        if (is_ep_capture(move))
        {
            score = MVVLVA[PAWN >> 1][PAWN >> 1];
        }
        move_set_score(&move, score);
        list.set_move(i, move);
    }
}
#else
void MoveGenerator::score_moves(class MoveList& list, const class Board& board)
{
    int n = list.length();

    for (int i = 0; i < n; i++)
    {
        Move_t move = list[i];
        U8 score = 0;

        if (is_capture(move))
        {
            U8 from = move_from(move);
            U8 to = move_to(move);
            U8 piece = board[from];
            U8 captured = board[to];
            score = MVVLVA[captured >> 1][piece >> 1];
            if (is_ep_capture(move))
            {
                score = MVVLVA[PAWN >> 1][PAWN >> 1];
            }
            if ((SEE_PIECE_VALUE[piece >> 1] - SEE_PIECE_VALUE[captured >> 1]) >= 0)
            {
                if (MoveGenerator::see(board, move) < 0)
                {
                    // Bad capture
                    score = 0;
                }
            }
        }
        else
        {
            score = 5;
        }
        if (is_promotion(move))
        {
            score = static_cast<U8>(score + 70);
        }

        move_set_score(&move, score);
        list.set_move(i, move);
    }
}
#endif

U64 MoveGenerator::get_checkers(const class Board& board, const U8 side)
{
    const int diffs[2][2] = { { 7, 64 - 9 }, { 9, 64 - 7 } };
    const U64 pawn_file_mask[2] = { ~FILE_H, ~FILE_A };
    U8 attacker_side = !side;

    U64 kings = board.bitboards_[KING | side];
    U64 checkers = BB_EMPTY;

    if (pop_count(kings) != 1)
    {
        return checkers;
    }
#ifndef NDEBUG
    assert(pop_count(kings) == 1);
#endif
    U64 occupied = board.bitboards_[WHITE] | board.bitboards_[BLACK];
    U8 king_sq = bit_scan_forward(kings);

    // Knights
    U64 knights = board.bitboards_[KNIGHT | attacker_side];
    checkers |= KNIGHT_LOOKUP_TABLE[king_sq] & knights;

    // Pawns
    U64 pawns = board.bitboards_[PAWN | attacker_side];
    for (int dir = 0; dir < 2; dir++)
    {
        int diff = diffs[dir][side];
        U64 targets = circular_left_shift(kings, diff) & pawn_file_mask[dir];
        checkers |= targets & pawns;
    }

    // Sliders
    U64 queens = board.bitboards_[QUEEN | attacker_side];
    U64 rooks = board.bitboards_[ROOK | attacker_side];
    U64 bishops = board.bitboards_[BISHOP | attacker_side];

    U64 diag_attackers = queens | bishops;
    U64 non_diag_attackers = queens | rooks;

    checkers |= rook_attacks(occupied, king_sq) & non_diag_attackers;
    checkers |= bishop_attacks(occupied, king_sq) & diag_attackers;

    return checkers;
}

MoveGenPreprocessing MoveGenerator::get_checkers_and_pinned(const class Board& board, const U8 side)
{
    const int diffs[2][2] = { { 7, 64 - 9 }, { 9, 64 - 7 } };
    const U64 pawn_file_mask[2] = { ~FILE_H, ~FILE_A };
    MoveGenPreprocessing mgp;
    U8 attacker_side = !side;

    U64 kings = board.bitboards_[KING | side];
#ifndef NDEBUG
    assert(pop_count(kings) == 1);
#endif
    U64 occupied = board.bitboards_[WHITE] | board.bitboards_[BLACK];
    U64 friendly = board.bitboards_[side];
    U8 king_sq = bit_scan_forward(kings);

    U64 checkers = BB_EMPTY;
    U64 pinners = BB_EMPTY;
    U64 pinned = BB_EMPTY;

    // Pawns and Knights can only be checkers, not pinners
    // Knights
    U64 knights = board.bitboards_[KNIGHT | attacker_side];
    checkers |= KNIGHT_LOOKUP_TABLE[king_sq] & knights;

    // Pawns
    U64 pawns = board.bitboards_[PAWN | attacker_side];
    for (int dir = 0; dir < 2; dir++)
    {
        int diff = diffs[dir][side];
        U64 targets = circular_left_shift(kings, diff) & pawn_file_mask[dir];
        checkers |= targets & pawns;
    }

    // Sliding pieces can be checkers or pinners depending on occupancy of intermediate squares
    U64 queens = board.bitboards_[QUEEN | attacker_side];
    U64 rooks = board.bitboards_[ROOK | attacker_side];
    U64 bishops = board.bitboards_[BISHOP | attacker_side];

    U64 diag_attackers = queens | bishops;
    U64 non_diag_attackers = queens | rooks;

    U64 potential_king_attackers = BB_EMPTY;
    potential_king_attackers |= rook_mask_ex(king_sq) & non_diag_attackers;
    potential_king_attackers |= bishop_mask_ex(king_sq) & diag_attackers;
    potential_king_attackers &= occupied;

    while (potential_king_attackers)
    {
        U8 from = bit_scan_forward(potential_king_attackers);
        U64 potentially_pinned = squares_between(from, king_sq) & occupied;

        // If there are no pieces between the attacker and the king
        // then the attacker is giving check
        if (potentially_pinned == BB_EMPTY)
        {
            checkers |= 1ULL << (from);
        }
        // If there is a friendly piece between the attacker and the king
        // then it is pinned
        else if ((pop_count(potentially_pinned) == 1) && (potentially_pinned & friendly))
        {
            pinned |= potentially_pinned & friendly;
            pinners |= 1ULL << (from);
        }
        potential_king_attackers &= potential_king_attackers - 1;
    }

    mgp.checkers = checkers;
    mgp.pinned = pinned;
    mgp.pinners = pinners;

    return mgp;
}

/// returns squares king may not move to
/// - removes king from occupied to handle attacking sliders correctly
U64 MoveGenerator::get_king_danger_squares(const class Board& board, const U8 side, U64 king)
{
    U8 attacker_side = !side;
    U64 occupied_without_king = (board.bitboards_[WHITE] | board.bitboards_[BLACK]) & (~king);

    U64 attacked_squares = BB_EMPTY;

    U64 queens = board.bitboards_[QUEEN | attacker_side];
    U64 rooks = board.bitboards_[ROOK | attacker_side];
    U64 bishops = board.bitboards_[BISHOP | attacker_side];

    U64 diag_attackers = queens | bishops;
    U64 non_diag_attackers = queens | rooks;
    attacked_squares |= bishop_targets(diag_attackers, occupied_without_king);
    attacked_squares |= rook_targets(non_diag_attackers, occupied_without_king);

    U64 knights = board.bitboards_[KNIGHT | attacker_side];
    attacked_squares |= knight_targets(knights);

    U64 kings = board.bitboards_[KING | attacker_side];
#ifndef NDEBUG
    assert(pop_count(kings) == 1);
#endif
    attacked_squares |= king_targets(kings);

    U64 pawns = board.bitboards_[PAWN | attacker_side];
    attacked_squares |= pawn_targets(pawns, attacker_side);

    return attacked_squares;
}

#define INDENT "    "
void MoveGenerator::generate_move_lookup_tables()
{
    cout << "// LOOKUP TABLE FOR KNIGHT MOVES" << endl;
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
    cout << "};" << endl << endl;

    cout << "// LOOKUP TABLE FOR KING MOVES" << endl;
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
    cout << "};" << endl << endl;

    cout << "// LOOKUP TABLES FOR SLIDERS" << endl;
    cout << "const U64 RANK_MASK[64] = {" << endl << INDENT;
    for (int row = 0; row < 8; row++)
    {
        for (int file = 0; file < 8; file++)
        {
            int square = row * 8 + file;
            U64 targets = rank_mask_calc(square);
            printf("0x%016llXULL", targets);
            cout << ", ";
            if (file == 3)
                cout << endl << INDENT;
        }
        cout << endl << INDENT;
    }
    cout << "};" << endl << endl;
    cout << "const U64 FILE_MASK[64] = {" << endl << INDENT;
    for (int row = 0; row < 8; row++)
    {
        for (int file = 0; file < 8; file++)
        {
            int square = row * 8 + file;
            U64 targets = file_mask_calc(square);
            printf("0x%016llXULL", targets);
            cout << ", ";
            if (file == 3)
                cout << endl << INDENT;
        }
        cout << endl << INDENT;
    }
    cout << "};" << endl << endl;
    cout << "const U64 DIAG_MASK[64] = {" << endl << INDENT;
    for (int row = 0; row < 8; row++)
    {
        for (int file = 0; file < 8; file++)
        {
            int square = row * 8 + file;
            U64 targets = diag_mask_calc(square);
            printf("0x%016llXULL", targets);
            cout << ", ";
            if (file == 3)
                cout << endl << INDENT;
        }
        cout << endl << INDENT;
    }
    cout << "};" << endl << endl;
    cout << "const U64 ANTI_DIAG_MASK[64] = {" << endl << INDENT;
    for (int row = 0; row < 8; row++)
    {
        for (int file = 0; file < 8; file++)
        {
            int square = row * 8 + file;
            U64 targets = anti_diag_mask_calc(square);
            printf("0x%016llXULL", targets);
            cout << ", ";
            if (file == 3)
                cout << endl << INDENT;
        }
        cout << endl << INDENT;
    }
    cout << "};" << endl << endl;
    cout << "const U64 ROOK_MASK[64] = {" << endl << INDENT;
    for (int row = 0; row < 8; row++)
    {
        for (int file = 0; file < 8; file++)
        {
            int square = row * 8 + file;
            U64 targets = rook_mask_calc(square);
            printf("0x%016llXULL", targets);
            cout << ", ";
            if (file == 3)
                cout << endl << INDENT;
        }
        cout << endl << INDENT;
    }
    cout << "};" << endl << endl;
    cout << "const U64 BISHOP_MASK[64] = {" << endl << INDENT;
    for (int row = 0; row < 8; row++)
    {
        for (int file = 0; file < 8; file++)
        {
            int square = row * 8 + file;
            U64 targets = bishop_mask_calc(square);
            printf("0x%016llXULL", targets);
            cout << ", ";
            if (file == 3)
                cout << endl << INDENT;
        }
        cout << endl << INDENT;
    }
    cout << "};" << endl << endl;

    cout << "const U64 RANK_MASK_EX[64] = {" << endl << INDENT;
    for (int row = 0; row < 8; row++)
    {
        for (int file = 0; file < 8; file++)
        {
            int square = row * 8 + file;
            U64 targets = rank_mask_ex_calc(square);
            printf("0x%016llXULL", targets);
            cout << ", ";
            if (file == 3)
                cout << endl << INDENT;
        }
        cout << endl << INDENT;
    }
    cout << "};" << endl << endl;
    cout << "const U64 FILE_MASK_EX[64] = {" << endl << INDENT;
    for (int row = 0; row < 8; row++)
    {
        for (int file = 0; file < 8; file++)
        {
            int square = row * 8 + file;
            U64 targets = file_mask_ex_calc(square);
            printf("0x%016llXULL", targets);
            cout << ", ";
            if (file == 3)
                cout << endl << INDENT;
        }
        cout << endl << INDENT;
    }
    cout << "};" << endl << endl;
    cout << "const U64 DIAG_MASK_EX[64] = {" << endl << INDENT;
    for (int row = 0; row < 8; row++)
    {
        for (int file = 0; file < 8; file++)
        {
            int square = row * 8 + file;
            U64 targets = diag_mask_ex_calc(square);
            printf("0x%016llXULL", targets);
            cout << ", ";
            if (file == 3)
                cout << endl << INDENT;
        }
        cout << endl << INDENT;
    }
    cout << "};" << endl << endl;
    cout << "const U64 ANTI_DIAG_MASK_EX[64] = {" << endl << INDENT;
    for (int row = 0; row < 8; row++)
    {
        for (int file = 0; file < 8; file++)
        {
            int square = row * 8 + file;
            U64 targets = anti_diag_mask_ex_calc(square);
            printf("0x%016llXULL", targets);
            cout << ", ";
            if (file == 3)
                cout << endl << INDENT;
        }
        cout << endl << INDENT;
    }
    cout << "};" << endl << endl;
    cout << "const U64 ROOK_MASK_EX[64] = {" << endl << INDENT;
    for (int row = 0; row < 8; row++)
    {
        for (int file = 0; file < 8; file++)
        {
            int square = row * 8 + file;
            U64 targets = rook_mask_ex_calc(square);
            printf("0x%016llXULL", targets);
            cout << ", ";
            if (file == 3)
                cout << endl << INDENT;
        }
        cout << endl << INDENT;
    }
    cout << "};" << endl << endl;
    cout << "const U64 BISHOP_MASK_EX[64] = {" << endl << INDENT;
    for (int row = 0; row < 8; row++)
    {
        for (int file = 0; file < 8; file++)
        {
            int square = row * 8 + file;
            U64 targets = bishop_mask_ex_calc(square);
            printf("0x%016llXULL", targets);
            cout << ", ";
            if (file == 3)
                cout << endl << INDENT;
        }
        cout << endl << INDENT;
    }
    cout << "};" << endl << endl;

    cout << "// LOOKUP TABLE FOR SQUARES BETWEEN" << endl;
    cout << "const U64 SQUARES_BETWEEN[64][64] = {" << endl << INDENT;
    for (U8 from = 0; from < 64; from++)
    {
        cout << "{ ";
        for (U8 to = 0; to < 64; to++)
        {
            if (to % 4 == 0)
                cout << endl << INDENT << INDENT;
            U64 between = squares_between_calc(from, to);
            printf("0x%016llXULL, ", between);
        }
        cout << endl << INDENT << INDENT;
        cout << "},";
        cout << endl << INDENT;
    }
    cout << "};" << endl << endl;

    cout << "// LOOKUP TABLE FOR LINES ALONG" << endl;
    cout << "const U64 LINES_ALONG[64][64] = {" << endl << INDENT;
    for (U8 from = 0; from < 64; from++)
    {
        cout << "{ ";
        for (U8 to = 0; to < 64; to++)
        {
            if (to % 4 == 0)
                cout << endl << INDENT << INDENT;
            U64 lines = lines_along_calc(from, to);
            printf("0x%016llXULL, ", lines);
        }
        cout << endl << INDENT << INDENT;
        cout << "},";
        cout << endl << INDENT;
    }
    cout << "};" << endl << endl;

    cout << "// LOOKUP TABLES FOR MAGIC BITBOARDS" << endl;
    cout << "const U64 ROOK_MB_MASK[64] = {" << endl << INDENT;
    for (int row = 0; row < 8; row++)
    {
        for (int file = 0; file < 8; file++)
        {
            int square = row * 8 + file;
            U64 targets = rank_mask_ex_calc(square) & ~FILE_A & ~FILE_H;
            targets |= file_mask_ex_calc(square) & ~ROW_1 & ~ROW_8;
            printf("0x%016llXULL", targets);
            cout << ", ";
            if (file == 3)
                cout << endl << INDENT;
        }
        cout << endl << INDENT;
    }
    cout << "};" << endl << endl;

    cout << "const U64 BISHOP_MB_MASK[64] = {" << endl << INDENT;
    for (int row = 0; row < 8; row++)
    {
        for (int file = 0; file < 8; file++)
        {
            int square = row * 8 + file;
            U64 edge_squares = FILE_A | FILE_H | ROW_1 | ROW_8;
            U64 targets = bishop_mask_ex_calc(square) & ~FILE_A & ~edge_squares;
            printf("0x%016llXULL", targets);
            cout << ", ";
            if (file == 3)
                cout << endl << INDENT;
        }
        cout << endl << INDENT;
    }
    cout << "};" << endl << endl;
}
