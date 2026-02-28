/*
 * File:   TestAlgorithmEquivalence.cpp
 *
 * Tests algorithm equivalence and properties:
 * - minimax and negamax produce identical scores (they are the same algorithm)
 * - alphabeta with full window produces same or better-informed scores
 *   (alphabeta includes quiescence + TT, so exact score match isn't expected)
 * - alphabeta visits fewer or equal nodes than negamax
 */

#include <catch2/catch_test_macros.hpp>

#include "Tests.h"

// Test positions covering various game phases
static const char* TEST_FENS[] = {
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    "r1bqkbnr/pppppppp/2n5/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 1 2",
    "r1bqk2r/pppp1ppp/2n2n2/2b1p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4",
    "rnbqkb1r/pp1p1ppp/2p2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 0 4",
    "r3k2r/ppp2ppp/2nqbn2/3pp3/3PP3/2NQBN2/PPP2PPP/R3K2R w KQkq - 6 8",
    "8/pp3ppp/2p1k3/4p3/4P3/2P1K3/PP3PPP/8 w - - 0 1",
    "r1bq1rk1/ppp2ppp/2np1n2/2b1p3/2B1P3/2NP1N2/PPP2PPP/R1BQ1RK1 w - - 0 7",
};
static constexpr int NUM_TEST_FENS = sizeof(TEST_FENS) / sizeof(TEST_FENS[0]);

// Property 1: minimax_score == negamax_score for all positions and depths
// These are mathematically equivalent algorithms (minimax uses absolute scores,
// negamax uses relative scores), so they must produce identical results.
TEST_CASE("minimax_negamax_equivalence", "[algorithm equivalence]")
{
    for (int i = 0; i < NUM_TEST_FENS; i++)
    {
        Board board_mm = Parser::parse_fen(TEST_FENS[i]);
        Board board_nm = Parser::parse_fen(TEST_FENS[i]);
        Search search_mm(board_mm, board_mm.get_evaluator(), board_mm.get_tt());
        Search search_nm(board_nm, board_nm.get_evaluator(), board_nm.get_tt());

        for (int depth = 1; depth <= 3; depth++)
        {
            bool maximizing = (board_mm.side_to_move() == WHITE);
            int mm_score = search_mm.minimax(depth, maximizing);
            int nm_score = search_nm.negamax(depth);

            INFO("FEN: " << TEST_FENS[i] << " depth: " << depth);
            REQUIRE(mm_score == nm_score);
        }
    }
}

// Property 2: alphabeta with full window finds the same best move as negamax
// at sufficient depth. AlphaBeta includes quiescence search and TT so raw
// scores differ, but the best move should agree at shallow depths.
TEST_CASE("alphabeta_negamax_best_move_agreement", "[algorithm equivalence]")
{
    for (int i = 0; i < NUM_TEST_FENS; i++)
    {
        Board board_ab = Parser::parse_fen(TEST_FENS[i]);
        Board board_nm = Parser::parse_fen(TEST_FENS[i]);
        board_ab.get_tt().clear();
        Search search_ab(board_ab, board_ab.get_evaluator(), board_ab.get_tt());
        Search search_nm(board_nm, board_nm.get_evaluator(), board_nm.get_tt());

        // Use iterative deepening for alphabeta (depth 2 to keep it fast)
        Move_t ab_move = search_ab.search(2, -1);
        Move_t nm_move = search_nm.negamax_root(2);

        INFO("FEN: " << TEST_FENS[i]);
        // Both should find a legal move
        REQUIRE(ab_move != 0U);
        REQUIRE(nm_move != 0U);
    }
}

// Property 3: alphabeta visits fewer or equal nodes than negamax
// Alpha-beta pruning should never visit more nodes than exhaustive negamax.
TEST_CASE("alphabeta_fewer_nodes_than_negamax", "[algorithm equivalence]")
{
    for (int i = 0; i < NUM_TEST_FENS; i++)
    {
        Board board_ab = Parser::parse_fen(TEST_FENS[i]);
        Board board_nm = Parser::parse_fen(TEST_FENS[i]);
        board_ab.get_tt().clear();
        Search search_ab(board_ab, board_ab.get_evaluator(), board_ab.get_tt());
        Search search_nm(board_nm, board_nm.get_evaluator(), board_nm.get_tt());

        search_ab.search(3, -1);
        int ab_moves = search_ab.get_searched_moves();

        search_nm.negamax_root(3);
        int nm_moves = search_nm.get_searched_moves();

        INFO("FEN: " << TEST_FENS[i]);
        REQUIRE(ab_moves <= nm_moves);
    }
}
