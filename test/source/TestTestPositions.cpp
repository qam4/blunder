/*
 * File:   TestTestPositions.cpp
 *
 */

#include <fstream>

#include <catch2/catch_test_macros.hpp>

#include "TestPositions.h"
#include "Tests.h"

// https://www.chessprogramming.org/Test-Positions

std::string test_positions_dir =
    "../../../test/data/test-positions/";  // assumes working dir is ./build/dev/test

TEST_CASE("test_positions_WAC", "[test-positions][.]")  // disabled
{
    // Win at Chess
    // https://www.chessprogramming.org/Win_at_Chess
    double score = test_positions(test_positions_dir + "WAC.epd");
    REQUIRE(score >= (118.0 / 300));
}

TEST_CASE("test_positions_STS-Rating", "[test-positions][.]")  // disabled
{
    // Strategic Test Suite Rating
    // https://github.com/fsmosca/STS-Rating
    double score = test_positions(test_positions_dir + "STS1-STS15_LAN_v6.epd");
    REQUIRE(score >= (63316.0 / 118800));
}
