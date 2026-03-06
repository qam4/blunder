/*
 * File:   Evaluator.h
 *
 * Abstract Evaluator interface and HandCraftedEvaluator implementation.
 * Extracted from Board to allow swapping evaluation strategies (e.g. NNUE).
 */

#ifndef EVALUATOR_H
#define EVALUATOR_H

class Board;

struct EvalConfig
{
    bool mobility_enabled = true;
    bool tempo_enabled = true;
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

private:
    void psqt_init();
    int phase(const Board& board) const;

    // Expanded table: piece_square_table_[phase][piece][square]
    int piece_square_table_[2][14][64];

    EvalConfig config_;
};

#endif /* EVALUATOR_H */
