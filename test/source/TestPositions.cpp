/*
 * File:   TestPositions.cpp
 *
 */

#include <fstream>

#include <catch2/catch_test_macros.hpp>

#include "Tests.h"

// https://www.chessprogramming.org/Test-Positions

std::string test_positions_dir =
    "../../../test/data/test-positions/";  // assumes working dir is ./build/dev/test
const int MAX_SEARCH_MOVES = 1000000;

TEST_CASE("test_positions_WAC", "[test-positions][.]")
{
    // Win at Chess
    // https://www.chessprogramming.org/Win_at_Chess
    std::ifstream infile(test_positions_dir + "WAC.epd");
    REQUIRE(infile.is_open());

    std::string line;
    int score = 0;
    int num_tests = 0;
    while (std::getline(infile, line))
    {
        std::istringstream iss(line);
        Board board = Parser::parse_epd(line);
        Move_t best_move = Parser::parse_san(board.epd_op("bm"), board);
        REQUIRE(best_move != 0);

        // Search with max depth, infinite time, max 1000000 moves searched
        Move_t move = board.search(MAX_SEARCH_PLY, -1, MAX_SEARCH_MOVES);
        if (move == best_move)
        {
            score++;
        }

        num_tests++;
    }
    cout << "WAC score=" << score << "/" << num_tests << endl;
    REQUIRE(num_tests == 300);
    REQUIRE(score >= 118);
}

TEST_CASE("test_positions_debug", "[test-positions][.]")
{
    string epd = "8/7p/5k2/5p2/p1p2P2/Pr1pPK2/1P1R3P/8 b - - bm Rxb2;";
    Board board = Parser::parse_epd(epd);

    Move_t best_move = Parser::parse_san(board.epd_op("bm"), board);
    REQUIRE(best_move != 0);

    Move_t move = board.search(MAX_SEARCH_PLY, -1, MAX_SEARCH_MOVES);
    REQUIRE(move == best_move);
}
