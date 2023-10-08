/*
 * File:   TestParser.cpp
 *
 */

#include <catch2/catch_test_macros.hpp>

#include "Tests.h"

TEST_CASE("parser_can_parse_fen", "[parser]")
{
    cout << "- Can parse FEN test 1" << endl;
    string fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    // clang-format off
    U8 expected_board_A[64] = {
        WHITE_ROOK, WHITE_KNIGHT, WHITE_BISHOP, WHITE_QUEEN, WHITE_KING, WHITE_BISHOP, WHITE_KNIGHT, WHITE_ROOK,
        WHITE_PAWN, WHITE_PAWN,   WHITE_PAWN,   WHITE_PAWN,  WHITE_PAWN, WHITE_PAWN,   WHITE_PAWN,   WHITE_PAWN,
        EMPTY,      EMPTY,        EMPTY,        EMPTY,       EMPTY,      EMPTY,        EMPTY,        EMPTY,
        EMPTY,      EMPTY,        EMPTY,        EMPTY,       EMPTY,      EMPTY,        EMPTY,        EMPTY,
        EMPTY,      EMPTY,        EMPTY,        EMPTY,       EMPTY,      EMPTY,        EMPTY,        EMPTY,
        EMPTY,      EMPTY,        EMPTY,        EMPTY,       EMPTY,      EMPTY,        EMPTY,        EMPTY,
        BLACK_PAWN, BLACK_PAWN,   BLACK_PAWN,   BLACK_PAWN,  BLACK_PAWN, BLACK_PAWN,   BLACK_PAWN,   BLACK_PAWN,
        BLACK_ROOK, BLACK_KNIGHT, BLACK_BISHOP, BLACK_QUEEN, BLACK_KING, BLACK_BISHOP, BLACK_KNIGHT, BLACK_ROOK};
    // clang-format on

    Board board = Parser::parse_fen(fen);

    for (int i = 0; i < 64; i++)
    {
        REQUIRE(board[i] == expected_board_A[i]);
    }
    REQUIRE(board.half_move_count() == 0);
    REQUIRE(board.full_move_count() == 1);
    REQUIRE(board.castling_rights() == FULL_CASTLING_RIGHTS);
    REQUIRE(board.ep_square() == NULL_SQUARE);
    REQUIRE(board.side_to_move() == WHITE);
    // cout << Output::board(board) << endl;

    cout << "- Can parse FEN test 2" << endl;
    fen = "rnbqkbnr/pppppppp/8/8/P7/8/RPPPPPPP/1NBQKBNR b Qkq a3 5 2";
    // clang-format off
    U8 expected_board_B[64] = {
        EMPTY,      WHITE_KNIGHT, WHITE_BISHOP, WHITE_QUEEN, WHITE_KING, WHITE_BISHOP, WHITE_KNIGHT, WHITE_ROOK,
        WHITE_ROOK, WHITE_PAWN,   WHITE_PAWN,   WHITE_PAWN,  WHITE_PAWN, WHITE_PAWN,   WHITE_PAWN,   WHITE_PAWN,
        EMPTY,      EMPTY,        EMPTY,        EMPTY,       EMPTY,      EMPTY,        EMPTY,        EMPTY,
        WHITE_PAWN, EMPTY,        EMPTY,        EMPTY,       EMPTY,      EMPTY,        EMPTY,        EMPTY,
        EMPTY,      EMPTY,        EMPTY,        EMPTY,       EMPTY,      EMPTY,        EMPTY,        EMPTY,
        EMPTY,      EMPTY,        EMPTY,        EMPTY,       EMPTY,      EMPTY,        EMPTY,        EMPTY,
        BLACK_PAWN, BLACK_PAWN,   BLACK_PAWN,   BLACK_PAWN,  BLACK_PAWN, BLACK_PAWN,   BLACK_PAWN,   BLACK_PAWN,
        BLACK_ROOK, BLACK_KNIGHT, BLACK_BISHOP, BLACK_QUEEN, BLACK_KING, BLACK_BISHOP, BLACK_KNIGHT, BLACK_ROOK};
    // clang-format on

    board = Parser::parse_fen(fen);

    for (int i = 0; i < 64; i++)
    {
        REQUIRE(board[i] == expected_board_B[i]);
    }
    REQUIRE(board.half_move_count() == 5);
    REQUIRE(board.full_move_count() == 2);
    REQUIRE(board.castling_rights() == (WHITE_QUEEN_SIDE | BLACK_KING_SIDE | BLACK_QUEEN_SIDE));
    REQUIRE(board.ep_square() == A3);
    REQUIRE(board.side_to_move() == BLACK);
    // cout << Output::board(board) << endl;
}

TEST_CASE("parser_can_parse_epd_1", "[parser]")
{
    string epd = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - hmvc 1234; fmvn 4321;";
    // clang-format off
    U8 expected_board_A[64] = {
        WHITE_ROOK, WHITE_KNIGHT, WHITE_BISHOP, WHITE_QUEEN, WHITE_KING, WHITE_BISHOP, WHITE_KNIGHT, WHITE_ROOK,
        WHITE_PAWN, WHITE_PAWN,   WHITE_PAWN,   WHITE_PAWN,  WHITE_PAWN, WHITE_PAWN,   WHITE_PAWN,   WHITE_PAWN,
        EMPTY,      EMPTY,        EMPTY,        EMPTY,       EMPTY,      EMPTY,        EMPTY,        EMPTY,
        EMPTY,      EMPTY,        EMPTY,        EMPTY,       EMPTY,      EMPTY,        EMPTY,        EMPTY,
        EMPTY,      EMPTY,        EMPTY,        EMPTY,       EMPTY,      EMPTY,        EMPTY,        EMPTY,
        EMPTY,      EMPTY,        EMPTY,        EMPTY,       EMPTY,      EMPTY,        EMPTY,        EMPTY,
        BLACK_PAWN, BLACK_PAWN,   BLACK_PAWN,   BLACK_PAWN,  BLACK_PAWN, BLACK_PAWN,   BLACK_PAWN,   BLACK_PAWN,
        BLACK_ROOK, BLACK_KNIGHT, BLACK_BISHOP, BLACK_QUEEN, BLACK_KING, BLACK_BISHOP, BLACK_KNIGHT, BLACK_ROOK};
    // clang-format on

    Board board = Parser::parse_epd(epd);

    for (int i = 0; i < 64; i++)
    {
        REQUIRE(board[i] == expected_board_A[i]);
    }
    REQUIRE(board.half_move_count() == 1234);
    REQUIRE(board.full_move_count() == 4321);
    REQUIRE(board.castling_rights() == FULL_CASTLING_RIGHTS);
    REQUIRE(board.ep_square() == NULL_SQUARE);
    REQUIRE(board.side_to_move() == WHITE);
}

TEST_CASE("parser_can_parse_epd_2", "[parser]")
{
    string epd =
        "8/3r4/pr1Pk1p1/8/7P/6P1/3R3K/5R2 w - - bm Re2+; id arasan21.16; c0 Aldiga (Brainfish "
        "091016)-Knight-king (Komodo 10 64-bit), playchess.com 2016;";
    // clang-format off
    U8 expected_board_A[64] = {
        EMPTY,      EMPTY,      EMPTY, EMPTY,      EMPTY,      WHITE_ROOK, EMPTY,      EMPTY,
        EMPTY,      EMPTY,      EMPTY, WHITE_ROOK, EMPTY,      EMPTY,      EMPTY,      WHITE_KING,
        EMPTY,      EMPTY,      EMPTY, EMPTY,      EMPTY,      EMPTY,      WHITE_PAWN, EMPTY,
        EMPTY,      EMPTY,      EMPTY, EMPTY,      EMPTY,      EMPTY,      EMPTY,      WHITE_PAWN,
        EMPTY,      EMPTY,      EMPTY, EMPTY,      EMPTY,      EMPTY,      EMPTY,      EMPTY,
        BLACK_PAWN, BLACK_ROOK, EMPTY, WHITE_PAWN, BLACK_KING, EMPTY,      BLACK_PAWN, EMPTY,
        EMPTY,      EMPTY,      EMPTY, BLACK_ROOK, EMPTY,      EMPTY,      EMPTY,      EMPTY,
        EMPTY,      EMPTY,      EMPTY, EMPTY,      EMPTY,      EMPTY,      EMPTY,      EMPTY};
    // clang-format on

    Board board = Parser::parse_epd(epd);

    for (int i = 0; i < 64; i++)
    {
        REQUIRE(board[i] == expected_board_A[i]);
    }
    REQUIRE(board.half_move_count() == 0);
    REQUIRE(board.full_move_count() == 1);
    REQUIRE(board.castling_rights() == 0);
    REQUIRE(board.ep_square() == NULL_SQUARE);
    REQUIRE(board.side_to_move() == WHITE);
    REQUIRE(board.epd_op("bm") == "Re2+");
    REQUIRE(board.epd_op("id") == "arasan21.16");
    REQUIRE(board.epd_op("c0")
            == "Aldiga (Brainfish 091016)-Knight-king (Komodo 10 64-bit), playchess.com 2016");
}