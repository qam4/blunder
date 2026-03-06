/*
 * File:   TestTaperedEval.cpp
 *
 * Unit tests for the tapered evaluation port.
 */

#include <catch2/catch_test_macros.hpp>

#include "Tests.h"

// Access the PSQT data through the evaluator's public interface
// by constructing a HandCraftedEvaluator and testing its behavior.

TEST_CASE("tapered_eval_starting_position_phase", "[tapered-eval]")
{
    // Starting position should have phase = 128 (full middlegame)
    Board board = Parser::parse_fen(DEFAULT_FEN);
    HandCraftedEvaluator eval;

    // Disable mobility and tempo to isolate PSQT + material
    EvalConfig cfg;
    cfg.mobility_enabled = false;
    cfg.tempo_enabled = false;
    eval.set_config(cfg);

    int score = eval.evaluate(board);
    // Starting position is symmetric, so score should be 0
    REQUIRE(score == 0);
}

TEST_CASE("tapered_eval_kvk_phase", "[tapered-eval]")
{
    // K vs K should have phase = 0 (full endgame)
    Board board = Parser::parse_fen("8/8/4k3/8/8/4K3/8/8 w - - 0 1");
    HandCraftedEvaluator eval;

    EvalConfig cfg;
    cfg.mobility_enabled = false;
    cfg.tempo_enabled = false;
    eval.set_config(cfg);

    int score = eval.evaluate(board);
    // K vs K is symmetric, score should be 0
    REQUIRE(score == 0);
}

TEST_CASE("tapered_eval_default_config", "[tapered-eval]")
{
    // Default config should have all terms enabled
    HandCraftedEvaluator eval;
    REQUIRE(eval.config().mobility_enabled == true);
    REQUIRE(eval.config().tempo_enabled == true);
}

TEST_CASE("tapered_eval_tempo_sign", "[tapered-eval]")
{
    // Starting position: white to move
    Board board = Parser::parse_fen(DEFAULT_FEN);
    HandCraftedEvaluator eval;

    // With tempo enabled, mobility disabled
    EvalConfig cfg_tempo;
    cfg_tempo.mobility_enabled = false;
    cfg_tempo.tempo_enabled = true;
    eval.set_config(cfg_tempo);
    int score_with_tempo = eval.evaluate(board);

    // Without tempo
    EvalConfig cfg_no_tempo;
    cfg_no_tempo.mobility_enabled = false;
    cfg_no_tempo.tempo_enabled = false;
    eval.set_config(cfg_no_tempo);
    int score_without_tempo = eval.evaluate(board);

    // Tempo should add +28 for white to move
    REQUIRE(score_with_tempo - score_without_tempo == 28);
}

TEST_CASE("tapered_eval_white_advantage", "[tapered-eval]")
{
    // White up a queen should have positive eval
    Board board = Parser::parse_fen("rnb1kbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    HandCraftedEvaluator eval;

    EvalConfig cfg;
    cfg.mobility_enabled = false;
    cfg.tempo_enabled = false;
    eval.set_config(cfg);

    int score = eval.evaluate(board);
    REQUIRE(score > 0);
}
