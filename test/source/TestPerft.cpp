/*
 * File:   TestPerft.cpp
 *
 */

#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Tests.h"

TEST_CASE("perft starting position depth 1", "[perft]")
{
    // https://www.chessprogramming.org/Perft_Results#Initial_Position
    long move_count = perft_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 1);
    REQUIRE(move_count == 20);
}

TEST_CASE("perft starting position depth 2", "[perft]")
{
    long move_count = perft_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 2);
    REQUIRE(move_count == 400);
}

TEST_CASE("perft starting position depth 3", "[perft]")
{
    // https://www.chessprogramming.org/Perft_Results#Initial_Position
    long move_count = perft_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 3);
    REQUIRE(move_count == 8902);
}

TEST_CASE("perft starting position depth 4", "[perft]")
{
    long move_count = perft_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 4);
    REQUIRE(move_count == 197281);
}

TEST_CASE("perft starting position depth 5", "[perft]")
{
    long move_count = perft_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 5);
    REQUIRE(move_count == 4865609);
}

TEST_CASE("perft starting position depth 6", "[perft]")
{
    long move_count = perft_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 6);
    REQUIRE(move_count == 119060324);
}

TEST_CASE("perft position 2 depth 3", "[perft]")
{
    // https://www.chessprogramming.org/Perft_Results#Position_2
    long move_count =
        perft_fen("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -", 3);
    REQUIRE(move_count == 97862);
}

TEST_CASE("perft position 2 depth 4", "[perft]")
{
    // https://www.chessprogramming.org/Perft_Results#Position_2
    long move_count =
        perft_fen("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -", 4);
    REQUIRE(move_count == 4085603);
}

TEST_CASE("perft position 2 depth 5", "[perft]")
{
    // https://www.chessprogramming.org/Perft_Results#Position_2
    long move_count =
        perft_fen("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -", 5);
    REQUIRE(move_count == 193690690);
}

TEST_CASE("perft position 3 depth 5", "[perft]")
{
    // https://www.chessprogramming.org/Perft_Results#Position_3
    long move_count = perft_fen("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -", 5);
    REQUIRE(move_count == 674624);
}

TEST_CASE("perft position 3 depth 6", "[perft]")
{
    // https://www.chessprogramming.org/Perft_Results#Position_3
    long move_count = perft_fen("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -", 6);
    REQUIRE(move_count == 11030083);
}

TEST_CASE("perft position 4 depth 4", "[perft]")
{
    // https://www.chessprogramming.org/Perft_Results#Position_4
    long move_count =
        perft_fen("r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1", 4);
    REQUIRE(move_count == 422333);
}

TEST_CASE("perft position 4 depth 5", "[perft]")
{
    // https://www.chessprogramming.org/Perft_Results#Position_4
    long move_count =
        perft_fen("r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1", 5);
    REQUIRE(move_count == 15833292);
}

TEST_CASE("perft position 5 depth 3", "[perft]")
{
    // https://www.chessprogramming.org/Perft_Results#Position_5
    long move_count = perft_fen("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 3);
    REQUIRE(move_count == 62379);
}

TEST_CASE("perft position 5 depth 4", "[perft]")
{
    // https://www.chessprogramming.org/Perft_Results#Position_5
    long move_count = perft_fen("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 4);
    REQUIRE(move_count == 2103487);
}

TEST_CASE("perft position 5 debug 1", "[perft]")
{
    // https://www.chessprogramming.org/Perft_Results#Position_5
    long move_count =
        perft_fen("r3k2r/p1pp1pb1/bn2pqp1/3PN3/1p2P3/2N5/PPPBBPpP/R4K1R w kq - 0 1", 1);
    REQUIRE(move_count == 3);
}

// Position 6 from CPW — alternative starting position
// https://www.chessprogramming.org/Perft_Results#Position_6
TEST_CASE("perft position 6 depth 4", "[perft]")
{
    long move_count =
        perft_fen("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", 4);
    REQUIRE(move_count == 3894594);
}

// Promotion-heavy position (CPW Position 4 mirrored)
TEST_CASE("perft promotions depth 5", "[perft]")
{
    long move_count = perft_fen("n1n5/PPPk4/8/8/8/8/4Kppp/5N1N b - - 0 1", 5);
    REQUIRE(move_count == 3605103);
}

// Castling rights edge case — all castling available, open board
TEST_CASE("perft castling rook capture", "[perft]")
{
    long move_count = perft_fen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1", 5);
    REQUIRE(move_count == 7594526);
}

// En-passant edge case — EP available with pin considerations
TEST_CASE("perft en passant edge case", "[perft]")
{
    // Position 3 from CPW at depth 4 (includes EP captures)
    long move_count = perft_fen("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -", 4);
    REQUIRE(move_count == 43238);
}

TEST_CASE("perft starting position benchmark", "[perft]")
{
    BENCHMARK("start position depth 4")
    {
        return perft_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 4);
    };
}
