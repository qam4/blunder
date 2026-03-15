/*
 * File:   Evaluator.h
 *
 * Abstract Evaluator interface and HandCraftedEvaluator implementation.
 * Extracted from Board to allow swapping evaluation strategies (e.g. NNUE).
 */

#ifndef EVALUATOR_H
#define EVALUATOR_H

#include <vector>

#include "Types.h"

class Board;

struct PawnHashEntry
{
    U64 key;
    int mg_score;
    int eg_score;
};

struct EvalConfig
{
    bool mobility_enabled = true;
    bool tempo_enabled = true;
    bool pawn_structure_enabled = true;
    bool king_safety_enabled = true;
    bool piece_bonuses_enabled = true;
};

class Evaluator
{
public:
    virtual ~Evaluator() = default;
    virtual int evaluate(const Board& board) = 0;
    virtual int side_relative_eval(const Board& board) = 0;
};

class HandCraftedEvaluator : public Evaluator
{
public:
    HandCraftedEvaluator();

    int evaluate(const Board& board) override;
    int side_relative_eval(const Board& board) override;

    // Runtime configuration
    void set_config(const EvalConfig& cfg) { config_ = cfg; }
    const EvalConfig& config() const { return config_; }

    // Sub-score accessors for coaching protocol
    int get_pawn_structure_score() const { return last_pawn_structure_; }
    int get_king_safety_score() const { return last_king_safety_; }
    int get_mobility_score() const { return last_mobility_; }
    int get_piece_bonuses_score() const { return last_piece_bonuses_; }

private:
    void psqt_init();
    int phase(const Board& board) const;
    int eval_pawn_structure(const Board& board, int phase);
    int eval_king_safety(const Board& board, int phase);
    int eval_mobility(const Board& board, int phase);
    int eval_piece_bonuses(const Board& board, int phase);

    // Expanded table: piece_square_table_[phase][piece][square]
    int piece_square_table_[2][14][64];

    // Pawn hash table
    static constexpr int PAWN_HASH_SIZE = 16384;
    std::vector<PawnHashEntry> pawn_hash_{PAWN_HASH_SIZE};

    // Cached sub-scores from last evaluate() call
    int last_pawn_structure_ = 0;
    int last_king_safety_ = 0;
    int last_mobility_ = 0;
    int last_piece_bonuses_ = 0;

    EvalConfig config_;
};

#endif /* EVALUATOR_H */
