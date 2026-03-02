/*
 * File:   NNUEEvaluator.cpp
 *
 * NNUE evaluator implementation: forward pass, incremental accumulator
 * updates, and weight loading from binary file.
 */

#include <algorithm>
#include <cstring>
#include <fstream>

#include "NNUEEvaluator.h"

#include "Board.h"

// ---------------------------------------------------------------------------
// Feature index: maps (piece, square) → [0, 768)
// piece is WHITE_PAWN(2)..BLACK_KING(13), giving piece_type_index 0..11.
// ---------------------------------------------------------------------------
int NNUEEvaluator::feature_index(U8 piece, int square)
{
    int piece_type_index = piece - 2;  // 0..11
    return piece_type_index * 64 + square;
}

// ---------------------------------------------------------------------------
// Constructor — zero-initialise everything
// ---------------------------------------------------------------------------
NNUEEvaluator::NNUEEvaluator()
    : l1_biases_ {}
    , l2_biases_ {}
    , l3_biases_ {}
    , l4_bias_ { 0 }
    , accumulator_ {}
    , loaded_ { false }
{
    std::memset(l1_weights_, 0, sizeof(l1_weights_));
    std::memset(l2_weights_, 0, sizeof(l2_weights_));
    std::memset(l3_weights_, 0, sizeof(l3_weights_));
    std::memset(l4_weights_, 0, sizeof(l4_weights_));
}

// ---------------------------------------------------------------------------
// load — read network weights from a binary file
// ---------------------------------------------------------------------------
bool NNUEEvaluator::load(const std::string& weights_path)
{
    std::ifstream file(weights_path, std::ios::binary);
    if (!file.is_open())
    {
        return false;
    }

    // Read weights in order matching the network architecture
    auto read = [&](void* dst, std::size_t bytes) -> bool
    {
        file.read(reinterpret_cast<char*>(dst), static_cast<std::streamsize>(bytes));
        return file.good();
    };

    if (!read(l1_weights_, sizeof(l1_weights_)))
        return false;
    if (!read(l1_biases_, sizeof(l1_biases_)))
        return false;
    if (!read(l2_weights_, sizeof(l2_weights_)))
        return false;
    if (!read(l2_biases_, sizeof(l2_biases_)))
        return false;
    if (!read(l3_weights_, sizeof(l3_weights_)))
        return false;
    if (!read(l3_biases_, sizeof(l3_biases_)))
        return false;
    if (!read(l4_weights_, sizeof(l4_weights_)))
        return false;
    if (!read(&l4_bias_, sizeof(l4_bias_)))
        return false;

    loaded_ = true;
    return true;
}

// ---------------------------------------------------------------------------
// refresh — recompute accumulators from scratch for the given board
// ---------------------------------------------------------------------------
void NNUEEvaluator::refresh(const Board& board)
{
    // Reset both perspectives to biases
    std::memcpy(accumulator_[0], l1_biases_, sizeof(l1_biases_));
    std::memcpy(accumulator_[1], l1_biases_, sizeof(l1_biases_));

    for (int sq = 0; sq < 64; ++sq)
    {
        U8 piece = board[sq];
        if (piece == EMPTY)
        {
            continue;
        }

        // White perspective: feature_index(piece, square)
        int w_idx = feature_index(piece, sq);
        // Black perspective: mirror piece color and square vertically
        int b_idx = feature_index(static_cast<U8>(piece ^ 1), sq ^ 56);

        for (int j = 0; j < L1_SIZE; ++j)
        {
            accumulator_[0][j] += l1_weights_[w_idx * L1_SIZE + j];
            accumulator_[1][j] += l1_weights_[b_idx * L1_SIZE + j];
        }
    }
}

// ---------------------------------------------------------------------------
// add_piece — incrementally add a piece to both accumulator perspectives
// ---------------------------------------------------------------------------
void NNUEEvaluator::add_piece(U8 piece, int square)
{
    int w_idx = feature_index(piece, square);
    int b_idx = feature_index(static_cast<U8>(piece ^ 1), square ^ 56);

    for (int j = 0; j < L1_SIZE; ++j)
    {
        accumulator_[0][j] += l1_weights_[w_idx * L1_SIZE + j];
        accumulator_[1][j] += l1_weights_[b_idx * L1_SIZE + j];
    }
}

// ---------------------------------------------------------------------------
// remove_piece — incrementally remove a piece from both accumulator perspectives
// ---------------------------------------------------------------------------
void NNUEEvaluator::remove_piece(U8 piece, int square)
{
    int w_idx = feature_index(piece, square);
    int b_idx = feature_index(static_cast<U8>(piece ^ 1), square ^ 56);

    for (int j = 0; j < L1_SIZE; ++j)
    {
        accumulator_[0][j] -= l1_weights_[w_idx * L1_SIZE + j];
        accumulator_[1][j] -= l1_weights_[b_idx * L1_SIZE + j];
    }
}

// ---------------------------------------------------------------------------
// push — save current accumulator state onto the stack
// ---------------------------------------------------------------------------
void NNUEEvaluator::push()
{
    AccumulatorEntry entry;
    std::memcpy(entry.data, accumulator_, sizeof(accumulator_));
    acc_stack_.push_back(entry);
}

// ---------------------------------------------------------------------------
// pop — restore accumulator state from the stack
// ---------------------------------------------------------------------------
void NNUEEvaluator::pop()
{
    std::memcpy(accumulator_, acc_stack_.back().data, sizeof(accumulator_));
    acc_stack_.pop_back();
}

// ---------------------------------------------------------------------------
// clipped_relu — clamp to [0, 127]
// ---------------------------------------------------------------------------
int16_t NNUEEvaluator::clipped_relu(int32_t x)
{
    return static_cast<int16_t>(std::max(0, std::min(static_cast<int>(x), 127)));
}

// ---------------------------------------------------------------------------
// forward — run layers 2-4 on the concatenated accumulator
// ---------------------------------------------------------------------------
int NNUEEvaluator::forward(int perspective) const
{
    static constexpr int CONCAT_SIZE = L1_SIZE * 2;  // 512
    static constexpr int QUANT_FACTOR = 64;

    // Layer 1 output: concatenate own perspective first, opponent second
    int16_t l1_output[CONCAT_SIZE];
    for (int i = 0; i < L1_SIZE; ++i)
    {
        l1_output[i] = clipped_relu(accumulator_[perspective][i]);
        l1_output[L1_SIZE + i] = clipped_relu(accumulator_[perspective ^ 1][i]);
    }

    // Layer 2: (L1_SIZE * 2) → L2_SIZE
    int16_t l2_output[L2_SIZE];
    for (int i = 0; i < L2_SIZE; ++i)
    {
        int32_t sum = static_cast<int32_t>(l2_biases_[i]);
        for (int j = 0; j < CONCAT_SIZE; ++j)
        {
            sum += static_cast<int32_t>(l2_weights_[i * CONCAT_SIZE + j])
                * static_cast<int32_t>(l1_output[j]);
        }
        // Scale down intermediate result to stay in int16 range after ReLU
        l2_output[i] = clipped_relu(sum / 128);
    }

    // Layer 3: L2_SIZE → L3_SIZE
    int16_t l3_output[L3_SIZE];
    for (int i = 0; i < L3_SIZE; ++i)
    {
        int32_t sum = static_cast<int32_t>(l3_biases_[i]);
        for (int j = 0; j < L2_SIZE; ++j)
        {
            sum += static_cast<int32_t>(l3_weights_[i * L2_SIZE + j])
                * static_cast<int32_t>(l2_output[j]);
        }
        l3_output[i] = clipped_relu(sum / 128);
    }

    // Layer 4: L3_SIZE → 1 (dot product)
    int32_t raw_score = static_cast<int32_t>(l4_bias_);
    for (int i = 0; i < L3_SIZE; ++i)
    {
        raw_score += static_cast<int32_t>(l4_weights_[i]) * static_cast<int32_t>(l3_output[i]);
    }

    return static_cast<int>(raw_score / QUANT_FACTOR);
}

// ---------------------------------------------------------------------------
// evaluate — full board evaluation from white's perspective
// ---------------------------------------------------------------------------
int NNUEEvaluator::evaluate(const Board& /*board*/)
{
    if (!loaded_)
    {
        return 0;
    }
    return forward(0);
}

// ---------------------------------------------------------------------------
// side_relative_eval — evaluation relative to the side to move
// ---------------------------------------------------------------------------
int NNUEEvaluator::side_relative_eval(const Board& board)
{
    if (!loaded_)
    {
        return 0;
    }
    int perspective = (board.side_to_move() == WHITE) ? 0 : 1;
    return forward(perspective);
}
