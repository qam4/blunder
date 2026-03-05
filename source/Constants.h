/*
 * File:   Constants.h
 *
 */

#ifndef CONSTANTS_H
#define CONSTANTS_H

#include "Common.h"

#include <string_view>

constexpr int UNKNOWN_SCORE = 500000;
constexpr int MAX_SCORE = 200000;
constexpr int MATE_SCORE = 100000;  // As defined in xboard protocol
constexpr int ASPIRATION_WINDOW = 50;
constexpr int DRAW_SCORE = 0;
constexpr int MAX_SEARCH_PLY = 64;

constexpr int DEFAULT_SEARCH_TIME = 1000000;  // default search time in usec
constexpr int MAX_GAME_PLY = 1024;  // max number of moves in a chess game (theoretically 5949)

constexpr const char* DEFAULT_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

// PIECES
constexpr U8 EMPTY = 0;

constexpr U8 WHITE = 0;
constexpr U8 BLACK = 1;

constexpr U8 PAWN = 2;
constexpr U8 KNIGHT = 4;
constexpr U8 BISHOP = 6;
constexpr U8 ROOK = 8;
constexpr U8 QUEEN = 10;
constexpr U8 KING = 12;

constexpr U8 WHITE_PAWN = 2;
constexpr U8 BLACK_PAWN = 3;
constexpr U8 WHITE_KNIGHT = 4;
constexpr U8 BLACK_KNIGHT = 5;
constexpr U8 WHITE_BISHOP = 6;
constexpr U8 BLACK_BISHOP = 7;
constexpr U8 WHITE_ROOK = 8;
constexpr U8 BLACK_ROOK = 9;
constexpr U8 WHITE_QUEEN = 10;
constexpr U8 BLACK_QUEEN = 11;
constexpr U8 WHITE_KING = 12;
constexpr U8 BLACK_KING = 13;
constexpr int NUM_PIECES = 14;

constexpr std::string_view PIECE_CHARS = "_EPpNnBbRrQqKk";

// Unicode chess piece characters (indexed same as PIECE_CHARS)
// Index: 0=EMPTY, 1=EMPTY, 2=WHITE_PAWN, 3=BLACK_PAWN, 4=WHITE_KNIGHT, ...
inline const char* const PIECE_UNICODE[] = {
    " ", " ",
    "\xe2\x99\x99", "\xe2\x99\x9f",  // WHITE_PAWN, BLACK_PAWN
    "\xe2\x99\x98", "\xe2\x99\x9e",  // WHITE_KNIGHT, BLACK_KNIGHT
    "\xe2\x99\x97", "\xe2\x99\x9d",  // WHITE_BISHOP, BLACK_BISHOP
    "\xe2\x99\x96", "\xe2\x99\x9c",  // WHITE_ROOK, BLACK_ROOK
    "\xe2\x99\x95", "\xe2\x99\x9b",  // WHITE_QUEEN, BLACK_QUEEN
    "\xe2\x99\x94", "\xe2\x99\x9a",  // WHITE_KING, BLACK_KING
};

// CASTLING RIGHTS

constexpr U8 WHITE_KING_SIDE = 1;
constexpr U8 WHITE_QUEEN_SIDE = 2;
constexpr U8 BLACK_KING_SIDE = 4;
constexpr U8 BLACK_QUEEN_SIDE = 8;
constexpr U8 FULL_CASTLING_RIGHTS = 1 | 2 | 4 | 8;

// SQUARES
constexpr U64 LIGHT_SQUARES = C64(0x55AA55AA55AA55AA);
constexpr U64 DARK_SQUARES = C64(0xAA55AA55AA55AA55);

constexpr int A1 = 0;
constexpr int B1 = 1;
constexpr int C1 = 2;
constexpr int D1 = 3;
constexpr int E1 = 4;
constexpr int F1 = 5;
constexpr int G1 = 6;
constexpr int H1 = 7;

constexpr int A2 = 8;
constexpr int B2 = 9;
constexpr int C2 = 10;
constexpr int D2 = 11;
constexpr int E2 = 12;
constexpr int F2 = 13;
constexpr int G2 = 14;
constexpr int H2 = 15;

constexpr int A3 = 16;
constexpr int B3 = 17;
constexpr int C3 = 18;
constexpr int D3 = 19;
constexpr int E3 = 20;
constexpr int F3 = 21;
constexpr int G3 = 22;
constexpr int H3 = 23;

constexpr int A4 = 24;
constexpr int B4 = 25;
constexpr int C4 = 26;
constexpr int D4 = 27;
constexpr int E4 = 28;
constexpr int F4 = 29;
constexpr int G4 = 30;
constexpr int H4 = 31;

constexpr int A5 = 32;
constexpr int B5 = 33;
constexpr int C5 = 34;
constexpr int D5 = 35;
constexpr int E5 = 36;
constexpr int F5 = 37;
constexpr int G5 = 38;
constexpr int H5 = 39;

constexpr int A6 = 40;
constexpr int B6 = 41;
constexpr int C6 = 42;
constexpr int D6 = 43;
constexpr int E6 = 44;
constexpr int F6 = 45;
constexpr int G6 = 46;
constexpr int H6 = 47;

constexpr int A7 = 48;
constexpr int B7 = 49;
constexpr int C7 = 50;
constexpr int D7 = 51;
constexpr int E7 = 52;
constexpr int F7 = 53;
constexpr int G7 = 54;
constexpr int H7 = 55;

constexpr int A8 = 56;
constexpr int B8 = 57;
constexpr int C8 = 58;
constexpr int D8 = 59;
constexpr int E8 = 60;
constexpr int F8 = 61;
constexpr int G8 = 62;
constexpr int H8 = 63;

constexpr int NULL_SQUARE = 64;
constexpr int NUM_SQUARES = 64;

// Empty bitboard
constexpr U64 BB_EMPTY = 0ULL;

// BITBOARD FILES

constexpr U64 FILE_A = 0x0101010101010101ULL << 0;
constexpr U64 FILE_B = 0x0101010101010101ULL << 1;
constexpr U64 FILE_C = 0x0101010101010101ULL << 2;
constexpr U64 FILE_D = 0x0101010101010101ULL << 3;
constexpr U64 FILE_E = 0x0101010101010101ULL << 4;
constexpr U64 FILE_F = 0x0101010101010101ULL << 5;
constexpr U64 FILE_G = 0x0101010101010101ULL << 6;
constexpr U64 FILE_H = 0x0101010101010101ULL << 7;

// BITBOARD ROWS

constexpr U64 ROW_1 = 0xFFULL << (0 * 8);
constexpr U64 ROW_2 = 0xFFULL << (1 * 8);
constexpr U64 ROW_3 = 0xFFULL << (2 * 8);
constexpr U64 ROW_4 = 0xFFULL << (3 * 8);
constexpr U64 ROW_5 = 0xFFULL << (4 * 8);
constexpr U64 ROW_6 = 0xFFULL << (5 * 8);
constexpr U64 ROW_7 = 0xFFULL << (6 * 8);
constexpr U64 ROW_8 = 0xFFULL << (7 * 8);

#endif /* CONSTANTS_H */
