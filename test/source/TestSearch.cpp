/*
 * File:   TestSearch.cpp
 *
 */

#include "Tests.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("search_white_mates_in_two_1", "[search]")
{
    cout << "- Mate in two" << endl;
    string fen = "4r1rk/5K1b/7R/R7/8/8/8/8 w - - 0 1";
    Board board = Parser::parse_fen(fen);
    Move_t move = board.search(4, -1);
    REQUIRE(move == build_capture(H6, H7, BLACK_BISHOP));  // [ Rxh7+ Kxh7 Rh5# ]
}

TEST_CASE("search_black_mates_in_two_1", "[search]")
{
    cout << "- Mate in two" << endl;
    string fen = "8/8/8/8/1b6/1k6/8/KBB5 b - - 0 1";
    Board board = Parser::parse_fen(fen);
    Move_t move = board.search(4, -1);
    REQUIRE(move == build_move(B4, C3));  // [ ...Bc3+ ]
}

TEST_CASE("search_white_mates_in_three_1", "[search]")
{
    cout << "- Mate in three" << endl;
    string fen = "1rb5/1p2k2r/p5n1/2p1pp2/2B5/6P1/PPPB1PP1/2KR4 w - - 1 0";
    Board board = Parser::parse_fen(fen);
    Move_t move = board.search(6, -1);
    REQUIRE(move == build_move(D2, G5));  // [ 1.Bg5+ if 1...Kf8 2.Rd8+ ]
}

TEST_CASE("search_black_mates_in_three_1", "[search]")
{
    cout << "- Mate in three" << endl;
    string fen = "7r/p5p1/7k/2Np3p/3P4/2P2Q2/Pr5q/R4K2 b - - 0 1";
    Board board = Parser::parse_fen(fen);
    Move_t move = board.search(6, -1);
    REQUIRE(move == build_move(H8, F8));  // [ 1...Rf8 ]
}

TEST_CASE("search_white_mates_in_four_1", "[search]")
{
    cout << "- Mate in four" << endl;
    string fen = "2r1brk1/p7/q3p2p/8/2PQR3/6P1/1P2KPP1/8 w - - 1 0";
    Board board = Parser::parse_fen(fen);
    Move_t move = board.search(8, -1);
    REQUIRE(move == build_move(E4, G4));  // [ Rg4+ Bg6 Rxg6+ Kf7 Rg7+ ]
}

TEST_CASE("search_black_mates_in_four_1", "[search]")
{
    cout << "- Mate in four" << endl;
    string fen = "k1r5/1R3R2/2r3p1/p4p1p/5Q2/1P6/PKP2PPP/5q2 b - - 0 1";
    Board board = Parser::parse_fen(fen);
    Move_t move = board.search(8, -1);
    REQUIRE(move == build_capture(C6, C2, WHITE_PAWN));  // [ ...Rxc2+ Ka3 Rxa2+ Kxa2 Rc2+ ]
}
