/*
 * File:   TestNNUE.cpp
 *
 * Unit tests for the NNUE evaluator: loading, evaluation, incremental
 * accumulator updates, push/pop, and fallback to hand-crafted evaluator.
 *
 * Validates: Requirements 29.1, 29.2, 29.3
 */

#include <cstdio>
#include <fstream>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "Board.h"
#include "MoveGenerator.h"
#include "MoveList.h"
#include "NNUEEvaluator.h"
#include "Parser.h"

// ---------------------------------------------------------------------------
// Helper: generate a temporary binary weights file with deterministic values.
// The file layout must match NNUEEvaluator::load() exactly.
// Returns the file path.  Caller should std::remove() it when done.
// ---------------------------------------------------------------------------
static std::string generate_test_weights()
{
    std::string path = "test_nnue_weights.bin";
    std::ofstream f(path, std::ios::binary);

    auto write_val = [&](int16_t val)
    { f.write(reinterpret_cast<const char*>(&val), sizeof(val)); };

    // The forward pass applies clipped_relu (clamp [0,127]) and divides by
    // 128 between layers.  To get a non-zero final score we need the
    // intermediate sums to be large enough to survive the divisions.
    //
    // Strategy: use positive biases large enough to survive /128 after ReLU,
    // and small positive weights so the accumulator values stay in range.

    // L1 weights: INPUT_SIZE(768) * L1_SIZE(256)
    // Small positive values — each active piece adds a little to each neuron.
    for (int i = 0; i < 768 * 256; ++i)
        write_val(static_cast<int16_t>(1));
    // L1 biases: 256  — start each accumulator neuron at a positive baseline
    for (int i = 0; i < 256; ++i)
        write_val(static_cast<int16_t>(64));

    // L2 weights: (L1_SIZE*2=512) * L2_SIZE(32)
    // Positive weights so the product with clipped_relu output stays positive.
    for (int i = 0; i < 512 * 32; ++i)
        write_val(static_cast<int16_t>(1));
    // L2 biases: 32  — large enough so sum/128 > 0 after ReLU
    for (int i = 0; i < 32; ++i)
        write_val(static_cast<int16_t>(500));

    // L3 weights: L3_SIZE(32) * L3_SIZE(32)
    for (int i = 0; i < 32 * 32; ++i)
        write_val(static_cast<int16_t>(1));
    // L3 biases: 32
    for (int i = 0; i < 32; ++i)
        write_val(static_cast<int16_t>(500));

    // L4 weights: L3_SIZE(32)
    for (int i = 0; i < 32; ++i)
        write_val(static_cast<int16_t>(1));
    // L4 bias: 1
    write_val(static_cast<int16_t>(100));

    f.close();
    return path;
}

// ===========================================================================
// Test 1: is_loaded returns false before loading
// ===========================================================================
TEST_CASE("NNUE is_loaded returns false before loading", "[nnue]")
{
    NNUEEvaluator nnue;
    REQUIRE_FALSE(nnue.is_loaded());
}

// ===========================================================================
// Test 2: load returns false for nonexistent file
// ===========================================================================
TEST_CASE("NNUE load returns false for nonexistent file", "[nnue]")
{
    NNUEEvaluator nnue;
    REQUIRE_FALSE(nnue.load("nonexistent_weights_file.bin"));
    REQUIRE_FALSE(nnue.is_loaded());
}

// ===========================================================================
// Test 3: evaluate returns 0 when not loaded
// ===========================================================================
TEST_CASE("NNUE evaluate returns 0 when not loaded", "[nnue]")
{
    NNUEEvaluator nnue;
    Board board = Parser::parse_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    REQUIRE(nnue.evaluate(board) == 0);
    REQUIRE(nnue.side_relative_eval(board) == 0);
}

// ===========================================================================
// Test 4: load succeeds with valid weights file
// ===========================================================================
TEST_CASE("NNUE load succeeds with valid weights file", "[nnue]")
{
    auto path = generate_test_weights();
    NNUEEvaluator nnue;
    REQUIRE(nnue.load(path));
    REQUIRE(nnue.is_loaded());
    std::remove(path.c_str());
}

// ===========================================================================
// Test 5: evaluate returns non-zero with loaded weights
// ===========================================================================
TEST_CASE("NNUE evaluate returns non-zero with loaded weights", "[nnue]")
{
    auto path = generate_test_weights();
    NNUEEvaluator nnue;
    REQUIRE(nnue.load(path));

    Board board = Parser::parse_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    nnue.refresh(board);

    // With non-zero weights and 32 pieces on the board the score should
    // be non-zero (the exact value depends on the synthetic weights).
    REQUIRE(nnue.evaluate(board) != 0);

    std::remove(path.c_str());
}

// ===========================================================================
// Test 6: incremental update matches full refresh
// ===========================================================================
TEST_CASE("NNUE incremental update matches full refresh", "[nnue]")
{
    auto path = generate_test_weights();

    // Primary evaluator wired to the board — uses push/refresh via do_move
    NNUEEvaluator nnue;
    REQUIRE(nnue.load(path));

    Board board = Parser::parse_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    board.set_nnue(&nnue);
    nnue.refresh(board);

    // Generate all legal moves from the starting position
    MoveList list;
    MoveGenerator::add_all_moves(list, board, board.side_to_move());

    for (int i = 0; i < list.length(); ++i)
    {
        Move_t move = list[i];

        // do_move calls nnue.push() then nnue.refresh()
        board.do_move(move);

        int score_incremental = nnue.evaluate(board);

        // Build a fresh evaluator and recompute from scratch on the same
        // board state to get the reference score.
        NNUEEvaluator fresh;
        REQUIRE(fresh.load(path));
        fresh.refresh(board);
        int score_fresh = fresh.evaluate(board);

        REQUIRE(score_incremental == score_fresh);

        board.undo_move(move);
    }

    board.set_nnue(nullptr);
    std::remove(path.c_str());
}

// ===========================================================================
// Test 7: push/pop restores accumulator state
// ===========================================================================
TEST_CASE("NNUE push/pop restores accumulator state", "[nnue]")
{
    auto path = generate_test_weights();

    NNUEEvaluator nnue;
    REQUIRE(nnue.load(path));

    Board board = Parser::parse_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    board.set_nnue(&nnue);
    nnue.refresh(board);

    int score_before = nnue.evaluate(board);

    // Pick the first legal move
    MoveList list;
    MoveGenerator::add_all_moves(list, board, board.side_to_move());
    REQUIRE(list.length() > 0);

    Move_t move = list[0];

    // do_move pushes accumulator, undo_move pops it
    board.do_move(move);
    board.undo_move(move);

    int score_after = nnue.evaluate(board);
    REQUIRE(score_before == score_after);

    board.set_nnue(nullptr);
    std::remove(path.c_str());
}

// ===========================================================================
// Test 8: fallback — get_evaluator returns HandCrafted when no NNUE
// ===========================================================================
TEST_CASE("NNUE fallback: get_evaluator returns HandCrafted when no NNUE", "[nnue]")
{
    Board board = Parser::parse_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    // No NNUE set — should use HandCraftedEvaluator
    int hc_score = board.get_evaluator().evaluate(board);

    // The starting position is symmetric so the hand-crafted eval should
    // return 0 (or very close to it).  Just verify we got a value without
    // crashing and that the evaluator is indeed the hand-crafted one.
    // A dynamic_cast confirms the concrete type.
    Evaluator& eval = board.get_evaluator();
    REQUIRE(dynamic_cast<HandCraftedEvaluator*>(&eval) != nullptr);
    (void)hc_score;  // suppress unused warning
}

// ===========================================================================
// Test 9: fallback — get_evaluator returns NNUE when loaded
// ===========================================================================
TEST_CASE("NNUE fallback: get_evaluator returns NNUE when loaded", "[nnue]")
{
    auto path = generate_test_weights();

    NNUEEvaluator nnue;
    REQUIRE(nnue.load(path));

    Board board = Parser::parse_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    board.set_nnue(&nnue);
    nnue.refresh(board);

    // get_evaluator should now return the NNUE evaluator
    Evaluator& eval = board.get_evaluator();
    REQUIRE(dynamic_cast<NNUEEvaluator*>(&eval) != nullptr);

    // The NNUE score should differ from the hand-crafted score
    int nnue_score = eval.evaluate(board);
    HandCraftedEvaluator hc;
    int hc_score = hc.evaluate(board);
    // They use completely different evaluation logic, so they should differ
    // (unless by extreme coincidence, which won't happen with our synthetic
    // weights).
    REQUIRE(nnue_score != hc_score);

    board.set_nnue(nullptr);
    std::remove(path.c_str());
}
