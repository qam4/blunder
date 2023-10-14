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

TEST_CASE("parser_can_parse_san_1", "[parser]")
{
    string fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    Board board = Parser::parse_fen(fen);

    string san;
    Move_t move;

    san = "f3";
    move = Parser::parse_san(san, board);
    REQUIRE(move == build_move(F2, F3));
    san = "e4";
    move = Parser::parse_san(san, board);
    REQUIRE(move == build_pawn_double_push(E2, E4));
    san = "Nc3";
    move = Parser::parse_san(san, board);
    REQUIRE(move == build_move(B1, C3));
    san = "Nf3";
    move = Parser::parse_san(san, board);
    REQUIRE(move == build_move(G1, F3));

    // non existent move
    san = "Bc4";
    move = Parser::parse_san(san, board);
    REQUIRE(move == 0);
}

TEST_CASE("parser_can_parse_san_2", "[parser]")
{
    string fen = "3bk2r/2P5/6N1/4p3/1pN5/4p3/N1N2P2/R3K2R w KQkq - 0 1";
    Board board = Parser::parse_fen(fen);

    string san;
    Move_t move;

    // check 2 pieces on same file: Ngf3, Nef3, Ngxf3, Nexf3
    san = "Naxb4";
    move = Parser::parse_san(san, board);
    REQUIRE(move == build_capture(A2, B4, BLACK_PAWN));
    san = "Ncb4";
    move = Parser::parse_san(san, board);
    REQUIRE(move == build_capture(C2, B4, BLACK_PAWN));

    // check 2 pieces on same rank: N5f3, N1f3, N5xf3, N1xf3
    san = "N2xe3";
    move = Parser::parse_san(san, board);
    REQUIRE(move == build_capture(C2, E3, BLACK_PAWN));
    san = "N4e3";
    move = Parser::parse_san(san, board);
    REQUIRE(move == build_capture(C4, E3, BLACK_PAWN));

    // check 2 pieces different rank and file: Nhf3, Ndf3, Nhxf3, Ndxf3
    san = "Ncxe5";
    move = Parser::parse_san(san, board);
    REQUIRE(move == build_capture(C4, E5, BLACK_PAWN));
    san = "Nge5";
    move = Parser::parse_san(san, board);
    REQUIRE(move == build_capture(G6, E5, BLACK_PAWN));

    // pawn capture: ed4, exd4
    san = "fxe3";
    move = Parser::parse_san(san, board);
    REQUIRE(move == build_capture(F2, E3, BLACK_PAWN));
    san = "fe3";
    move = Parser::parse_san(san, board);
    REQUIRE(move == build_capture(F2, E3, BLACK_PAWN));

    // check castles
    san = "O-O";
    move = Parser::parse_san(san, board);
    REQUIRE(move == build_castle(KING_CASTLE));
    san = "O-O-O";
    move = Parser::parse_san(san, board);
    REQUIRE(move == build_castle(QUEEN_CASTLE));

    // check promo
    san = "c8Q";
    move = Parser::parse_san(san, board);
    REQUIRE(move == build_promotion(C7, C8, WHITE_QUEEN));
    san = "cd8R";
    move = Parser::parse_san(san, board);
    REQUIRE(move == build_capture_promotion(C7, D8, BLACK_BISHOP, WHITE_ROOK));
    san = "cxd8B";
    move = Parser::parse_san(san, board);
    REQUIRE(move == build_capture_promotion(C7, D8, BLACK_BISHOP, WHITE_BISHOP));

    // check ignoring trailing +, ++, #, e.p.
    san = "cxd8Q+";
    move = Parser::parse_san(san, board);
    REQUIRE(move == build_capture_promotion(C7, D8, BLACK_BISHOP, WHITE_QUEEN));
}

TEST_CASE("parser_can_parse_san_3", "[parser]")
{
    // Black side to move
    string epd = "8/7p/5k2/5p2/p1p2P2/Pr1pPK2/1P1R3P/8 b - - bm Rxb2;";
    Board board = Parser::parse_epd(epd);
    Move_t best_move = Parser::parse_san(board.epd_op("bm"), board);
    REQUIRE(best_move != 0);
}
