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

// --- Fixed-depth benchmarks (Requirement 9.1) ---

TEST_CASE("test_positions_WAC_depth8", "[.][slow][test-positions][fixed-depth]")
{
    auto result = test_positions_ex(test_positions_dir + "WAC.epd",
                                    SearchMode::FixedDepth, 8);
    INFO("WAC depth-8: " << result.score_pct << "% NPS=" << result.nps);
    CHECK(result.score_pct >= 50.0);
}

TEST_CASE("test_positions_STS_depth8", "[.][slow][test-positions][fixed-depth]")
{
    auto result = test_positions_ex(test_positions_dir + "STS1-STS15_LAN_v6.epd",
                                    SearchMode::FixedDepth, 8);
    INFO("STS depth-8: " << result.score_pct << "% ELO=" << result.elo << " NPS=" << result.nps);
    CHECK(result.score_pct >= 50.0);
}

// --- Fixed-time benchmarks (Requirement 9.2) ---

TEST_CASE("test_positions_WAC_time1s", "[.][slow][test-positions][fixed-time]")
{
    auto result = test_positions_ex(test_positions_dir + "WAC.epd",
                                    SearchMode::FixedTime, 1000);  // 1 second per position
    INFO("WAC 1s/pos: " << result.score_pct << "% NPS=" << result.nps);
    CHECK(result.score_pct >= 50.0);
}

TEST_CASE("test_positions_STS_time1s", "[.][slow][test-positions][fixed-time]")
{
    auto result = test_positions_ex(test_positions_dir + "STS1-STS15_LAN_v6.epd",
                                    SearchMode::FixedTime, 1000);
    INFO("STS 1s/pos: " << result.score_pct << "% ELO=" << result.elo << " NPS=" << result.nps);
    CHECK(result.score_pct >= 50.0);
}
