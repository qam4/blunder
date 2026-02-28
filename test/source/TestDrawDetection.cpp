/*
 * File:   TestDrawDetection.cpp
 *
 * Tests for draw detection (fifty-move rule, repetition) and
 * mate/stalemate detection via search scores.
 */

#include <catch2/catch_test_macros.hpp>

#include "Tests.h"

// ---------------------------------------------------------------------------
// Draw detection: fifty-move rule boundaries
// ---------------------------------------------------------------------------

TEST_CASE("fifty_move_rule_not_draw_at_99", "[draw detection]")
{
    // half_move_count = 99 should NOT be a draw
    Board board = Parser::parse_fen("4k3/8/8/8/8/8/8/4K3 w - - 99 50");
    REQUIRE_FALSE(board.is_draw());
}

TEST_CASE("fifty_move_rule_draw_at_100", "[draw detection]")
{
    // half_move_count = 100 IS a draw
    Board board = Parser::parse_fen("4k3/8/8/8/8/8/8/4K3 w - - 100 51");
    REQUIRE(board.is_draw());
}

// ---------------------------------------------------------------------------
// Draw detection: threefold repetition (outside search)
// ---------------------------------------------------------------------------

TEST_CASE("threefold_repetition_draw", "[draw detection]")
{
    // Play Ke1-e2-e1-e2-e1 to create threefold repetition
    Board board = Parser::parse_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");

    // We need to create hash history entries by making moves.
    // Position 1: initial (Ke1, Ke8)
    // Move 1: Ke1-e2
    Move_t we2 = build_move(E1, E2);
    Move_t be7 = build_move(E8, E7);
    Move_t we1 = build_move(E2, E1);
    Move_t be8 = build_move(E7, E8);

    // Cycle 1
    board.do_move(we2);
    board.do_move(be7);
    board.do_move(we1);
    board.do_move(be8);
    // Back to start — seen twice now (initial + after cycle 1)
    REQUIRE_FALSE(board.is_draw(false));  // only twofold, not threefold

    // Cycle 2
    board.do_move(we2);
    board.do_move(be7);
    board.do_move(we1);
    board.do_move(be8);
    // Back to start — seen three times now
    REQUIRE(board.is_draw(false));
}

// ---------------------------------------------------------------------------
// Draw detection: twofold repetition during search
// ---------------------------------------------------------------------------

TEST_CASE("twofold_repetition_in_search", "[draw detection]")
{
    Board board = Parser::parse_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");

    Move_t we2 = build_move(E1, E2);
    Move_t be7 = build_move(E8, E7);
    Move_t we1 = build_move(E2, E1);
    Move_t be8 = build_move(E7, E8);

    // One cycle: position seen twice
    board.do_move(we2);
    board.do_move(be7);
    board.do_move(we1);
    board.do_move(be8);

    // Outside search: twofold is NOT a draw
    REQUIRE_FALSE(board.is_draw(false));
    // Inside search: twofold IS a draw
    REQUIRE(board.is_draw(true));
}

// ---------------------------------------------------------------------------
// Mate detection: checkmate returns mate score
// ---------------------------------------------------------------------------

TEST_CASE("checkmate_returns_mate_score", "[mate detection]")
{
    // Scholar's mate position: black is checkmated
    // White queen on f7 delivers checkmate
    Board board =
        Parser::parse_fen("r1bqkb1r/pppp1Qpp/2n2n2/4p3/2B1P3/8/PPPP1PPP/RNB1K1NR b KQkq - 0 4");

    // No legal moves and king is in check = checkmate
    MoveList list;
    MoveGenerator::add_all_moves(list, board, board.side_to_move());
    bool in_check = MoveGenerator::in_check(board, board.side_to_move());
    REQUIRE(list.length() == 0);
    REQUIRE(in_check);
}

TEST_CASE("stalemate_is_game_over", "[mate detection]")
{
    // Classic stalemate: black king on a8, white king on c7, white queen on b6
    // Black has no legal moves but is NOT in check
    Board board = Parser::parse_fen("k7/8/1QK5/8/8/8/8/8 b - - 0 1");

    MoveList list;
    MoveGenerator::add_all_moves(list, board, board.side_to_move());
    bool in_check = MoveGenerator::in_check(board, board.side_to_move());
    REQUIRE(list.length() == 0);
    REQUIRE_FALSE(in_check);
    REQUIRE(board.is_game_over());
}

// ---------------------------------------------------------------------------
// Mate-in-N: search finds mate at sufficient depth
// ---------------------------------------------------------------------------

TEST_CASE("search_finds_mate_score", "[mate detection]")
{
    // Back-rank mate in 1: Rd1-d8#
    Board board = Parser::parse_fen("6k1/5ppp/8/8/8/8/8/3RK3 w - - 0 1");
    Search search(board, board.get_evaluator(), board.get_tt());
    search.search(4, -1);
    int score = search.get_search_best_score();
    // Score should be near MATE_SCORE (positive, large)
    REQUIRE(score > MATE_SCORE - 10);
}
