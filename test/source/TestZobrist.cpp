/*
 * File:   TestZobrist.cpp
 *
 */

#include <catch2/catch_test_macros.hpp>

#include "Tests.h"

TEST_CASE("zobrist", "[zobrist]")
{
    cout << "Testing Zobrist" << endl;

    // Blank board
    Board board = Board();
    U64 key = Zobrist::get_zobrist_key(board);
    REQUIRE(key != 0ULL);

    board.set_side_to_move(BLACK);
    REQUIRE(key != Zobrist::get_zobrist_key(board));
    board.set_side_to_move(WHITE);
    REQUIRE(key == Zobrist::get_zobrist_key(board));

    board.add_piece(WHITE_ROOK, D2);
    REQUIRE(key != Zobrist::get_zobrist_key(board));
    board.remove_piece(D2);
    REQUIRE(key == Zobrist::get_zobrist_key(board));

    board.set_castling_rights(WHITE_KING_SIDE);
    REQUIRE(key != Zobrist::get_zobrist_key(board));
    board.set_castling_rights(BLACK_KING_SIDE);
    REQUIRE(key != Zobrist::get_zobrist_key(board));
    board.set_castling_rights(BLACK_QUEEN_SIDE | WHITE_QUEEN_SIDE);
    REQUIRE(key != Zobrist::get_zobrist_key(board));
    board.set_castling_rights(FULL_CASTLING_RIGHTS);
    REQUIRE(key == Zobrist::get_zobrist_key(board));

    board.set_ep_square(D2);
    REQUIRE(key != Zobrist::get_zobrist_key(board));
    board.set_ep_square(NULL_SQUARE);
    REQUIRE(key == Zobrist::get_zobrist_key(board));
}
