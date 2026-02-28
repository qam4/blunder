/*
 * File:   TestBoardInvariance.cpp
 *
 * Tests board state invariance properties:
 * - Search doesn't modify board state
 * - do_move + undo_move is identity
 * - Incremental Zobrist hash matches full recomputation
 * - do_null_move + undo_null_move is identity
 */

#include <catch2/catch_test_macros.hpp>

#include "Output.h"
#include "Tests.h"
#include "Zobrist.h"

static const char* INVARIANCE_FENS[] = {
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    "r1bqk2r/pppp1ppp/2n2n2/2b1p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4",
    "r3k2r/ppp2ppp/2nqbn2/3pp3/3PP3/2NQBN2/PPP2PPP/R3K2R w KQkq - 6 8",
    "8/pp3ppp/2p1k3/4p3/4P3/2P1K3/PP3PPP/8 w - - 0 1",
    "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1",
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
};
static constexpr int NUM_INVARIANCE_FENS = sizeof(INVARIANCE_FENS) / sizeof(INVARIANCE_FENS[0]);

// Helper: snapshot board state for comparison
struct BoardSnapshot
{
    U64 hash;
    U8 side_to_move;
    U8 castling_rights;
    U8 ep_square;
    int half_move_count;
    int full_move_count;
    U8 pieces[64];

    static BoardSnapshot capture(const Board& board)
    {
        BoardSnapshot s;
        s.hash = board.get_hash();
        s.side_to_move = board.side_to_move();
        s.castling_rights = board.castling_rights();
        s.ep_square = board.ep_square();
        s.half_move_count = board.half_move_count();
        s.full_move_count = board.full_move_count();
        for (int i = 0; i < 64; i++)
            s.pieces[i] = board[i];
        return s;
    }

    bool operator==(const BoardSnapshot& o) const
    {
        if (hash != o.hash || side_to_move != o.side_to_move || castling_rights != o.castling_rights
            || ep_square != o.ep_square || half_move_count != o.half_move_count
            || full_move_count != o.full_move_count)
            return false;
        for (int i = 0; i < 64; i++)
            if (pieces[i] != o.pieces[i])
                return false;
        return true;
    }
};

// Property 4: Board state is identical before and after search
TEST_CASE("board_invariant_after_search", "[board invariance]")
{
    for (int i = 0; i < NUM_INVARIANCE_FENS; i++)
    {
        Board board = Parser::parse_fen(INVARIANCE_FENS[i]);
        BoardSnapshot before = BoardSnapshot::capture(board);

        Search search(board, board.get_evaluator(), board.get_tt());
        search.search(4, -1);

        BoardSnapshot after = BoardSnapshot::capture(board);

        INFO("FEN: " << INVARIANCE_FENS[i]);
        REQUIRE(before == after);
    }
}

// Property 5: do_move + undo_move is identity for all legal moves
TEST_CASE("do_undo_move_identity", "[board invariance]")
{
    for (int i = 0; i < NUM_INVARIANCE_FENS; i++)
    {
        Board board = Parser::parse_fen(INVARIANCE_FENS[i]);

        MoveList list;
        MoveGenerator::add_all_moves(list, board, board.side_to_move());
        int n = list.length();

        for (int j = 0; j < n; j++)
        {
            Move_t move = list[j];
            BoardSnapshot before = BoardSnapshot::capture(board);

            board.do_move(move);
            board.undo_move(move);

            BoardSnapshot after = BoardSnapshot::capture(board);

            INFO("FEN: " << INVARIANCE_FENS[i] << " move: " << Output::move(move, board));
            REQUIRE(before == after);
        }
    }
}

// Property 6: Incremental Zobrist hash equals full recomputation after do_move
TEST_CASE("incremental_hash_matches_recomputation", "[board invariance]")
{
    for (int i = 0; i < NUM_INVARIANCE_FENS; i++)
    {
        Board board = Parser::parse_fen(INVARIANCE_FENS[i]);

        MoveList list;
        MoveGenerator::add_all_moves(list, board, board.side_to_move());
        int n = list.length();

        for (int j = 0; j < n; j++)
        {
            Move_t move = list[j];
            board.do_move(move);

            U64 incremental_hash = board.get_hash();
            board.update_hash();
            U64 recomputed_hash = board.get_hash();

            INFO("FEN: " << INVARIANCE_FENS[i] << " move: " << Output::move(move, board));
            REQUIRE(incremental_hash == recomputed_hash);

            board.undo_move(move);
        }
    }
}

// Property 7: do_null_move + undo_null_move is identity
TEST_CASE("null_move_identity", "[board invariance]")
{
    for (int i = 0; i < NUM_INVARIANCE_FENS; i++)
    {
        Board board = Parser::parse_fen(INVARIANCE_FENS[i]);
        BoardSnapshot before = BoardSnapshot::capture(board);

        board.do_null_move();
        board.undo_null_move();

        BoardSnapshot after = BoardSnapshot::capture(board);

        INFO("FEN: " << INVARIANCE_FENS[i]);
        REQUIRE(before == after);
    }
}
