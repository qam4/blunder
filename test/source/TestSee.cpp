/*
 * File:   TestSee.cpp
 *
 */

#include "Tests.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("move_generator_see_1", "[move generator see]")
{
    // https://www.chessprogramming.org/SEE_-_The_Swap_Algorithm#Position_1
    string fen = "1k1r4/1pp4p/p7/4p3/8/P5P1/1PP4P/2K1R3 w - - ; Rxe5?";
    Board board = Parser::parse_fen(fen);
    Move_t move = Parser::parse_san("Rxe5", board);
    REQUIRE(MoveGenerator::see(board, move) == SEE_PIECE_VALUE[PAWN >> 1]);
}

TEST_CASE("move_generator_see_2", "[move generator see]")
{
    // https://www.chessprogramming.org/SEE_-_The_Swap_Algorithm#Position_2
    string fen = "1k1r3q/1ppn3p/p4b2/4p3/8/P2N2P1/1PP1R1BP/2K1Q3 w - - ; Nxe5?";
    Board board = Parser::parse_fen(fen);
    Move_t move = Parser::parse_san("Nxe5", board);
    REQUIRE(MoveGenerator::see(board, move)
            == SEE_PIECE_VALUE[PAWN >> 1] - SEE_PIECE_VALUE[KNIGHT >> 1]);
}

TEST_CASE("move_generator_see_debug", "[move generator see]")
{
    string fen = "3q2nk/pb1r1p2/np6/3P2Pp/2p1P3/2R4B/PQ3P1P/3R2K1 w - h6";
    Board board = Parser::parse_fen(fen);
    Move_t move = Parser::parse_san("gxh6", board);
    REQUIRE(MoveGenerator::see(board, move) == 0);
}

TEST_CASE("move_generator_see_3", "[move generator see]")
{
    struct SeeData
    {
        string fen;
        string move;
        int see;
        SeeData(string fenStr, string moveStr, int seeStr)
            : fen(fenStr)
            , move(moveStr)
            , see(seeStr)
        {
        }
    };

    SeeData test_data[] = {
        SeeData("4R3/2r3p1/5bk1/1p1r3p/p2PR1P1/P1BK1P2/1P6/8 b - -", "hxg4", 0),
        SeeData("4R3/2r3p1/5bk1/1p1r1p1p/p2PR1P1/P1BK1P2/1P6/8 b - -", "hxg4", 0),
        SeeData("4r1k1/5pp1/nbp4p/1p2p2q/1P2P1b1/1BP2N1P/1B2QPPK/3R4 b - -",
                "Bxf3",
                SEE_PIECE_VALUE[KNIGHT >> 1] - SEE_PIECE_VALUE[BISHOP >> 1]),
        SeeData("2r1r1k1/pp1bppbp/3p1np1/q3P3/2P2P2/1P2B3/P1N1B1PP/2RQ1RK1 b - -",
                "dxe5",
                SEE_PIECE_VALUE[PAWN >> 1]),
        SeeData("7r/5qpk/p1Qp1b1p/3r3n/BB3p2/5p2/P1P2P2/4RK1R w - -", "Re8", 0),
        SeeData("6rr/6pk/p1Qp1b1p/2n5/1B3p2/5p2/P1P2P2/4RK1R w - -",
                "Re8",
                -SEE_PIECE_VALUE[ROOK >> 1]),
        SeeData("7r/5qpk/2Qp1b1p/1N1r3n/BB3p2/5p2/P1P2P2/4RK1R w - -",
                "Re8",
                -SEE_PIECE_VALUE[ROOK >> 1]),
        // promotions
        // SeeData("6RR/4bP2/8/8/5r2/3K4/5p2/4k3 w - -", "f8=Q", SEE_PIECE_VALUE[BISHOP >> 1]-SEE_PIECE_VALUE[PAWN >> 1]),
        // SeeData("6RR/4bP2/8/8/5r2/3K4/5p2/4k3 w - -", "f8=N", SEE_PIECE_VALUE[KNIGHT >> 1]-SEE_PIECE_VALUE[PAWN >> 1]),
        // SeeData("7R/5P2/8/8/8/3K2r1/5p2/4k3 w - -", "f8=Q", SEE_PIECE_VALUE[QUEEN >> 1]-SEE_PIECE_VALUE[PAWN >> 1]),
        // SeeData("7R/5P2/8/8/8/3K2r1/5p2/4k3 w - -", "f8=B", SEE_PIECE_VALUE[BISHOP >> 1]-SEE_PIECE_VALUE[PAWN >> 1]),
        // SeeData("7R/4bP2/8/8/1q6/3K4/5p2/4k3 w - -", "f8=R", -SEE_PIECE_VALUE[PAWN >> 1]),
        SeeData("8/4kp2/2npp3/1Nn5/1p2PQP1/7q/1PP1B3/4KR1r b - -", "Rxf1+", 0),
        SeeData("8/4kp2/2npp3/1Nn5/1p2P1P1/7q/1PP1B3/4KR1r b - -", "Rxf1+", 0),
        SeeData("2r2r1k/6bp/p7/2q2p1Q/3PpP2/1B6/P5PP/2RR3K b - -",
                "Qxc1",
                2 * SEE_PIECE_VALUE[ROOK >> 1] - SEE_PIECE_VALUE[QUEEN >> 1]),
        SeeData("r2qk1nr/pp2ppbp/2b3p1/2p1p3/8/2N2N2/PPPP1PPP/R1BQR1K1 w kq -",
                "Nxe5",
                SEE_PIECE_VALUE[PAWN >> 1]),
        SeeData("6r1/4kq2/b2p1p2/p1pPb3/p1P2B1Q/2P4P/2B1R1P1/6K1 w - -", "Bxe5", 0),
        // en-passant
        SeeData("3q2nk/pb1r1p2/np6/3P2Pp/2p1P3/2R4B/PQ3P1P/3R2K1 w - h6", "gxh6", 0),
        SeeData("3q2nk/pb1r1p2/np6/3P2Pp/2p1P3/2R1B2B/PQ3P1P/3R2K1 w - h6", "gxh6", SEE_PIECE_VALUE[PAWN >> 1]),
        SeeData("2r4r/1P4pk/p2p1b1p/7n/BB3p2/2R2p2/P1P2P2/4RK2 w - -",
                "Rxc8",
                SEE_PIECE_VALUE[ROOK >> 1]),
        // promotion+capture
        // SeeData("2r5/1P4pk/p2p1b1p/5b1n/BB3p2/2R2p2/P1P2P2/4RK2 w - -", "Rxc8", SEE_PIECE_VALUE[ROOK >> 1]),
        SeeData("2r4k/2r4p/p7/2b2p1b/4pP2/1BR5/P1R3PP/2Q4K w - -",
                "Rxc5",
                SEE_PIECE_VALUE[BISHOP >> 1]),
        SeeData("8/pp6/2pkp3/4bp2/2R3b1/2P5/PP4B1/1K6 w - -",
                "Bxc6",
                SEE_PIECE_VALUE[PAWN >> 1] - SEE_PIECE_VALUE[BISHOP >> 1]),
        SeeData("4q3/1p1pr1k1/1B2rp2/6p1/p3PP2/P3R1P1/1P2R1K1/4Q3 b - -",
                "Rxe4",
                SEE_PIECE_VALUE[PAWN >> 1] - SEE_PIECE_VALUE[ROOK >> 1]),
        SeeData("4q3/1p1pr1kb/1B2rp2/6p1/p3PP2/P3R1P1/1P2R1K1/4Q3 b - -",
                "Bxe4",
                SEE_PIECE_VALUE[PAWN >> 1])
    };

    size_t len = sizeof(test_data) / sizeof(SeeData);
    for (size_t i = 0; i < len; i++)
    {
        cout << "see test " << i << endl;
        SeeData data = test_data[i];
        Board board = Parser::parse_fen(data.fen);
        Move_t move = Parser::parse_san(data.move, board);
        REQUIRE(MoveGenerator::see(board, move) == data.see);
    }
}
