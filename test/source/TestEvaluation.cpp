/*
 * File:   TestEvaluation.cpp
 *
 * Property 9: Evaluation symmetry under color swap.
 * evaluate(pos) == -evaluate(mirror(pos))
 * where mirror swaps white/black pieces, flips ranks, and swaps side to move.
 */

#include <catch2/catch_test_macros.hpp>

#include "Tests.h"

// Mirror a square vertically (rank flip): a1<->a8, b2<->b7, etc.
static int mirror_square(int sq)
{
    return sq ^ 56;
}

// Mirror a piece color: WHITE_PAWN <-> BLACK_PAWN, etc.
static U8 mirror_piece(U8 piece)
{
    if (piece == EMPTY)
        return EMPTY;
    return piece ^ 1;  // toggle the color bit
}

// Create a color-mirrored board from a FEN
// Swaps white/black pieces, flips ranks, swaps side to move, mirrors castling
static Board create_mirror(const Board& original)
{
    Board mirror;
    mirror.reset();

    // Place mirrored pieces
    for (int sq = 0; sq < 64; sq++)
    {
        U8 piece = original[sq];
        if (piece != EMPTY)
        {
            mirror.add_piece(mirror_piece(piece), mirror_square(sq));
        }
    }

    // Swap side to move
    mirror.set_side_to_move(original.side_to_move() ^ 1);

    // Mirror castling rights: white <-> black
    U8 orig_castle = original.castling_rights();
    U8 new_castle = 0;
    if (orig_castle & WHITE_KING_SIDE)
        new_castle |= BLACK_KING_SIDE;
    if (orig_castle & WHITE_QUEEN_SIDE)
        new_castle |= BLACK_QUEEN_SIDE;
    if (orig_castle & BLACK_KING_SIDE)
        new_castle |= WHITE_KING_SIDE;
    if (orig_castle & BLACK_QUEEN_SIDE)
        new_castle |= WHITE_QUEEN_SIDE;
    mirror.set_castling_rights(new_castle);

    // Mirror EP square
    U8 ep = original.ep_square();
    if (ep != NULL_SQUARE)
    {
        mirror.set_ep_square(static_cast<U8>(mirror_square(ep)));
    }

    mirror.set_half_move_count(original.half_move_count());
    mirror.set_full_move_count(original.full_move_count());
    mirror.update_hash();

    return mirror;
}

static const char* SYMMETRY_FENS[] = {
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    "r1bqk2r/pppp1ppp/2n2n2/2b1p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4",
    "r3k2r/ppp2ppp/2nqbn2/3pp3/3PP3/2NQBN2/PPP2PPP/R3K2R w KQkq - 6 8",
    "8/pp3ppp/2p1k3/4p3/4P3/2P1K3/PP3PPP/8 w - - 0 1",
    "r1bq1rk1/ppp2ppp/2np1n2/2b1p3/2B1P3/2NP1N2/PPP2PPP/R1BQ1RK1 w - - 0 7",
    "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1",
    // Asymmetric material
    "r1bqkb1r/pppppppp/2n2n2/8/8/5N2/PPPPPPPP/RNBQKB1R w KQkq - 2 3",
    // Endgame
    "8/5k2/8/8/8/8/2K5/8 w - - 0 1",
};
static constexpr int NUM_SYMMETRY_FENS = sizeof(SYMMETRY_FENS) / sizeof(SYMMETRY_FENS[0]);

TEST_CASE("evaluation_color_symmetry", "[evaluation]")
{
    HandCraftedEvaluator eval;

    for (int i = 0; i < NUM_SYMMETRY_FENS; i++)
    {
        Board original = Parser::parse_fen(SYMMETRY_FENS[i]);
        Board mirror = create_mirror(original);

        int orig_score = eval.evaluate(original);
        int mirror_score = eval.evaluate(mirror);

        INFO("FEN: " << SYMMETRY_FENS[i]);
        INFO("original score: " << orig_score << " mirror score: " << mirror_score);
        REQUIRE(orig_score == -mirror_score);
    }
}
