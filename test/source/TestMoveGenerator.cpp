/*
 * File:   TestMoveGenerator.cpp
 *
 */

#include <catch2/catch_test_macros.hpp>

#include "Tests.h"

TEST_CASE("move_generator_can_generate_rook_moves", "[move generator]")
{
    cout << "- Can generate rook moves" << endl;
    Board board;
    board.add_piece(WHITE_ROOK, D2);
    MoveList list;
    // check normal moves
    MoveGenerator::add_rook_moves(list, board, board.side_to_move());
    // cout << Output::board(board);
    // cout << Output::movelist(list, board);
    REQUIRE(list.length() == 14);
    REQUIRE(list.contains_valid_moves(board));
    REQUIRE(!list.contains_duplicates());
    REQUIRE(list.contains(build_move(D2, A2)));
    REQUIRE(list.contains(build_move(D2, B2)));
    REQUIRE(list.contains(build_move(D2, C2)));
    REQUIRE(list.contains(build_move(D2, E2)));
    REQUIRE(list.contains(build_move(D2, F2)));
    REQUIRE(list.contains(build_move(D2, G2)));
    REQUIRE(list.contains(build_move(D2, H2)));
    REQUIRE(list.contains(build_move(D2, D1)));
    REQUIRE(list.contains(build_move(D2, D3)));
    REQUIRE(list.contains(build_move(D2, D4)));
    REQUIRE(list.contains(build_move(D2, D5)));
    REQUIRE(list.contains(build_move(D2, D6)));
    REQUIRE(list.contains(build_move(D2, D7)));
    REQUIRE(list.contains(build_move(D2, D8)));
    list.reset();
    board.reset();

    // check black player, captures
    board.add_piece(BLACK_ROOK, D5);
    board.add_piece(WHITE_ROOK, D2);
    board.add_piece(WHITE_BISHOP, D7);
    board.set_side_to_move(BLACK);
    MoveGenerator::add_rook_moves(list, board, board.side_to_move());
    // cout << Output::board(board);
    // cout << Output::movelist(list, board);
    REQUIRE(list.length() == 12);
    REQUIRE(list.contains_valid_moves(board));
    REQUIRE(!list.contains_duplicates());
    REQUIRE(list.contains(build_move(D5, A5)));
    REQUIRE(list.contains(build_move(D5, B5)));
    REQUIRE(list.contains(build_move(D5, C5)));
    REQUIRE(list.contains(build_move(D5, E5)));
    REQUIRE(list.contains(build_move(D5, F5)));
    REQUIRE(list.contains(build_move(D5, G5)));
    REQUIRE(list.contains(build_move(D5, H5)));
    REQUIRE(list.contains(build_move(D5, D6)));
    REQUIRE(list.contains(build_move(D5, D4)));
    REQUIRE(list.contains(build_move(D5, D3)));
    REQUIRE(list.contains(build_capture(D5, D2, WHITE_ROOK)));
    REQUIRE(list.contains(build_capture(D5, D7, WHITE_BISHOP)));
    list.reset();
}

TEST_CASE("move_generator_can_generate_bishop_moves", "[move generator]")
{
    cout << "- Can generate bishop moves" << endl;
    Board board;
    board.add_piece(WHITE_BISHOP, D2);
    MoveList list;
    // check normal moves
    MoveGenerator::add_bishop_moves(list, board, WHITE);
    // cout << Output::board(board);
    // cout << Output::movelist(list, board);
    REQUIRE(list.length() == 9);
    REQUIRE(list.contains_valid_moves(board));
    REQUIRE(!list.contains_duplicates());
    REQUIRE(list.contains(build_move(D2, E1)));
    REQUIRE(list.contains(build_move(D2, E3)));
    REQUIRE(list.contains(build_move(D2, F4)));
    REQUIRE(list.contains(build_move(D2, G5)));
    REQUIRE(list.contains(build_move(D2, H6)));
    REQUIRE(list.contains(build_move(D2, C1)));
    REQUIRE(list.contains(build_move(D2, C3)));
    REQUIRE(list.contains(build_move(D2, B4)));
    REQUIRE(list.contains(build_move(D2, A5)));
    list.reset();

    // check black player
    // check captures
    board.reset();
    board.add_piece(BLACK_BISHOP, D5);
    board.add_piece(WHITE_KING, F7);
    board.add_piece(WHITE_BISHOP, B3);
    board.set_side_to_move(BLACK);
    MoveGenerator::add_bishop_moves(list, board, BLACK);
    cout << Output::board(board);
    cout << Output::movelist(list, board);
    REQUIRE(list.length() == 11);
    REQUIRE(list.contains_valid_moves(board));
    REQUIRE(!list.contains_duplicates());
    REQUIRE(list.contains(build_move(D5, E4)));
    REQUIRE(list.contains(build_move(D5, F3)));
    REQUIRE(list.contains(build_move(D5, G2)));
    REQUIRE(list.contains(build_move(D5, H1)));
    REQUIRE(list.contains(build_move(D5, C4)));
    REQUIRE(list.contains(build_capture(D5, B3, WHITE_BISHOP)));
    REQUIRE(list.contains(build_move(D5, C6)));
    REQUIRE(list.contains(build_move(D5, B7)));
    REQUIRE(list.contains(build_move(D5, A8)));
    REQUIRE(list.contains(build_move(D5, E6)));
    REQUIRE(list.contains(build_capture(D5, F7, WHITE_KING)));
    list.reset();
}

TEST_CASE("move_generator_can_generate_pawn_pushes", "[move generator]"){
    cout << "- Can generate pawn pushes" << endl;
    Board board;
    // put 8 pieces in whites starting position
    for(U8 sq = A2; sq <= H2; sq++){
        board.add_piece(WHITE_PAWN, sq);
    }
    REQUIRE(board.bitboard(WHITE_PAWN) == 0xFF00ULL);
    REQUIRE(board.bitboard(WHITE) == 0xFF00ULL);
    REQUIRE(board.bitboard(BLACK) == 0x0ULL);
    MoveList list;
    MoveGenerator::add_pawn_pushes(list, board, WHITE);
    // check 8 pushes generated and 8 double pushes
    REQUIRE(list.length() == 16);
    REQUIRE(!list.contains_duplicates());
    REQUIRE(list.contains_valid_moves(board));
    for(U8 sq = A2; sq <= H2; sq++){
        REQUIRE(list.contains(build_move(sq, sq+8)));
    }
    // put 3 pieces in random places
    board = Board();
    board.add_piece(WHITE_PAWN, A3);
    board.add_piece(WHITE_PAWN, C6);
    // put black piece in front of piece at C6
    board.add_piece(BLACK_PAWN, C7);
    // generate moves
    list.reset();
    MoveGenerator::add_pawn_pushes(list, board, WHITE);
    // check only one move: A3A4
    REQUIRE(list.length() == 1);
    REQUIRE(list.contains_valid_moves(board));
    REQUIRE(list.contains(build_move(A3, A4)));
    // repeat above with black pieces
    board = Board();
    board.add_piece(BLACK_PAWN, A6);
    board.add_piece(BLACK_PAWN, C3);
    board.add_piece(WHITE_PAWN, C2);
    list.reset();
    board.set_side_to_move(BLACK);
    MoveGenerator::add_pawn_pushes(list, board, BLACK);
    REQUIRE(list.length() == 1);
    REQUIRE(list.contains_valid_moves(board));
    REQUIRE(list.contains(build_move(A6, A5)));
}

TEST_CASE("move_generator_can_generate_pawn_double_pushes", "[move generator]"){
    cout << "- Can generate pawn double pushes" << endl;
    Board board;
    // this should move forward two
    board.add_piece(WHITE_PAWN, A2);
    // this is blocked by a white piece
    board.add_piece(WHITE_PAWN, B2);
    board.add_piece(WHITE_PAWN, B3);
    // this is block by a black piece, further forward
    board.add_piece(WHITE_PAWN, C2);
    board.add_piece(BLACK_PAWN, C4);
    // this is on the wrong row
    board.add_piece(WHITE_PAWN, D7);
    // this should move forward
    board.add_piece(WHITE_PAWN, E2);
    // check we can calculate for black too
    board.add_piece(BLACK_PAWN, E7);
    MoveList list;
    MoveGenerator::add_pawn_pushes(list, board, WHITE);
    // last two moves should be double pushes
    REQUIRE(list.length() == 10);
    REQUIRE(list.contains_valid_moves(board));
    REQUIRE(!list.contains_duplicates());
    REQUIRE(list.contains(build_pawn_double_push(E2, E4)));
    REQUIRE(list.contains(build_pawn_double_push(A2, A4)));
    list.reset();
    board.set_side_to_move(BLACK);
    MoveGenerator::add_pawn_pushes(list, board, BLACK);
    REQUIRE(list.length() == 3);
    REQUIRE(list.contains_valid_moves(board));
    REQUIRE(!list.contains_duplicates());
    REQUIRE(list.contains(build_pawn_double_push(E7, E5)));
}

TEST_CASE("move_generator_can_generate_pawn_attacks", "[move generator]"){
    cout << "- Can generate pawn attacks" << endl;
    Board board;
    board.add_piece(WHITE_PAWN, A4);
    board.add_piece(WHITE_PAWN, B4);
    board.add_piece(WHITE_PAWN, C4);
    board.add_piece(BLACK_PAWN, B5);
    board.add_piece(BLACK_PAWN, C5);
    MoveList list;
    MoveGenerator::add_pawn_attacks(list, board, WHITE);

    REQUIRE(list.length() == 3);
    REQUIRE(list.contains_valid_moves(board));
    REQUIRE(!list.contains_duplicates());
    REQUIRE(list.contains(build_capture(B4, C5, BLACK_PAWN)));
    REQUIRE(list.contains(build_capture(A4, B5, BLACK_PAWN)));
    REQUIRE(list.contains(build_capture(C4, B5, BLACK_PAWN)));
    list.reset();
    board.set_side_to_move(BLACK);
    MoveGenerator::add_pawn_attacks(list, board, BLACK);
    REQUIRE(list.length() == 3);
    REQUIRE(list.contains_valid_moves(board));
    REQUIRE(!list.contains_duplicates());
    REQUIRE(list.contains(build_capture(C5, B4, WHITE_PAWN)));
    REQUIRE(list.contains(build_capture(B5, C4, WHITE_PAWN)));
    REQUIRE(list.contains(build_capture(B5, A4, WHITE_PAWN)));
}

TEST_CASE("move_generator_can_generate_pawn_promotions", "[move generator]"){
    cout << "- Can generate pawn promotions" << endl;
    Board board;
    board.add_piece(WHITE_PAWN, C7);
    board.add_piece(BLACK_PAWN, C2);
    MoveList list;
    MoveGenerator::add_pawn_pushes(list, board, WHITE);
    REQUIRE(list.length() == 4);
    REQUIRE(list.contains_valid_moves(board));
    REQUIRE(!list.contains_duplicates());
    REQUIRE(list.contains(build_promotion(C7, C8, WHITE_QUEEN)));
    REQUIRE(list.contains(build_promotion(C7, C8, WHITE_BISHOP)));
    REQUIRE(list.contains(build_promotion(C7, C8, WHITE_ROOK)));
    REQUIRE(list.contains(build_promotion(C7, C8, WHITE_KNIGHT)));
    list.reset();
    board.set_side_to_move(BLACK);
    MoveGenerator::add_pawn_pushes(list, board, BLACK);
    REQUIRE(list.length() == 4);
    REQUIRE(list.contains_valid_moves(board));
    REQUIRE(!list.contains_duplicates());
    REQUIRE(list.contains(build_promotion(C2, C1, BLACK_QUEEN)));
    REQUIRE(list.contains(build_promotion(C2, C1, BLACK_BISHOP)));
    REQUIRE(list.contains(build_promotion(C2, C1, BLACK_ROOK)));
    REQUIRE(list.contains(build_promotion(C2, C1, BLACK_KNIGHT)));
}

TEST_CASE("move_generator_can_generate_pawn_capture_promotions", "[move generator]"){
    cout << "- Can generate pawn capture promotions" << endl;
    Board board;
    board.add_piece(WHITE_PAWN, C7);
    board.add_piece(BLACK_ROOK, C8);
    board.add_piece(BLACK_KNIGHT, B8);
    board.add_piece(BLACK_PAWN, C2);
    board.add_piece(WHITE_KNIGHT, C1);
    board.add_piece(WHITE_ROOK, D1);
    MoveList list;
    MoveGenerator::add_pawn_attacks(list, board, WHITE);
    REQUIRE(list.length() == 4);
    REQUIRE(list.contains_valid_moves(board));
    REQUIRE(!list.contains_duplicates());
    REQUIRE(list.contains(build_capture_promotion(C7, B8, BLACK_KNIGHT, WHITE_QUEEN)));
    REQUIRE(list.contains(build_capture_promotion(C7, B8, BLACK_KNIGHT, WHITE_BISHOP)));
    REQUIRE(list.contains(build_capture_promotion(C7, B8, BLACK_KNIGHT, WHITE_KNIGHT)));
    REQUIRE(list.contains(build_capture_promotion(C7, B8, BLACK_KNIGHT, WHITE_ROOK)));
    list.reset();
    board.set_side_to_move(BLACK);
    MoveGenerator::add_pawn_attacks(list, board, BLACK);
    REQUIRE(list.length() == 4);
    REQUIRE(!list.contains_duplicates());
    REQUIRE(list.contains(build_capture_promotion(C2, D1, WHITE_ROOK, BLACK_QUEEN)));
    REQUIRE(list.contains(build_capture_promotion(C2, D1, WHITE_ROOK, BLACK_KNIGHT)));
    REQUIRE(list.contains(build_capture_promotion(C2, D1, WHITE_ROOK, BLACK_ROOK)));
    REQUIRE(list.contains(build_capture_promotion(C2, D1, WHITE_ROOK, BLACK_BISHOP)));
}

TEST_CASE("move_generator_can_generate_pawn_en_passant_attacks", "[move generator]"){
    cout << "- Can generate pawn en-passant attacks" << endl;
    Board board;
    board.add_piece(BLACK_PAWN, C5);
    board.add_piece(WHITE_PAWN, D5);
    board.set_ep_square(C6);
    MoveList list;
    MoveGenerator::add_pawn_attacks(list, board, WHITE);
    cout << Output::board(board);
    cout << Output::movelist(list, board);
    REQUIRE(list.length() == 1);
    REQUIRE(list.contains_valid_moves(board));
    REQUIRE(list.contains(build_ep_capture(D5, C6, BLACK_PAWN)));
    list.reset();
    board.reset();
    // board.add_piece(WHITE_PAWN, B4);
    // board.add_piece(BLACK_PAWN, A4);
    // board.add_piece(WHITE_PAWN, H3);
    // board.set_ep_square(B3);
    // board.set_side_to_move(BLACK);
    // MoveGenerator::add_pawn_attacks(list, board, BLACK);
    // REQUIRE(list.length() == 1);
    // REQUIRE(list.contains(build_ep_capture(A4, B3, WHITE_PAWN)));
}


TEST_CASE("move_generator_can_generate_knight_moves", "[move generator]"){
    cout << "- Can generate knight moves" << endl;
    Board board;
    board.add_piece(WHITE_KNIGHT, D4);
    MoveList list;
    // check normal moves
    MoveGenerator::add_knight_moves(list, board, WHITE);
    REQUIRE(list.length() == 8);
    REQUIRE(list.contains_valid_moves(board));
    REQUIRE(!list.contains_duplicates());
    REQUIRE(list.contains(build_move(D4, B5)));
    REQUIRE(list.contains(build_move(D4, C6)));
    REQUIRE(list.contains(build_move(D4, E6)));
    REQUIRE(list.contains(build_move(D4, F5)));
    REQUIRE(list.contains(build_move(D4, B3)));
    REQUIRE(list.contains(build_move(D4, C2)));
    REQUIRE(list.contains(build_move(D4, E2)));
    REQUIRE(list.contains(build_move(D4, F3)));
    list.reset();
    // check captures
    board.add_piece(BLACK_KNIGHT, B3);
    board.set_side_to_move(BLACK);
    MoveGenerator::add_knight_moves(list, board, BLACK);
    REQUIRE(list.contains(build_capture(B3, D4, WHITE_KNIGHT)));
    list.reset();
    // check corners only have two moves
    board.reset();
    board.add_piece(WHITE_KNIGHT, A1);
    board.set_side_to_move(WHITE);
    MoveGenerator::add_knight_moves(list, board, WHITE);
    REQUIRE(list.contains(build_move(A1, B3)));
    REQUIRE(list.contains(build_move(A1, C2)));
    REQUIRE(list.length() == 2);
    REQUIRE(!list.contains_duplicates());
    board.reset();
    list.reset();
    board.add_piece(WHITE_KNIGHT, H8);
    MoveGenerator::add_knight_moves(list, board, WHITE);
    REQUIRE(list.contains(build_move(H8, F7)));
    REQUIRE(list.contains(build_move(H8, G6)));
    REQUIRE(list.length() == 2);
}

TEST_CASE("move_generator_can_generate_king_moves", "[move generator]"){
    cout << "- Can generate king moves" << endl;
    Board board;
    board.add_piece(WHITE_KING, D4);
    MoveList list;
    // check normal moves
    MoveGenerator::add_king_moves(list, board, WHITE);
    REQUIRE(list.length() == 8);
    REQUIRE(list.contains_valid_moves(board));
    REQUIRE(!list.contains_duplicates());
    REQUIRE(list.contains(build_move(D4, D5)));
    REQUIRE(list.contains(build_move(D4, D3)));
    REQUIRE(list.contains(build_move(D4, E4)));
    REQUIRE(list.contains(build_move(D4, C4)));
    REQUIRE(list.contains(build_move(D4, E5)));
    REQUIRE(list.contains(build_move(D4, C5)));
    REQUIRE(list.contains(build_move(D4, E3)));
    REQUIRE(list.contains(build_move(D4, C3)));
    list.reset();
    // check only three moves at corners, and captures
    board.reset();
    board.add_piece(BLACK_KING, A1);
    board.add_piece(WHITE_QUEEN, B2);
    board.set_side_to_move(BLACK);
    MoveGenerator::add_king_moves(list, board, BLACK);
    REQUIRE(list.contains(build_move(A1, A2)));
    REQUIRE(list.contains(build_move(A1, B1)));
    REQUIRE(list.contains(build_capture(A1, B2, WHITE_QUEEN)));
    REQUIRE(list.length() == 3);
    REQUIRE(list.contains_valid_moves(board));
    REQUIRE(!list.contains_duplicates());
    board.reset();
    board.set_side_to_move(BLACK);
    list.reset();
    board.add_piece(BLACK_KING, H8);
    board.add_piece(WHITE_QUEEN, G7);
    MoveGenerator::add_king_moves(list, board, BLACK);
    REQUIRE(list.contains(build_move(H8, G8)));
    REQUIRE(list.contains(build_move(H8, H7)));
    REQUIRE(list.contains(build_capture(H8, G7, WHITE_QUEEN)));
    REQUIRE(list.length() == 3);
    REQUIRE(!list.contains_duplicates());
    REQUIRE(list.contains_valid_moves(board));
}
