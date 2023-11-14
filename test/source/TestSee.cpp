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
    REQUIRE(MoveGenerator::see(board, move) == 1);
}

TEST_CASE("move_generator_see_2", "[move generator see]")
{
    // https://www.chessprogramming.org/SEE_-_The_Swap_Algorithm#Position_2
    string fen = "1k1r3q/1ppn3p/p4b2/4p3/8/P2N2P1/1PP1R1BP/2K1Q3 w - - ; Nxe5?";
    Board board = Parser::parse_fen(fen);
    Move_t move = Parser::parse_san("Nxe5", board);
    REQUIRE(MoveGenerator::see(board, move) == -2);
}
