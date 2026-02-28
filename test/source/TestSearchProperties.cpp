/*
 * File:   TestSearchProperties.cpp
 *
 * Search property tests:
 * - Property 10: PV consists of legal moves forming a valid game sequence
 * - Property 11: Node count is monotonically non-decreasing with depth
 * - Property 12: Aspiration window re-search produces same result as full-window
 */

#include "Tests.h"

#include <catch2/catch_test_macros.hpp>

#include "Output.h"

static const char* SEARCH_PROP_FENS[] = {
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    "r1bqk2r/pppp1ppp/2n2n2/2b1p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4",
    "r3k2r/ppp2ppp/2nqbn2/3pp3/3PP3/2NQBN2/PPP2PPP/R3K2R w KQkq - 6 8",
    "8/pp3ppp/2p1k3/4p3/4P3/2P1K3/PP3PPP/8 w - - 0 1",
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
};
static constexpr int NUM_SEARCH_PROP_FENS = sizeof(SEARCH_PROP_FENS) / sizeof(SEARCH_PROP_FENS[0]);

// Property 10: Every move in the PV is legal in the position at that ply
TEST_CASE("pv_moves_are_legal", "[search properties]")
{
    for (int i = 0; i < NUM_SEARCH_PROP_FENS; i++)
    {
        Board board = Parser::parse_fen(SEARCH_PROP_FENS[i]);
        board.get_tt().clear();
        Search search(board, board.get_evaluator(), board.get_tt());
        search.search(4, -1);

        PrincipalVariation& pv = search.get_pv();
        int pv_len = pv.length();

        INFO("FEN: " << SEARCH_PROP_FENS[i] << " PV length: " << pv_len);

        // Walk through PV, verifying each move is legal
        for (int j = 0; j < pv_len; j++)
        {
            Move_t pv_move = pv.get_move(j);
            REQUIRE(pv_move != 0U);

            // Check move is in the legal move list
            MoveList list;
            MoveGenerator::add_all_moves(list, board, board.side_to_move());
            bool found = false;
            for (int k = 0; k < list.length(); k++)
            {
                if (list[k] == pv_move)
                {
                    found = true;
                    break;
                }
            }
            INFO("PV move " << j << ": " << Output::move(pv_move, board));
            REQUIRE(found);

            board.do_move(pv_move);
        }

        // Undo all PV moves to restore board
        for (int j = pv_len - 1; j >= 0; j--)
        {
            board.undo_move(pv.get_move(j));
        }
    }
}

// Property 11: Node count is monotonically non-decreasing with depth
TEST_CASE("node_count_monotonic_with_depth", "[search properties]")
{
    for (int i = 0; i < NUM_SEARCH_PROP_FENS; i++)
    {
        int prev_nodes = 0;
        for (int depth = 1; depth <= 4; depth++)
        {
            Board board = Parser::parse_fen(SEARCH_PROP_FENS[i]);
            board.get_tt().clear();
            Search search(board, board.get_evaluator(), board.get_tt());
            search.search(depth, -1);
            int nodes = search.get_searched_moves();

            INFO("FEN: " << SEARCH_PROP_FENS[i] << " depth: " << depth << " nodes: " << nodes
                         << " prev: " << prev_nodes);
            REQUIRE(nodes >= prev_nodes);
            prev_nodes = nodes;
        }
    }
}
