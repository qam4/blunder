/*
 * File:   DualHeadNetwork.h
 *
 * Dual-head neural network for AlphaZero-style MCTS.
 *
 * Architecture:
 *   Shared trunk:  768 -> 256 (ReLU) -> 128 (ReLU)
 *   Value head:    128 -> 32 (ReLU) -> 1 (tanh)   => output in [-1, +1]
 *   Policy head:   128 -> 4096 (linear)            => masked softmax over legal moves
 *
 * Feature encoding: same 768 HalfKP as NNUEEvaluator
 *   6 piece types x 2 colors x 64 squares = 768 binary inputs.
 *
 * Policy uses 64x64 from-to encoding (4096 outputs). For a given position,
 * only the slots corresponding to legal moves are kept; the rest are masked
 * to -infinity before softmax.
 */

#ifndef DUAL_HEAD_NETWORK_H
#define DUAL_HEAD_NETWORK_H

#include <cstdint>
#include <string>
#include <vector>

#include "Move.h"

class Board;

class DualHeadNetwork
{
public:
    // Network architecture constants
    static constexpr int INPUT_SIZE = 768;     // 6 piece types * 2 colors * 64 squares
    static constexpr int TRUNK1_OUT = 256;
    static constexpr int TRUNK2_OUT = 128;
    static constexpr int VALUE_HIDDEN = 32;
    static constexpr int POLICY_OUT = 4096;    // 64 * 64 from-to pairs

    DualHeadNetwork();

    /// Load network weights from a binary file.
    /// Layer order: trunk1 w/b, trunk2 w/b, value1 w/b, value2 w/b, policy w/b.
    /// Returns true on success.
    bool load(const std::string& weights_path);

    /// Full forward pass: returns value and policy for the given legal moves.
    /// @param board   Current position (for feature extraction)
    /// @param moves   Legal moves in this position
    /// @param policy  Output: probability for each move (same order as moves)
    /// @param value   Output: position evaluation in [-1, +1] from side-to-move
    void evaluate(const Board& board,
                  const std::vector<Move_t>& moves,
                  std::vector<float>& policy,
                  float& value);

    /// Value-only evaluation (centipawn-scale int, for Evaluator-like usage).
    int evaluate_value(const Board& board);

    /// Whether weights have been loaded successfully.
    bool is_loaded() const { return loaded_; }

private:
    /// Extract 768 binary features from the board (1.0 or 0.0).
    static void extract_features(const Board& board, float features[INPUT_SIZE]);

    /// Map (piece, square) to feature index in [0, 768).
    /// Same mapping as NNUEEvaluator::feature_index.
    static int feature_index(uint8_t piece, int square);

    /// Run the shared trunk: features[768] -> trunk_out[128].
    void forward_trunk(const float features[INPUT_SIZE],
                       float trunk_out[TRUNK2_OUT]) const;

    /// Run the value head: trunk_out[128] -> scalar in [-1, +1].
    float forward_value(const float trunk_out[TRUNK2_OUT]) const;

    /// Run the policy head: trunk_out[128] -> logits[4096].
    void forward_policy(const float trunk_out[TRUNK2_OUT],
                        float logits[POLICY_OUT]) const;

    /// Convert a move to a policy index in [0, 4096): from_sq * 64 + to_sq.
    static int move_to_policy_index(Move_t move);

    // --- Network weights (float, not quantized) ---

    // Shared trunk layer 1: INPUT_SIZE -> TRUNK1_OUT
    float w_trunk1_[INPUT_SIZE * TRUNK1_OUT];
    float b_trunk1_[TRUNK1_OUT];

    // Shared trunk layer 2: TRUNK1_OUT -> TRUNK2_OUT
    float w_trunk2_[TRUNK1_OUT * TRUNK2_OUT];
    float b_trunk2_[TRUNK2_OUT];

    // Value head layer 1: TRUNK2_OUT -> VALUE_HIDDEN
    float w_value1_[TRUNK2_OUT * VALUE_HIDDEN];
    float b_value1_[VALUE_HIDDEN];

    // Value head layer 2: VALUE_HIDDEN -> 1
    float w_value2_[VALUE_HIDDEN];
    float b_value2_;

    // Policy head: TRUNK2_OUT -> POLICY_OUT
    float w_policy_[TRUNK2_OUT * POLICY_OUT];
    float b_policy_[POLICY_OUT];

    bool loaded_ = false;
};

#endif /* DUAL_HEAD_NETWORK_H */
