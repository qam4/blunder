/*
 * File:   TestSee.cpp
 *
 */

#include "Tests.h"

#include <catch2/catch_test_macros.hpp>

static int seeTest(string fen, string move_str)
{
    Board board = Parser::parse_fen(fen);
    auto opt = Parser::parse_san(move_str, board);
    REQUIRE(opt.has_value());
    return MoveGenerator::see(board, *opt);
}

TEST_CASE("move_generator_see_1", "[move generator see]")
{
    // https://www.chessprogramming.org/SEE_-_The_Swap_Algorithm#Position_1
    string fen = "1k1r4/1pp4p/p7/4p3/8/P5P1/1PP4P/2K1R3 w - - ; Rxe5?";
    Board board = Parser::parse_fen(fen);
    auto opt = Parser::parse_san("Rxe5", board);
    REQUIRE(opt.has_value());
    REQUIRE(MoveGenerator::see(board, *opt) == SEE_PIECE_VALUE[PAWN >> 1]);
}

TEST_CASE("move_generator_see_2", "[move generator see]")
{
    // https://www.chessprogramming.org/SEE_-_The_Swap_Algorithm#Position_2
    string fen = "1k1r3q/1ppn3p/p4b2/4p3/8/P2N2P1/1PP1R1BP/2K1Q3 w - - ; Nxe5?";
    Board board = Parser::parse_fen(fen);
    auto opt = Parser::parse_san("Nxe5", board);
    REQUIRE(opt.has_value());
    REQUIRE(MoveGenerator::see(board, *opt)
            == SEE_PIECE_VALUE[PAWN >> 1] - SEE_PIECE_VALUE[KNIGHT >> 1]);
}

TEST_CASE("move_generator_see_debug", "[move generator see]")
{
    REQUIRE(seeTest("7R/5P2/8/8/8/3K2b1/5p2/4k3 w - -", "f8=Q")
            == SEE_PIECE_VALUE[QUEEN >> 1] - SEE_PIECE_VALUE[PAWN >> 1]);
}

TEST_CASE("move_generator_see_3", "[move generator see]")
{
    REQUIRE(seeTest("4R3/2r3p1/5bk1/1p1r3p/p2PR1P1/P1BK1P2/1P6/8 b - -", "hxg4") == 0);
    REQUIRE(seeTest("4R3/2r3p1/5bk1/1p1r1p1p/p2PR1P1/P1BK1P2/1P6/8 b - -", "hxg4") == 0);
    REQUIRE(seeTest("4r1k1/5pp1/nbp4p/1p2p2q/1P2P1b1/1BP2N1P/1B2QPPK/3R4 b - -", "Bxf3")
            == SEE_PIECE_VALUE[KNIGHT >> 1] - SEE_PIECE_VALUE[BISHOP >> 1]);
    REQUIRE(seeTest("2r1r1k1/pp1bppbp/3p1np1/q3P3/2P2P2/1P2B3/P1N1B1PP/2RQ1RK1 b - -", "dxe5")
            == SEE_PIECE_VALUE[PAWN >> 1]);
    REQUIRE(seeTest("7r/5qpk/p1Qp1b1p/3r3n/BB3p2/5p2/P1P2P2/4RK1R w - -", "Re8") == 0);
    REQUIRE(seeTest("6rr/6pk/p1Qp1b1p/2n5/1B3p2/5p2/P1P2P2/4RK1R w - -", "Re8")
            == -SEE_PIECE_VALUE[ROOK >> 1]);
    REQUIRE(seeTest("7r/5qpk/2Qp1b1p/1N1r3n/BB3p2/5p2/P1P2P2/4RK1R w - -", "Re8")
            == -SEE_PIECE_VALUE[ROOK >> 1]);
    // promotions
    REQUIRE(seeTest("6RR/4bP2/8/8/5r2/3K4/5p2/4k3 w - -", "f8=Q")
            == SEE_PIECE_VALUE[BISHOP >> 1] - SEE_PIECE_VALUE[PAWN >> 1]);
    REQUIRE(seeTest("6RR/4bP2/8/8/5r2/3K4/5p2/4k3 w - -", "f8=N")
            == SEE_PIECE_VALUE[KNIGHT >> 1] - SEE_PIECE_VALUE[PAWN >> 1]);
    // Change r to b, otherwise K in check
    REQUIRE(seeTest("7R/5P2/8/8/8/3K2b1/5p2/4k3 w - -", "f8=Q")
            == SEE_PIECE_VALUE[QUEEN >> 1] - SEE_PIECE_VALUE[PAWN >> 1]);
    REQUIRE(seeTest("7R/5P2/8/8/8/3K2b1/5p2/4k3 w - -", "f8=B")
            == SEE_PIECE_VALUE[BISHOP >> 1] - SEE_PIECE_VALUE[PAWN >> 1]);
    REQUIRE(seeTest("7R/4bP2/8/8/1q6/3K4/5p2/4k3 w - -", "f8=R") == -SEE_PIECE_VALUE[PAWN >> 1]);
    REQUIRE(seeTest("8/4kp2/2npp3/1Nn5/1p2PQP1/7q/1PP1B3/4KR1r b - -", "Rxf1+") == 0);
    REQUIRE(seeTest("8/4kp2/2npp3/1Nn5/1p2P1P1/7q/1PP1B3/4KR1r b - -", "Rxf1+") == 0);
    REQUIRE(seeTest("2r2r1k/6bp/p7/2q2p1Q/3PpP2/1B6/P5PP/2RR3K b - -", "Qxc1")
            == 2 * SEE_PIECE_VALUE[ROOK >> 1] - SEE_PIECE_VALUE[QUEEN >> 1]);
    REQUIRE(seeTest("r2qk1nr/pp2ppbp/2b3p1/2p1p3/8/2N2N2/PPPP1PPP/R1BQR1K1 w kq -", "Nxe5")
            == SEE_PIECE_VALUE[PAWN >> 1]);
    REQUIRE(seeTest("6r1/4kq2/b2p1p2/p1pPb3/p1P2B1Q/2P4P/2B1R1P1/6K1 w - -", "Bxe5") == 0);
    // en-passant
    REQUIRE(seeTest("3q2nk/pb1r1p2/np6/3P2Pp/2p1P3/2R4B/PQ3P1P/3R2K1 w - h6", "gxh6") == 0);
    REQUIRE(seeTest("3q2nk/pb1r1p2/np6/3P2Pp/2p1P3/2R1B2B/PQ3P1P/3R2K1 w - h6", "gxh6")
            == SEE_PIECE_VALUE[PAWN >> 1]);
    REQUIRE(seeTest("2r4r/1P4pk/p2p1b1p/7n/BB3p2/2R2p2/P1P2P2/4RK2 w - -", "Rxc8")
            == SEE_PIECE_VALUE[ROOK >> 1]);
    // promotion+capture
    REQUIRE(seeTest("2r5/1P4pk/p2p1b1p/5b1n/BB3p2/2R2p2/P1P2P2/4RK2 w - -", "Rxc8")
            == SEE_PIECE_VALUE[ROOK >> 1]);
    REQUIRE(seeTest("2r4k/2r4p/p7/2b2p1b/4pP2/1BR5/P1R3PP/2Q4K w - -", "Rxc5")
            == SEE_PIECE_VALUE[BISHOP >> 1]);
    REQUIRE(seeTest("8/pp6/2pkp3/4bp2/2R3b1/2P5/PP4B1/1K6 w - -", "Bxc6")
            == SEE_PIECE_VALUE[PAWN >> 1] - SEE_PIECE_VALUE[BISHOP >> 1]);
    REQUIRE(seeTest("4q3/1p1pr1k1/1B2rp2/6p1/p3PP2/P3R1P1/1P2R1K1/4Q3 b - -", "Rxe4")
            == SEE_PIECE_VALUE[PAWN >> 1] - SEE_PIECE_VALUE[ROOK >> 1]);
    REQUIRE(seeTest("4q3/1p1pr1kb/1B2rp2/6p1/p3PP2/P3R1P1/1P2R1K1/4Q3 b - -", "Bxe4")
            == SEE_PIECE_VALUE[PAWN >> 1]);
    // TODO: black promotions
}

// Additional SEE example-based tests for specific capture categories

TEST_CASE("see_undefended_capture", "[move generator see]")
{
    // Rook on e2 captures undefended pawn on e5: full pawn value
    REQUIRE(seeTest("4k3/8/8/4p3/8/8/4R3/4K3 w - -", "Rxe5") == SEE_PIECE_VALUE[PAWN >> 1]);
    // Queen on b2 captures undefended knight on e5 (diagonal b2-e5): full knight value
    REQUIRE(seeTest("4k3/8/8/4n3/8/8/1Q6/4K3 w - -", "Qxe5") == SEE_PIECE_VALUE[KNIGHT >> 1]);
    // Bishop on h2 captures undefended rook on e5 (diagonal h2-e5): full rook value
    REQUIRE(seeTest("4k3/8/8/4r3/8/8/7B/4K3 w - -", "Bxe5") == SEE_PIECE_VALUE[ROOK >> 1]);
}

TEST_CASE("see_equally_defended_capture", "[move generator see]")
{
    // Knight captures pawn on e5 defended by pawn on d6: pawn - knight (losing trade)
    REQUIRE(seeTest("4k3/8/3p4/4p3/8/3N4/8/4K3 w - -", "Nxe5")
            == SEE_PIECE_VALUE[PAWN >> 1] - SEE_PIECE_VALUE[KNIGHT >> 1]);
    // Queen on b2 captures undefended queen on e5 (diagonal): full queen value
    REQUIRE(seeTest("4k3/8/8/4q3/8/8/1Q6/4K3 w - -", "Qxe5") == SEE_PIECE_VALUE[QUEEN >> 1]);
}

// Property 8: Quiescence search has no side effects on board state
TEST_CASE("quiesce_no_side_effects", "[board invariance]")
{
    static const char* QUIESCE_FENS[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r1bqk2r/pppp1ppp/2n2n2/2b1p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4",
        "r3k2r/ppp2ppp/2nqbn2/3pp3/3PP3/2NQBN2/PPP2PPP/R3K2R w KQkq - 6 8",
        "8/pp3ppp/2p1k3/4p3/4P3/2P1K3/PP3PPP/8 w - - 0 1",
        // Tactical position with many captures available
        "r1bqk2r/ppp2ppp/2n5/3np1N1/2B5/8/PPPP1PPP/RNBQK2R w KQkq - 0 6",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
    };

    for (const auto* fen : QUIESCE_FENS)
    {
        Board board = Parser::parse_fen(fen);
        U64 hash_before = board.get_hash();
        U8 stm_before = board.side_to_move();
        U8 castle_before = board.castling_rights();
        U8 ep_before = board.ep_square();
        int hmc_before = board.half_move_count();

        // Capture board array
        U8 pieces_before[64];
        for (int i = 0; i < 64; i++)
            pieces_before[i] = board[i];

        Search search(board, board.get_evaluator(), board.get_tt());
        board.set_search_ply(0);
        search.quiesce(-MAX_SCORE, MAX_SCORE);

        INFO("FEN: " << fen);
        REQUIRE(board.get_hash() == hash_before);
        REQUIRE(board.side_to_move() == stm_before);
        REQUIRE(board.castling_rights() == castle_before);
        REQUIRE(board.ep_square() == ep_before);
        REQUIRE(board.half_move_count() == hmc_before);
        for (int i = 0; i < 64; i++)
        {
            INFO("square: " << i);
            REQUIRE(board[i] == pieces_before[i]);
        }
    }
}
