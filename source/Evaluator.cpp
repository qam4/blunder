/*
 * File:   Evaluator.cpp
 *
 * HandCraftedEvaluator implementation.
 * Stockfish-style tapered evaluation with dual-phase PSQT.
 */

#include <algorithm>

#include "Evaluator.h"

#include "Board.h"
#include "MoveGenerator.h"
#include "MoveList.h"

// Phase constants
static constexpr int MIDGAME_LIMIT = 15258;
static constexpr int ENDGAME_LIMIT = 3915;
static constexpr int PHASE_MAX = 128;
static constexpr int MG = 0;
static constexpr int EG = 1;
static constexpr int NUM_PHASES = 2;

// Material values: PIECE_VALUE_BONUS[phase][piece_type >> 1]
// Index: 0=empty, 1=Pawn, 2=Knight, 3=Bishop, 4=Rook, 5=Queen, 6=King
static const int PIECE_VALUE_BONUS[NUM_PHASES][NUM_PIECES / 2] = {
    // MG: empty, pawn, knight, bishop, rook, queen, king
    { 0, 124, 781, 825, 1276, 2538, 0 },
    // EG
    { 0, 206, 854, 915, 1380, 2682, 0 }
};

// clang-format off

// Pawn PSQT: full 64-square layout per phase
static const int PIECE_SQUARE_BONUS_PAWN[NUM_PHASES][NUM_SQUARES] =
{
    {
        0, 0, 0, 0, 0, 0, 0, 0,
        3, 3, 10, 19, 16, 19, 7, -5,
        -9, -15, 11, 15, 32, 22, 5, -22,
        -4, -23, 6, 20, 40, 17, 4, -8,
        13, 0, -13, 1, 11, -2, -13, 5,
        5, -12, -7, 22, -8, -5, -15, -8,
        -7, 7, -3, -13, 5, -16, 10, -8,
        0, 0, 0, 0, 0, 0, 0, 0
    },
    {
        0, 0, 0, 0, 0, 0, 0, 0,
        -10, -6, 10, 0, 14, 7, -5, -19,
        -10, -10, -10, 4, 4, 3, -6, -4,
        6, -2, -8, -4, -13, -12, -10, -9,
        10, 5, 4, -5, -5, -5, 14, 9,
        28, 20, 21, 28, 30, 7, 6, 13,
        0, -11, 12, 21, 25, 19, 4, 7,
        0, 0, 0, 0, 0, 0, 0, 0
    }
};

// Compressed file-symmetric PSQT: PIECE_SQUARE_BONUS[phase][piece_index][rank][file_half]
// piece_index: 0=Knight, 1=Bishop, 2=Rook, 3=Queen, 4=King
static const int PIECE_SQUARE_BONUS[NUM_PHASES][5][8][4] = {
{
    {{-175, -92, -74, -73}, {-77, -41, -27, -15}, {-61, -17, 6, 12}, {-35, 8, 40, 49}, {-34, 13, 44, 51}, {-9, 22, 58, 53}, {-67, -27, 4, 37}, {-201, -83, -56, -26}},
    {{-53, -5, -8, -23}, {-15, 8, 19, 4}, {-7, 21, -5, 17}, {-5, 11, 25, 39}, {-12, 29, 22, 31}, {-16, 6, 1, 11}, {-17, -14, 5, 0}, {-48, 1, -14, -23}},
    {{-31, -20, -14, -5}, {-21, -13, -8, 6}, {-25, -11, -1, 3}, {-13, -5, -4, -6}, {-27, -15, -4, 3}, {-22, -2, 6, 12}, {-2, 12, 16, 18}, {-17, -19, -1, 9}},
    {{3, -5, -5, 4}, {-3, 5, 8, 12}, {-3, 6, 13, 7}, {4, 5, 9, 8}, {0, 14, 12, 5}, {-4, 10, 6, 8}, {-5, 6, 10, 8}, {-2, -2, 1, -2}},
    {{271, 327, 271, 198}, {278, 303, 234, 179}, {195, 258, 169, 120}, {164, 190, 138, 98}, {154, 179, 105, 70}, {123, 145, 81, 31}, {88, 120, 65, 33}, {59, 89, 45, -1}}
},
{
    {{-96, -65, -49, -21}, {-67, -54, -18, 8}, {-40, -27, -8, 29}, {-35, -2, 13, 28}, {-45, -16, 9, 39}, {-51, -44, -16, 17}, {-69, -50, -51, 12}, {-100, -88, -56, -17}},
    {{-57, -30, -37, -12}, {-37, -13, -17, 1}, {-16, -1, -2, 10}, {-20, -6, 0, 17}, {-17, -1, -14, 15}, {-30, 6, 4, 6}, {-31, -20, -1, 1}, {-46, -42, -37, -24}},
    {{-9, -13, -10, -9}, {-12, -9, -1, -2}, {6, -8, -2, -6}, {-6, 1, -9, 7}, {-5, 8, 7, -6}, {6, 1, -7, 10}, {4, 5, 20, -5}, {18, 0, 19, 13}},
    {{-69, -57, -47, -26}, {-55, -31, -22, -4}, {-39, -18, -9, 3}, {-23, -3, 13, 24}, {-29, -6, 9, 21}, {-38, -18, -12, 1}, {-50, -27, -24, -8}, {-75, -52, -43, -36}},
    {{1, 45, 85, 76}, {53, 100, 133, 135}, {88, 130, 169, 175}, {103, 156, 172, 172}, {96, 166, 199, 199}, {92, 172, 184, 191}, {47, 121, 116, 131}, {11, 59, 73, 78}}
}
};

// clang-format on

HandCraftedEvaluator::HandCraftedEvaluator()
    : piece_square_table_ {}
    , config_ {}
{
    psqt_init();
}

void HandCraftedEvaluator::psqt_init()
{
    for (int phase = 0; phase < NUM_PHASES; phase++)
    {
        for (int piece = 0; piece < NUM_PIECES; piece += 2)
        {
            for (int square = 0; square < NUM_SQUARES; square++)
            {
                int bonus = 0;
                if (piece == 0)
                {
                    // Empty square — leave as 0
                }
                else if (piece == PAWN)
                {
                    bonus = PIECE_SQUARE_BONUS_PAWN[phase][square];
                }
                else
                {
                    int file = std::min(square % 8, 7 - (square % 8));  // file symmetric
                    bonus = PIECE_SQUARE_BONUS[phase][(piece >> 1) - 2][square / 8][file];
                }

                int material = PIECE_VALUE_BONUS[phase][piece >> 1];
                piece_square_table_[phase][piece][square] = material + bonus;

                // Black piece: flip rank
                piece_square_table_[phase][piece | BLACK][square ^ 56] = material + bonus;
            }
        }
    }
}

int HandCraftedEvaluator::phase(const Board& board) const
{
    int npm = PIECE_VALUE_BONUS[MG][QUEEN >> 1] * pop_count(board.bitboard(WHITE_QUEEN))
        + PIECE_VALUE_BONUS[MG][ROOK >> 1] * pop_count(board.bitboard(WHITE_ROOK))
        + PIECE_VALUE_BONUS[MG][BISHOP >> 1] * pop_count(board.bitboard(WHITE_BISHOP))
        + PIECE_VALUE_BONUS[MG][KNIGHT >> 1] * pop_count(board.bitboard(WHITE_KNIGHT))
        + PIECE_VALUE_BONUS[MG][QUEEN >> 1] * pop_count(board.bitboard(BLACK_QUEEN))
        + PIECE_VALUE_BONUS[MG][ROOK >> 1] * pop_count(board.bitboard(BLACK_ROOK))
        + PIECE_VALUE_BONUS[MG][BISHOP >> 1] * pop_count(board.bitboard(BLACK_BISHOP))
        + PIECE_VALUE_BONUS[MG][KNIGHT >> 1] * pop_count(board.bitboard(BLACK_KNIGHT));

    npm = std::max(ENDGAME_LIMIT, std::min(npm, MIDGAME_LIMIT));
    return ((npm - ENDGAME_LIMIT) * PHASE_MAX) / (MIDGAME_LIMIT - ENDGAME_LIMIT);
}

int HandCraftedEvaluator::evaluate(const Board& board)
{
    int mg = 0;
    int eg = 0;

    for (int square = 0; square < NUM_SQUARES; square++)
    {
        U8 piece = board[square];
        if (piece == EMPTY)
            continue;

        if (piece & BLACK)
        {
            mg -= piece_square_table_[MG][piece][square];
            eg -= piece_square_table_[EG][piece][square];
        }
        else
        {
            mg += piece_square_table_[MG][piece][square];
            eg += piece_square_table_[EG][piece][square];
        }
    }

    int p = phase(board);
    int score = (mg * p + eg * (PHASE_MAX - p)) / PHASE_MAX;

    if (config_.mobility_enabled)
    {
        MoveList white_moves, black_moves;
        MoveGenerator::add_all_moves(white_moves, board, WHITE);
        MoveGenerator::add_all_moves(black_moves, board, BLACK);
        score += 20 * (white_moves.length() - black_moves.length());
    }

    if (config_.tempo_enabled)
    {
        score += (board.side_to_move() == WHITE) ? 28 : -28;
    }

    return score;
}

int HandCraftedEvaluator::side_relative_eval(const Board& board)
{
    int who2move = (board.side_to_move() == WHITE) ? 1 : -1;
    return who2move * evaluate(board);
}
