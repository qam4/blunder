/*
 * File:   NNUEEvaluator.h
 *
 * NNUE (Efficiently Updatable Neural Network) evaluator using HalfKP
 * architecture: 768 → 256 → 32 → 32 → 1.
 *
 * Feature encoding: 6 piece types × 2 colors × 64 squares = 768 inputs.
 * The accumulator (layer 1 output) is incrementally updated on piece
 * add/remove, avoiding full recomputation on every move.
 */

#ifndef NNUE_EVALUATOR_H
#define NNUE_EVALUATOR_H

#include <cstdint>
#include <string>
#include <vector>

#include "Evaluator.h"
#include "Types.h"
#include "Constants.h"

class Board;

class NNUEEvaluator : public Evaluator
{
public:
    // Network architecture constants
    static constexpr int INPUT_SIZE  = 768;  // 6 piece types * 2 colors * 64 squares
    static constexpr int L1_SIZE     = 256;
    static constexpr int L2_SIZE     = 32;
    static constexpr int L3_SIZE     = 32;
    static constexpr int OUTPUT_SIZE = 1;

    // Number of non-king piece types per side used in feature encoding
    static constexpr int NUM_PIECE_TYPES = 6;  // pawn, knight, bishop, rook, queen, king
    static constexpr int NUM_COLORS      = 2;

    NNUEEvaluator();

    /// Load network weights from a binary file.
    /// Returns true on success, false on failure.
    bool load(const std::string& weights_path);

    /// Full board evaluation from white's perspective.
    int evaluate(const Board& board) override;

    /// Evaluation relative to the side to move.
    int side_relative_eval(const Board& board) override;

    // --- Incremental accumulator updates ---

    /// Save current accumulator state onto the stack (call before do_move).
    void push();

    /// Restore previous accumulator state (call on undo_move).
    void pop();

    /// Incrementally add a piece to the accumulator.
    void add_piece(U8 piece, int square);

    /// Incrementally remove a piece from the accumulator.
    void remove_piece(U8 piece, int square);

    /// Recompute the accumulator from scratch for the given board.
    void refresh(const Board& board);

    /// Whether weights have been loaded successfully.
    bool is_loaded() const { return loaded_; }

private:
    // --- Feature index computation ---

    /// Map a (piece, square) to a feature index in [0, 768).
    /// piece is one of WHITE_PAWN..BLACK_KING (2..13), excluding EMPTY.
    /// Kings are included in the feature set (piece_type_index 0..11).
    static int feature_index(U8 piece, int square);

    // --- Network forward pass helpers ---

    /// Run layers 2-4 on the concatenated accumulator and return the raw score.
    int forward(int perspective) const;

    /// Clipped ReLU: clamp to [0, max_val].
    static int16_t clipped_relu(int32_t x);

    // --- Network weights ---
    // Layer 1: INPUT_SIZE → L1_SIZE (per perspective, shared weights)
    alignas(64) int16_t l1_weights_[INPUT_SIZE * L1_SIZE];
    alignas(64) int16_t l1_biases_[L1_SIZE];

    // Layer 2: (L1_SIZE * 2) → L2_SIZE  (both perspectives concatenated)
    alignas(64) int16_t l2_weights_[L1_SIZE * 2 * L2_SIZE];
    alignas(64) int16_t l2_biases_[L2_SIZE];

    // Layer 3: L2_SIZE → L3_SIZE
    alignas(64) int16_t l3_weights_[L3_SIZE * L3_SIZE];
    alignas(64) int16_t l3_biases_[L3_SIZE];

    // Layer 4: L3_SIZE → OUTPUT_SIZE
    alignas(64) int16_t l4_weights_[L3_SIZE * OUTPUT_SIZE];
    alignas(64) int16_t l4_bias_;

    // --- Accumulator (layer 1 output) ---
    // [perspective][L1_SIZE]: white=0, black=1
    alignas(64) int16_t accumulator_[2][L1_SIZE];

    // Stack for push/pop of accumulator state during search
    struct AccumulatorEntry
    {
        alignas(64) int16_t data[2][L1_SIZE];
    };
    std::vector<AccumulatorEntry> acc_stack_;

    bool loaded_ = false;
};

#endif /* NNUE_EVALUATOR_H */
