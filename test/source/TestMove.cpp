/*
 * File:   TestMove.cpp
 *
 */

#include <catch2/catch_test_macros.hpp>

#include "Tests.h"

TEST_CASE("move", "[move]")
{
    cout << "Testing Move helper functions" << endl;
    Move_t move;
    // test normal move
    move = build_move(A3, A4);
    REQUIRE(move_from(move) == A3);
    REQUIRE(move_to(move) == A4);
    REQUIRE(!is_capture(move));
    REQUIRE(!move_flags(move));

    // test capture
    move = build_capture(A3, B4, BLACK_PAWN);
    REQUIRE(move_from(move) == A3);
    REQUIRE(move_to(move) == B4);
    REQUIRE(move_captured(move) == BLACK_PAWN);
    REQUIRE(!move_flags(move));

    // test pawn double push
    move = build_pawn_double_push(A3, A5);
    REQUIRE(move_from(move) == A3);
    REQUIRE(move_to(move) == A5);
    REQUIRE(is_pawn_double_push(move));
    REQUIRE(!is_capture(move));

    // test pawn promotion
    move = build_promotion(C7, C8, WHITE_QUEEN);
    REQUIRE(move_from(move) == C7);
    REQUIRE(move_to(move) == C8);
    REQUIRE(is_promotion(move));
    REQUIRE(!is_capture(move));
    REQUIRE(move_promote_to(move) == WHITE_QUEEN);

    // test pawn promotion capture
    move = build_capture_promotion(C7, D8, BLACK_KNIGHT, WHITE_BISHOP);
    REQUIRE(move_from(move) == C7);
    REQUIRE(move_to(move) == D8);
    REQUIRE(is_promotion(move));
    REQUIRE(is_capture(move));
    REQUIRE(move_captured(move) == BLACK_KNIGHT);
    REQUIRE(move_promote_to(move) == WHITE_BISHOP);

    // test castle
    move = build_castle(KING_CASTLE);
    REQUIRE(is_castle(move));

    // test ep capture
    move = build_ep_capture(C5, D6, BLACK_PAWN);
    REQUIRE(move_from(move) == C5);
    REQUIRE(move_to(move) == D6);
    REQUIRE(is_capture(move));
    REQUIRE(is_ep_capture(move));
}
