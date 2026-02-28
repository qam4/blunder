/*
 * File:   Evaluator.h
 *
 * Abstract Evaluator interface and HandCraftedEvaluator implementation.
 * Extracted from Board to allow swapping evaluation strategies (e.g. NNUE).
 */

#ifndef EVALUATOR_H
#define EVALUATOR_H

class Board;

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
    int evaluate(const Board& board) override;
    int side_relative_eval(const Board& board) override;
};

#endif /* EVALUATOR_H */
