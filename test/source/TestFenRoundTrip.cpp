/*
 * File:   TestFenRoundTrip.cpp
 *
 * Property 13: FEN round-trip produces identical board state.
 * parse(serialize(parse(fen))) == parse(fen)
 */

#include <catch2/catch_test_macros.hpp>

#include "Output.h"
#include "Tests.h"

static const char* ROUND_TRIP_FENS[] = {
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1",
    "r1bqk2r/pppp1ppp/2n2n2/2b1p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4",
    "r3k2r/ppp2ppp/2nqbn2/3pp3/3PP3/2NQBN2/PPP2PPP/R3K2R w KQkq - 6 8",
    "8/pp3ppp/2p1k3/4p3/4P3/2P1K3/PP3PPP/8 w - - 0 1",
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
    // No castling, no EP
    "8/5k2/8/8/8/8/2K5/8 w - - 0 1",
    // Black to move with EP
    "rnbqkbnr/ppp1pppp/8/3pP3/8/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 3",
    // Partial castling rights
    "r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K3 w Qkq - 0 1",
};
static constexpr int NUM_ROUND_TRIP_FENS = sizeof(ROUND_TRIP_FENS) / sizeof(ROUND_TRIP_FENS[0]);

TEST_CASE("fen_round_trip", "[parser]")
{
    for (int i = 0; i < NUM_ROUND_TRIP_FENS; i++)
    {
        // Parse original FEN
        Board board1 = Parser::parse_fen(ROUND_TRIP_FENS[i]);
        // Serialize back to FEN
        string fen1 = Output::board_to_fen(board1);
        // Parse the serialized FEN
        Board board2 = Parser::parse_fen(fen1);
        // Serialize again
        string fen2 = Output::board_to_fen(board2);

        INFO("Original FEN: " << ROUND_TRIP_FENS[i]);
        INFO("Round-trip 1: " << fen1);
        INFO("Round-trip 2: " << fen2);

        // FEN strings should be identical after first round-trip
        REQUIRE(fen1 == fen2);

        // Board state should match
        REQUIRE(board1.side_to_move() == board2.side_to_move());
        REQUIRE(board1.castling_rights() == board2.castling_rights());
        REQUIRE(board1.ep_square() == board2.ep_square());
        REQUIRE(board1.half_move_count() == board2.half_move_count());
        REQUIRE(board1.full_move_count() == board2.full_move_count());

        for (int sq = 0; sq < 64; sq++)
        {
            INFO("square: " << sq);
            REQUIRE(board1[sq] == board2[sq]);
        }
    }
}
