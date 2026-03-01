/*
 * File:   TestTestPositions.cpp
 *
 */

#include <fstream>

#include <catch2/catch_test_macros.hpp>

#include "TestPositions.h"
#include "Tests.h"

// https://www.chessprogramming.org/Test-Positions

std::string test_positions_dir = std::string(PROJECT_ROOT_DIR) + "/test/data/test-positions/";

TEST_CASE("test_positions_WAC", "[.][slow][test-positions]")
{
    // Win at Chess
    // https://www.chessprogramming.org/Win_at_Chess
    double score = test_positions(test_positions_dir + "WAC.epd");
    INFO("WAC score: " << score << " (threshold: " << (162.0 / 300) << ")");
    CHECK(score >= (162.0 / 300));
}

TEST_CASE("test_positions_STS-Rating", "[.][slow][test-positions]")
{
    // Strategic Test Suite Rating
    // https://github.com/fsmosca/STS-Rating
    double score = test_positions(test_positions_dir + "STS1-STS15_LAN_v6.epd");
    INFO("STS score: " << score << " (threshold: " << (66757.0 / 118800) << ")");
    CHECK(score >= (66757.0 / 118800));  // ELO=2259
}
