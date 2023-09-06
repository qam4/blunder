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
    irrev.board_hash ^= zobrist.get_pieces(piece, square);
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
    irrev.board_hash ^= zobrist.get_pieces(piece, square);
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
    irrev.full_move_count = 1;
    irrev.castling_rights = FULL_CASTLING_RIGHTS;
    irrev.ep_square = NULL_SQUARE;
    irrev.side_to_move = WHITE;
    game_ply = 0;
    search_ply = 0;
    search_time = MAX_SEARCH_TIME;
    update_hash();
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

void Board::do_move(Move_t move)
{
    U8 from = move_from(move);
    U8 to = move_to(move);
    U8 piece = board_array[from];
    bool move_resets_half_move_clock = false;

    // cout << "do_move:" << Output::move(move, *this) << endl;
    // cout << "do_move: move_flag=" << hex << move << endl;

    // Save irreversible state
    move_stack[search_ply] = irrev;

    if (irrev.ep_square != NULL_SQUARE)
    {
        irrev.board_hash ^= zobrist.get_ep_square(irrev.ep_square);
    }
    irrev.ep_square = NULL_SQUARE;
    if (is_pawn_double_push(move))
    {
        irrev.ep_square = static_cast<U8>((to + from) >> 1);
        irrev.board_hash ^= zobrist.get_ep_square(irrev.ep_square);
    }

    if (is_castle(move))
    {
        if (move & build_castle(QUEEN_CASTLE))
        {
            if (irrev.side_to_move == WHITE)
            {
                // White queen-side castle
                remove_piece(A1);
                remove_piece(E1);
                add_piece(WHITE_KING, C1);
                add_piece(WHITE_ROOK, D1);
            }
            else
            {
                // Black queen-side castle
                remove_piece(A8);
                remove_piece(E8);
                add_piece(BLACK_KING, C8);
                add_piece(BLACK_ROOK, D8);
            }
        }
        else
        {
            if (irrev.side_to_move == WHITE)
            {
                // White king-side castle
                remove_piece(H1);
                remove_piece(E1);
                add_piece(WHITE_KING, G1);
                add_piece(WHITE_ROOK, F1);
            }
            else
            {
                // Black king-side castle
                remove_piece(H8);
                remove_piece(E8);
                add_piece(BLACK_KING, G8);
                add_piece(BLACK_ROOK, F8);
            }
        }
    }
    else
    {
        if ((piece & (~1)) == PAWN)
        {
            move_resets_half_move_clock = true;
        }

        remove_piece(from);

        if (is_capture(move))
        {
            // half move clock reset after all pawn moves and captures
            move_resets_half_move_clock = true;

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
    if (irrev.castling_rights)
    {
        irrev.board_hash ^= zobrist.get_castling_rights(irrev.castling_rights);
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
        irrev.board_hash ^= zobrist.get_castling_rights(irrev.castling_rights);
    }

    // update flags
    if (move_resets_half_move_clock)
    {
        irrev.half_move_count = 0;
    }
    else
    {
        irrev.half_move_count++;
    }
    if (irrev.side_to_move == BLACK)
    {
        irrev.full_move_count++;
    }

    // update side_to_move
    irrev.side_to_move ^= 1;
    irrev.board_hash ^= zobrist.get_side();

    game_ply++;
    search_ply++;
    max_search_ply = max(max_search_ply, search_ply);
    // update_hash();
#ifndef NDEBUG
    assert(game_ply < MAX_GAME_PLY);
    assert(irrev.board_hash == zobrist.get_zobrist_key(*this));
#endif
    hash_history[game_ply] = irrev.board_hash;
}

void Board::undo_move(Move_t move)
{
    U8 from = move_from(move);
    U8 to = move_to(move);
    U8 piece = board_array[to];
    U64 hash;
    // cout << "undo_move:" << Output::move(move, *this) << endl;
    // cout << "undo_move: move_flag=" << hex << move << endl;

    game_ply--;
    search_ply--;

    // update irreversible state
    irrev = move_stack[search_ply];
    hash = irrev.board_hash;

    if (is_castle(move))
    {
        if (move & build_castle(QUEEN_CASTLE))
        {
            if (irrev.side_to_move == WHITE)
            {
                remove_piece(C1);
                remove_piece(D1);
                add_piece(WHITE_KING, E1);
                add_piece(WHITE_ROOK, A1);
            }
            else
            {
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
                remove_piece(G1);
                remove_piece(F1);
                add_piece(WHITE_KING, E1);
                add_piece(WHITE_ROOK, H1);
            }
            else
            {
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
    irrev.board_hash = hash;
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
bool Board::is_game_over()
{
    // Game over if:
    // - no legal moves
    // - draw
    MoveList list;
    MoveGenerator::add_all_moves(list, *this, side_to_move());
    if (list.length() == 0)
        return true;

    return is_draw();
}

bool Board::is_draw()
{
    // fifty-move rule: https://www.chessprogramming.org/Fifty-move_Rule
    if (irrev.half_move_count >= 100)
    {
        return true;
    }

    // threefold repetition rule
    // https://en.wikipedia.org/wiki/Threefold_repetition
    // https://www.chessprogramming.org/Repetitions
    // Check if we see the current position more than once in the game history
    int repetition_count = 0;
    for (int i = 0; i < game_ply; i++)
    {
        if (irrev.board_hash == hash_history[i])
        {
            repetition_count++;
        }
    }
    return (repetition_count > 1) ? true : false;
}

Move_t Board::search(int depth, int searchtime /*=MAX_SEARCH_TIME*/, bool xboard /*=false*/)
{
    search_time = searchtime;
    search_start_time = clock();
    search_ply = 0;
    reset_hash_table();
    reset_pv_table();

    // Iterative deepening
    Move_t last_best_move = 0;
    total_searched_moves = 0;

    for (int current_depth = 1; current_depth <= depth; current_depth++)
    {
        follow_pv = 1;
        max_search_ply = 0;
        searched_moves = 0;

#if 1
        int value = alphabeta(-MAX_SCORE, MAX_SCORE, current_depth);
#    ifndef NDEBUG
        assert(value <= MAX_SCORE);
        assert(value >= -MAX_SCORE);
#    endif
        search_best_move = pv_table[0];
#else
        search_best_move = negamax_root(current_depth);
        int value = 0;
#endif
        total_searched_moves += searched_moves;
        if (xboard)
        {
            clock_t current_time = clock();
            int elapsed_csecs =
                int((100 * double(current_time - search_start_time)) / CLOCKS_PER_SEC);

            // DEPTH SCORE TIME NODES PV
            cout << current_depth << " ";   // search depth
            cout << value << " ";           // score in centi-Pawn
            cout << elapsed_csecs << " ";   // time searched in centi-seconds
            cout << searched_moves << " ";  // node searched
            print_pv();
            cout << endl;
        }
        else
        {
            cout << "depth=" << current_depth;
            cout << ", search ply=" << max_search_ply;
            cout << ", searched moves=" << searched_moves;
            cout << ", score=" << value;
            cout << ", pv=";
            print_pv();
            cout << endl;
        }
        if (is_search_time_over())
        {
            break;
        }
        last_best_move = search_best_move;
        search_best_score = value;
    }

    move_remove_score(&last_best_move);
    return last_best_move;
}

bool Board::is_search_time_over()
{
    // Special case to allow infinite search time
    if (search_time == -1)
    {
        return false;
    }

    clock_t current_time = clock();
    clock_t elapsed_time = current_time - search_start_time;

    return (elapsed_time > search_time);
}

void Board::update_hash()
{
#ifndef NDEBUG
    assert(game_ply < MAX_GAME_PLY);
#endif
    set_hash(zobrist.get_zobrist_key(*this));
    hash_history[game_ply] = irrev.board_hash;
}
