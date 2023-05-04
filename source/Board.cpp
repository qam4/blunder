/*
 * File:   Board.cpp
 *
 */

#include "Board.h"

#include "MoveGenerator.h"
#include "MoveList.h"
#include "PrincipalVariation.h"
// #include "Zobrist.h"

// Count number of bits set to 1 in 64 bit word
int pop_count(U64 x)
{
    int count = 0;
    while (x)
    {
        count++;
        x &= x - 1;  // reset LS1B
    }
    return count;
}

Board::Board()
{
    reset();
}

bool Board::is_blank()
{
    return !(bitboards[WHITE] | bitboards[BLACK]);
}

void Board::add_piece(U8 piece, int square)
{
#ifndef NDEBUG
    assert(is_valid_piece(piece));
    assert(is_valid_square(square));
    assert(board_array[square] == EMPTY);
#endif
    board_array[square] = piece;
    U64 bitboard = 1ULL << square;
    bitboards[piece & 1] |= bitboard;
    bitboards[piece] |= bitboard;
}

void Board::remove_piece(int square)
{
#ifndef NDEBUG
    assert(is_valid_square(square));
    assert(board_array[square] != EMPTY);
#endif
    U8 piece = board_array[square];
    board_array[square] = EMPTY;
    U64 bitboard = ~(1ULL << square);
    bitboards[piece & 1] &= bitboard;
    bitboards[piece] &= bitboard;
}

void Board::reset()
{
    zobrist = Zobrist();
    for (int i = 0; i < 64; i++)
    {
        board_array[i] = EMPTY;
    }
    for (int i = 0; i < 14; i++)
    {
        bitboards[i] = 0ULL;
    }
    irrev.half_move_count = 0;
    irrev.castling_rights = FULL_CASTLING_RIGHTS;
    irrev.ep_square = NULL_SQUARE;
    irrev.side_to_move = WHITE;
    game_ply = 0;
}

U8 Board::operator[](const int square) const
{
#ifndef NDEBUG
    assert(is_valid_square(square));
#endif
    return board_array[square];
}

U64 Board::bitboard(const int type) const
{
#ifndef NDEBUG
    assert(type >= 0 && type <= BLACK_QUEEN);
#endif
    return bitboards[type];
}

// [fm] TODO: gonna need a stack for irrev
// handle special moves, caste, promo, ep
// with promotion, there should be no pawn on row 1 or 8
void Board::do_move(Move_t move)
{
    U8 from = move_from(move);
    U8 to = move_to(move);
    U8 piece = board_array[from];
    // cout << "do_move:" << Output::move(move, *this) << endl;
    // cout << "do_move: move_flag=" << hex << move << endl;

    irrev.ep_square = NULL_SQUARE;
    if (is_pawn_double_push(move))
    {
        irrev.ep_square = (to + from) >> 1;
    }
    else if (is_ep_capture(move))
    {
            // TODO
    }
    else if (is_capture(move))
    {
        remove_piece(to);
    }
    remove_piece(from);
    add_piece(piece, to);

    // update flags
    irrev.half_move_count++;

    // update side_to_move
    irrev.side_to_move ^= 1;

    game_ply++;
    search_ply++;
    max_search_ply = max(max_search_ply, search_ply);
}

void Board::undo_move(Move_t move)
{
    U8 from = move_from(move);
    U8 to = move_to(move);
    U8 piece = board_array[to];
    // cout << "undo_move:" << Output::move(move, *this) << endl;
    // cout << "undo_move: move_flag=" << hex << move << endl;

    // update side_to_move
    irrev.side_to_move ^= 1;

    remove_piece(to);
    add_piece(piece, from);
    if (is_capture(move))
    {
        add_piece(move_captured(move), to);
    }

    // update flags
    irrev.half_move_count--;
    irrev.ep_square = NULL_SQUARE;

    game_ply--;
    search_ply--;
}

int Board::evaluate()
{
    /*
    https://www.chessprogramming.org/Evaluation
    f(p) = 200(K-K')
           + 9(Q-Q')
           + 5(R-R')
           + 3(B-B' + N-N')
           + 1(P-P')
           - 0.5(D-D' + S-S' + I-I')
           + 0.1(M-M') + ...

    KQRBNP = number of kings, queens, rooks, bishops, knights and pawns
    D,S,I = doubled, blocked and isolated pawns
    M = Mobility (the number of legal moves)
    */
    int result = 0;
    result = 200 * (pop_count(bitboards[WHITE_KING]) - pop_count(bitboards[BLACK_KING]))
        + 9 * (pop_count(bitboards[WHITE_QUEEN]) - pop_count(bitboards[BLACK_QUEEN]))
        + 5 * (pop_count(bitboards[WHITE_ROOK]) - pop_count(bitboards[BLACK_ROOK]))
        + 3 * (pop_count(bitboards[WHITE_BISHOP]) - pop_count(bitboards[BLACK_BISHOP]))
        + 3 * (pop_count(bitboards[WHITE_KNIGHT]) - pop_count(bitboards[BLACK_KNIGHT]))
        + 1 * (pop_count(bitboards[WHITE_PAWN]) - pop_count(bitboards[BLACK_PAWN]));

    // cout << "evaluate=" << result << endl;
    return result;
}

// is_game_over(): return 1 if game is over.
int Board::is_game_over()
{
    // Game over if:
    // - no legal moves
    // TODO
    MoveList list;
    MoveGenerator::add_all_moves(list, *this, side_to_move());
    if (list.length() == 0)
        return 1;

    return 0;
}

Move_t Board::search(int depth)
{
    search_start_time = clock();
    search_ply = 0;
    reset_hash_table();
    reset_pv_table();

    // Iterative deepening
    Move_t last_best_move = 0;
    for (int current_depth = 1; current_depth <= depth; current_depth++)
    {
        follow_pv = 1;
        max_search_ply = 0;
        searched_moves = 0;

        alphabeta(-MAX_SCORE, MAX_SCORE, current_depth);
        search_best_move = pv_table[0];
        cout << "depth=" << current_depth;
        cout << ", search ply=" << max_search_ply;
        cout << ", searched moves=" << searched_moves;
        cout << ", pv=";
        print_pv();
        cout << endl;
        if (is_search_time_over())
        {
            break;
        }
        last_best_move = search_best_move;
    }

    return last_best_move;
}

bool Board::is_search_time_over()
{
    clock_t current_time = clock();
    clock_t elapsed_secs = current_time - search_start_time;

    return (elapsed_secs > MAX_SEARCH_TIME);
}

U64 Board::get_hash()
{
    return zobrist.get_zobrist_key(*this);
}