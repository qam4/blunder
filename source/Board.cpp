/*
 * File:   Board.cpp
 *
 */

#include "Board.h"

#include "MoveGenerator.h"
#include "MoveList.h"
#include "PrincipalVariation.h"

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
        bitboards[i] = BB_EMPTY;
    }
    irrev.half_move_count = 0;
    irrev.castling_rights = FULL_CASTLING_RIGHTS;
    irrev.ep_square = NULL_SQUARE;
    irrev.side_to_move = WHITE;
    game_ply = 0;
    search_ply = 0;
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

// [fm] TODO:
// handle special moves: caste, promo, ep
void Board::do_move(Move_t move)
{
    U8 from = move_from(move);
    U8 to = move_to(move);
    U8 piece = board_array[from];
    // cout << "do_move:" << Output::move(move, *this) << endl;
    // cout << "do_move: move_flag=" << hex << move << endl;

    // Save irreversible state
    move_stack[search_ply] = irrev;

    irrev.ep_square = NULL_SQUARE;
    if (is_pawn_double_push(move))
    {
        irrev.ep_square = static_cast<U8>((to + from) >> 1);
    }

    if (is_castle(move))
    {
        if (move & build_castle(QUEEN_CASTLE))
        {
            if (irrev.side_to_move == WHITE)
            {
                // White queen-side castle
#ifndef NDEBUG
                assert(board_array[A1] == WHITE_ROOK);
                assert(board_array[E1] == WHITE_KING);
                assert(board_array[B1] == EMPTY);
                assert(board_array[C1] == EMPTY);
                assert(board_array[D1] == EMPTY);
#endif
                remove_piece(A1);
                remove_piece(E1);
                add_piece(WHITE_KING, C1);
                add_piece(WHITE_ROOK, D1);
                irrev.castling_rights &= static_cast<U8>(~(WHITE_QUEEN_SIDE | WHITE_KING_SIDE));
            }
            else
            {
                // Black queen-side castle
#ifndef NDEBUG
                assert(board_array[A8] == BLACK_ROOK);
                assert(board_array[E8] == BLACK_KING);
                assert(board_array[B8] == EMPTY);
                assert(board_array[C8] == EMPTY);
                assert(board_array[D8] == EMPTY);
#endif
                remove_piece(A8);
                remove_piece(E8);
                add_piece(BLACK_KING, C8);
                add_piece(BLACK_ROOK, D8);
                irrev.castling_rights &= static_cast<U8>(~(BLACK_QUEEN_SIDE | BLACK_KING_SIDE));
            }
        }
        else
        {
            if (irrev.side_to_move == WHITE)
            {
                // White king-side castle
#ifndef NDEBUG
                assert(board_array[H1] == WHITE_ROOK);
                assert(board_array[E1] == WHITE_KING);
                assert(board_array[G1] == EMPTY);
                assert(board_array[F1] == EMPTY);
#endif
                remove_piece(H1);
                remove_piece(E1);
                add_piece(WHITE_KING, G1);
                add_piece(WHITE_ROOK, F1);
                irrev.castling_rights &= static_cast<U8>(~(WHITE_QUEEN_SIDE | WHITE_KING_SIDE));
            }
            else
            {
                // Black king-side castle
#ifndef NDEBUG
                assert(board_array[H8] == BLACK_ROOK);
                assert(board_array[E8] == BLACK_KING);
                if (board_array[G8] != EMPTY)
                {
                    cout << "dbg\n" << Output::board(*this) << endl;
                    cout << "do_move:" << Output::move(move, *this) << endl;
                    cout << "do_move: move_flag=" << hex << move << endl;
                }
                assert(board_array[G8] == EMPTY);
                assert(board_array[F8] == EMPTY);
#endif
                remove_piece(H8);
                remove_piece(E8);
                add_piece(BLACK_KING, G8);
                add_piece(BLACK_ROOK, F8);
                irrev.castling_rights &= static_cast<U8>(~(BLACK_QUEEN_SIDE | BLACK_KING_SIDE));
            }
        }
    }
    else
    {
        remove_piece(from);

        if (is_capture(move))
        {
            U8 captured_sq = to;
            if (is_ep_capture(move))
            {
                // along_row_with_col
                // returns a square at the same row as "from", and the same col as "to"
                captured_sq = (from & 56) | (to & 7);
            }
            remove_piece(captured_sq);
        }

        if (is_promotion(move))
        {
            add_piece(move_promote_to(move), to);
        }
        else
        {
            add_piece(piece, to);
        }
    }

    // Update castling rights
    if ((board_array[A1] != WHITE_ROOK) || (board_array[E1] != WHITE_KING))
    {
        irrev.castling_rights &= static_cast<U8>(~(WHITE_QUEEN_SIDE));
    }
    if ((board_array[H1] != WHITE_ROOK) || (board_array[E1] != WHITE_KING))
    {
        irrev.castling_rights &= static_cast<U8>(~(WHITE_KING_SIDE));
    }
    if ((board_array[A8] != BLACK_ROOK) || (board_array[E8] != BLACK_KING))
    {
        irrev.castling_rights &= static_cast<U8>(~(BLACK_QUEEN_SIDE));
    }
    if ((board_array[H8] != BLACK_ROOK) || (board_array[E8] != BLACK_KING))
    {
        irrev.castling_rights &= static_cast<U8>(~(BLACK_KING_SIDE));
    }

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

    game_ply--;
    search_ply--;

    // update irreversible state
    irrev = move_stack[search_ply];

    if (is_castle(move))
    {
        if (move & build_castle(QUEEN_CASTLE))
        {
            if (irrev.side_to_move == WHITE)
            {
#ifndef NDEBUG
                assert(board_array[C1] == WHITE_KING);
                assert(board_array[D1] == WHITE_ROOK);
                assert(board_array[E1] == EMPTY);
                assert(board_array[A1] == EMPTY);
#endif
                remove_piece(C1);
                remove_piece(D1);
                add_piece(WHITE_KING, E1);
                add_piece(WHITE_ROOK, A1);
            }
            else
            {
#ifndef NDEBUG
                assert(board_array[C8] == BLACK_KING);
                assert(board_array[D8] == BLACK_ROOK);
                assert(board_array[E8] == EMPTY);
                assert(board_array[A8] == EMPTY);
#endif
                remove_piece(C8);
                remove_piece(D8);
                add_piece(BLACK_KING, E8);
                add_piece(BLACK_ROOK, A8);
            }
        }
        else
        {
            if (irrev.side_to_move == WHITE)
            {
#ifndef NDEBUG
                assert(board_array[G1] == WHITE_KING);
                assert(board_array[F1] == WHITE_ROOK);
                assert(board_array[E1] == EMPTY);
                assert(board_array[H1] == EMPTY);
#endif
                remove_piece(G1);
                remove_piece(F1);
                add_piece(WHITE_KING, E1);
                add_piece(WHITE_ROOK, H1);
            }
            else
            {
#ifndef NDEBUG
                assert(board_array[G8] == BLACK_KING);
                assert(board_array[F8] == BLACK_ROOK);
                assert(board_array[E8] == EMPTY);
                assert(board_array[H8] == EMPTY);
#endif
                remove_piece(G8);
                remove_piece(F8);
                add_piece(BLACK_KING, E8);
                add_piece(BLACK_ROOK, H8);
            }
        }
    }
    else
    {
        remove_piece(to);

        if (is_promotion(move))
        {
            add_piece(PAWN | irrev.side_to_move, from);
        }
        else
        {
            add_piece(piece, from);
        }

        if (is_capture(move))
        {
            U8 captured_sq = to;
            if (is_ep_capture(move))
            {
                // along_row_with_col
                // returns a square at the same row as "from", and the same col as "to"
                captured_sq = (from & 56) | (to & 7);
            }
            add_piece(move_captured(move), captured_sq);
        }
    }
}

// clang-format off
const int PIECE_SQUARE[NUM_PIECES / 2][64] = {
    {
        0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0  // a1-h1
    },
    // pawn
    {
        0,  0,  0,  0,  0,  0,  0,  0,
        50, 50, 50, 50, 50, 50, 50, 50,
        10, 10, 20, 30, 30, 20, 10, 10,
        5,  5, 10, 25, 25, 10,  5,  5,
        0,  0,  0, 20, 20,  0,  0,  0,
        5, -5,-10,  0,  0,-10, -5,  5,
        5, 10, 10,-20,-20, 10, 10,  5,
        0,  0,  0,  0,  0,  0,  0,  0
    },
    // knight
    {
        -50,-40,-30,-30,-30,-30,-40,-50,
        -40,-20,  0,  0,  0,  0,-20,-40,
        -30,  0, 10, 15, 15, 10,  0,-30,
        -30,  5, 15, 20, 20, 15,  5,-30,
        -30,  0, 15, 20, 20, 15,  0,-30,
        -30,  5, 10, 15, 15, 10,  5,-30,
        -40,-20,  0,  5,  5,  0,-20,-40,
        -50,-40,-30,-30,-30,-30,-40,-50
    },
    // bishop
    {
        -20,-10,-10,-10,-10,-10,-10,-20,
        -10,  0,  0,  0,  0,  0,  0,-10,
        -10,  0,  5, 10, 10,  5,  0,-10,
        -10,  5,  5, 10, 10,  5,  5,-10,
        -10,  0, 10, 10, 10, 10,  0,-10,
        -10, 10, 10, 10, 10, 10, 10,-10,
        -10,  5,  0,  0,  0,  0,  5,-10,
        -20,-10,-10,-10,-10,-10,-10,-20
    },
    // rook
    {
         0,  0,  0,  0,  0,  0,  0,  0,
         5, 10, 10, 10, 10, 10, 10,  5,
        -5,  0,  0,  0,  0,  0,  0, -5,
        -5,  0,  0,  0,  0,  0,  0, -5,
        -5,  0,  0,  0,  0,  0,  0, -5,
        -5,  0,  0,  0,  0,  0,  0, -5,
        -5,  0,  0,  0,  0,  0,  0, -5,
         0,  0,  0,  5,  5,  0,  0,  0
    },
    // queen
    {
        -20,-10,-10, -5, -5,-10,-10,-20,
        -10,  0,  0,  0,  0,  0,  0,-10,
        -10,  0,  5,  5,  5,  5,  0,-10,
         -5,  0,  5,  5,  5,  5,  0, -5,
          0,  0,  5,  5,  5,  5,  0, -5,
        -10,  5,  5,  5,  5,  5,  0,-10,
        -10,  0,  5,  0,  0,  0,  0,-10,
        -20,-10,-10, -5, -5,-10,-10,-20
    },
    // king
    {
        -50,-40,-30,-20,-20,-30,-40,-50,
        -30,-20,-10,  0,  0,-10,-20,-30,
        -30,-10, 20, 30, 30, 20,-10,-30,
        -30,-10, 30, 40, 40, 30,-10,-30,
        -30,-10, 30, 40, 40, 30,-10,-30,
        -30,-10, 20, 30, 30, 20,-10,-30,
        -30,-30,  0,  0,  0,  0,-30,-30,
        -50,-30,-30,-30,-30,-30,-30,-50
    }
};
// clang-format on

int Board::evaluate()
{
    /*
    https://www.chessprogramming.org/Evaluation
    https://www.chessprogramming.org/Simplified_Evaluation_Function
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
    result = 20000 * (pop_count(bitboards[WHITE_KING]) - pop_count(bitboards[BLACK_KING]))
        + 900 * (pop_count(bitboards[WHITE_QUEEN]) - pop_count(bitboards[BLACK_QUEEN]))
        + 500 * (pop_count(bitboards[WHITE_ROOK]) - pop_count(bitboards[BLACK_ROOK]))
        + 330 * (pop_count(bitboards[WHITE_BISHOP]) - pop_count(bitboards[BLACK_BISHOP]))
        + 320 * (pop_count(bitboards[WHITE_KNIGHT]) - pop_count(bitboards[BLACK_KNIGHT]))
        + 100 * (pop_count(bitboards[WHITE_PAWN]) - pop_count(bitboards[BLACK_PAWN]));

    for (int square = 0; square < 64; square++)
    {
        U8 piece = board_array[square];
        if (piece & BLACK)
        {
            result -= PIECE_SQUARE[piece >> 1][square];
        }
        else
        {
            result += PIECE_SQUARE[piece >> 1][square ^ 56];  // vertical flipping
        }
    }

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

        int value = alphabeta(-MAX_SCORE, MAX_SCORE, current_depth);
        search_best_move = pv_table[0];
        cout << "depth=" << current_depth;
        cout << ", search ply=" << max_search_ply;
        cout << ", searched moves=" << searched_moves;
        cout << ", score=" << value;
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