/*
 * File:   Evaluation.cpp
 *
 */

#include "Board.h"
#include "MoveGenerator.h"
#include "MoveList.h"
#include "PrincipalVariation.h"

// clang-format off
const int PIECE_VALUE_BONUS[NUM_PHASES][NUM_PIECES / 2] = {
    // 0, P,   N,   B,   R,    Q,    K
    {  0, 124, 781, 825, 1276, 2538, 0 },
    {  0, 206, 854, 915, 1380, 2682, 0 }
};
// clang-format on

int Board::material_diff(int phase)
{
    const int* piece_value_bonus = &PIECE_VALUE_BONUS[phase][0];

    int material_white = piece_value_bonus[QUEEN >> 1] * pop_count(bitboard(WHITE_QUEEN))
        + piece_value_bonus[ROOK >> 1] * pop_count(bitboard(WHITE_ROOK))
        + piece_value_bonus[BISHOP >> 1] * pop_count(bitboard(WHITE_BISHOP))
        + piece_value_bonus[KNIGHT >> 1] * pop_count(bitboard(WHITE_KNIGHT))
        + piece_value_bonus[PAWN >> 1] * pop_count(bitboard(WHITE_PAWN));
    int material_back = piece_value_bonus[QUEEN >> 1] * pop_count(bitboard(BLACK_QUEEN))
        + piece_value_bonus[ROOK >> 1] * pop_count(bitboard(BLACK_ROOK))
        + piece_value_bonus[BISHOP >> 1] * pop_count(bitboard(BLACK_BISHOP))
        + piece_value_bonus[KNIGHT >> 1] * pop_count(bitboard(BLACK_KNIGHT))
        + piece_value_bonus[PAWN >> 1] * pop_count(bitboard(BLACK_PAWN));

    return material_white - material_back;
}

int Board::non_pawn_material()
{
    int phase = MG;
    const int* piece_value_bonus = &PIECE_VALUE_BONUS[phase][0];

    int material = piece_value_bonus[QUEEN >> 1] * pop_count(bitboard(WHITE_QUEEN))
        + piece_value_bonus[ROOK >> 1] * pop_count(bitboard(WHITE_ROOK))
        + piece_value_bonus[BISHOP >> 1] * pop_count(bitboard(WHITE_BISHOP))
        + piece_value_bonus[KNIGHT >> 1] * pop_count(bitboard(WHITE_KNIGHT))
        + piece_value_bonus[QUEEN >> 1] * pop_count(bitboard(BLACK_QUEEN))
        + piece_value_bonus[ROOK >> 1] * pop_count(bitboard(BLACK_ROOK))
        + piece_value_bonus[BISHOP >> 1] * pop_count(bitboard(BLACK_BISHOP))
        + piece_value_bonus[KNIGHT >> 1] * pop_count(bitboard(BLACK_KNIGHT));

    return material;
}

// clang-format off
int PIECE_SQUARE_BONUS_PAWN[NUM_PHASES][NUM_SQUARES] =
{
    {
        0, 0, 0, 0, 0, 0, 0, 0,             // a1-h1
        3, 3, 10, 19, 16, 19, 7, -5,
        -9, -15, 11, 15, 32, 22, 5, -22,
        -4, -23, 6, 20, 40, 17, 4, -8,
        13, 0, -13, 1, 11, -2, -13, 5,
        5, -12, -7, 22, -8, -5, -15, -8,
        -7, 7, -3, -13, 5, -16, 10, -8,
        0, 0, 0, 0, 0, 0, 0, 0             // a8-h8
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

// Table is defined for files A..D and white side: it is
// symmetric for black side and second half of the files.
int PIECE_SQUARE_BONUS[NUM_PHASES][(NUM_PIECES / 2) - 2][8][4] = {
{
    {{-175, -92, -74, -73}, {-77, -41, -27, -15}, {-61, -17, 6, 12}, {-35, 8, 40, 49}, {-34, 13, 44, 51}, {-9, 22, 58, 53}, {-67, -27, 4, 37}, {-201, -83, -56, -26}},     // N
    {{-53, -5, -8, -23}, {-15, 8, 19, 4}, {-7, 21, -5, 17}, {-5, 11, 25, 39}, {-12, 29, 22, 31}, {-16, 6, 1, 11}, {-17, -14, 5, 0}, {-48, 1, -14, -23}},                   // B
    {{-31, -20, -14, -5}, {-21, -13, -8, 6}, {-25, -11, -1, 3}, {-13, -5, -4, -6}, {-27, -15, -4, 3}, {-22, -2, 6, 12}, {-2, 12, 16, 18}, {-17, -19, -1, 9}},              // R
    {{3, -5, -5, 4}, {-3, 5, 8, 12}, {-3, 6, 13, 7}, {4, 5, 9, 8}, {0, 14, 12, 5}, {-4, 10, 6, 8}, {-5, 6, 10, 8}, {-2, -2, 1, -2}},                                       // Q
    {{271, 327, 271, 198}, {278, 303, 234, 179}, {195, 258, 169, 120}, {164, 190, 138, 98}, {154, 179, 105, 70}, {123, 145, 81, 31}, {88, 120, 65, 33}, {59, 89, 45, -1}}  // K
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

void Board::psqt_init()
{
    int phase, piece, square;
    for (phase = 0; phase < NUM_PHASES; phase++)
    {
        for (piece = 0; piece < NUM_PIECES; piece += 2)
        {
            for (square = 0; square < NUM_SQUARES; square++)
            {
                if (piece == 0)
                {
                    piece_square_table_[phase][piece][square] = 0;
                }
                else if (piece == PAWN)
                {
                    piece_square_table_[phase][piece][square] =
                        PIECE_SQUARE_BONUS_PAWN[phase][square];
                }
                else
                {
                    int file = min(file_of(square), 7 - file_of(square));  // file symmetric
                    piece_square_table_[phase][piece][square] =
                        PIECE_SQUARE_BONUS[phase][(piece >> 1) - 2][rank_of(square)][file];
                }
                piece_square_table_[phase][piece | BLACK][square ^ 56] =
                    piece_square_table_[phase][piece][square];  // flip the rank for black pieces
            }
        }
    }
}

int Board::psqt(int phase)
{
    int v = 0;
    for (int square = 0; square < NUM_SQUARES; square++)
    {
        U8 piece = (*this)[square];
        v += piece_square_table_[phase][piece >> 1][square];
    }
    return v;
}

int Board::evaluation(int phase)
{
    int v = 0;
    v += material_diff(phase);
    v += psqt(phase);
    return v;
}

#define PHASE_SCALE 128

// Returns a value from 0-PHASE_SCALE based on non pawn material
int Board::phase()
{
    const int midgame_limit = 15258;
    const int endgame_limit = 3915;
    int npm = non_pawn_material();
    npm = max(endgame_limit, min(npm, midgame_limit));
    return ((npm - endgame_limit) * PHASE_SCALE) / (midgame_limit - endgame_limit);
}

int Board::evaluate()
{
    /*
        https://www.chessprogramming.org/Evaluation
    */
    int mg = evaluation(MG);
    int eg = evaluation(EG);
    int p = phase();

    // Tapered eval
    int eval = ((mg * p) + (eg * (PHASE_SCALE - p))) / PHASE_SCALE;

#if 1
    // Mobility
    MoveList list;
    MoveGenerator::add_all_moves(list, *this, WHITE);
    int white_mobility = list.length();
    list.reset();
    MoveGenerator::add_all_moves(list, *this, BLACK);
    int black_mobility = list.length();
    eval += 20 * (white_mobility - black_mobility);
#endif

    // Tempo bonus
    eval += 28 * ((side_to_move() == WHITE) ? 1 : -1);

    // cout << "evaluate=" << v << endl;
    return eval;
}
