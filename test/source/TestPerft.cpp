/*
 * File:   TestPerft.cpp
 *
 */

#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Tests.h"

TEST_CASE("perft starting position depth 3", "[perft]")
{
    // https://www.chessprogramming.org/Perft_Results#Initial_Position
    int move_count = perft_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 3);
    REQUIRE(move_count == 8902);
}

TEST_CASE("perft starting position depth 4", "[perft]")
{
    int move_count = perft_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 4);
    REQUIRE(move_count == 197281);
}

TEST_CASE("perft starting position depth 5", "[perft]")
{
    int move_count = perft_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 5);
    REQUIRE(move_count == 4865609);
}

TEST_CASE("perft starting position depth 6", "[perft]")
{
    int move_count = perft_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 6);
    REQUIRE(move_count == 119060324);
}

TEST_CASE("perft position 2 depth 3", "[perft]")
{
    // https://www.chessprogramming.org/Perft_Results#Position_2
    int move_count =
        perft_fen("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -", 3);
    REQUIRE(move_count == 97862);
}

TEST_CASE("perft position 2 depth 4", "[perft]")
{
    // https://www.chessprogramming.org/Perft_Results#Position_2
    int move_count =
        perft_fen("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -", 4);
    REQUIRE(move_count == 4085603);
}

TEST_CASE("perft position 3 depth 5", "[perft]")
{
    // https://www.chessprogramming.org/Perft_Results#Position_3
    int move_count = perft_fen("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -", 5);
    REQUIRE(move_count == 674624);
}

TEST_CASE("perft position 4 depth 4", "[perft]")
{
    // https://www.chessprogramming.org/Perft_Results#Position_4
    int move_count =
        perft_fen("r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1", 4);
    REQUIRE(move_count == 422333);
}

TEST_CASE("perft position 5 depth 3", "[perft]")
{
    // https://www.chessprogramming.org/Perft_Results#Position_5
    int move_count = perft_fen("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 3);
    REQUIRE(move_count == 62379);
}

TEST_CASE("perft position 5 debug 1", "[perft]")
{
    // https://www.chessprogramming.org/Perft_Results#Position_5
    int move_count =
        perft_fen("r3k2r/p1pp1pb1/bn2pqp1/3PN3/1p2P3/2N5/PPPBBPpP/R4K1R w kq - 0 1", 1);
    REQUIRE(move_count == 3);
}

TEST_CASE("perft starting position benchmark", "[perft]")
{
    BENCHMARK("start position depth 4")
    {
        return perft_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 4);
    };
}
