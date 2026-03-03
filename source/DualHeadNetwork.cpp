/*
 * File:   DualHeadNetwork.cpp
 *
 * Dual-head neural network implementation: shared trunk, value head,
 * policy head, feature extraction, and weight loading.
 */

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <numeric>

#include "DualHeadNetwork.h"

#include "Board.h"

// ---------------------------------------------------------------------------
// Constructor — zero-initialise all weights
// ---------------------------------------------------------------------------
DualHeadNetwork::DualHeadNetwork()
    : b_trunk1_ {}
    , b_trunk2_ {}
    , b_value1_ {}
    , b_value2_(0.0F)
    , b_policy_ {}
    , loaded_(false)
{
    std::memset(w_trunk1_, 0, sizeof(w_trunk1_));
    std::memset(w_trunk2_, 0, sizeof(w_trunk2_));
    std::memset(w_value1_, 0, sizeof(w_value1_));
    std::memset(w_value2_, 0, sizeof(w_value2_));
    std::memset(w_policy_, 0, sizeof(w_policy_));
}

// ---------------------------------------------------------------------------
// feature_index — map (piece, square) to [0, 768)
// Piece constants are interleaved: WHITE_PAWN=2, BLACK_PAWN=3, ...
// piece_type_index = piece - 2 gives 0..11 (12 piece-color combos).
// ---------------------------------------------------------------------------
int DualHeadNetwork::feature_index(uint8_t piece, int square)
{
    int piece_type_index = piece - 2;  // 0..11
    return piece_type_index * 64 + square;
}

// ---------------------------------------------------------------------------
// extract_features — build 768-element binary feature vector from board
// ---------------------------------------------------------------------------
void DualHeadNetwork::extract_features(const Board& board, float features[INPUT_SIZE])
{
    std::memset(features, 0, sizeof(float) * INPUT_SIZE);

    for (int sq = 0; sq < 64; ++sq)
    {
        U8 piece = board[sq];
        if (piece == EMPTY)
        {
            continue;
        }
        int idx = feature_index(piece, sq);
        assert(idx >= 0 && idx < INPUT_SIZE);
        features[idx] = 1.0F;
    }
}

// ---------------------------------------------------------------------------
// move_to_policy_index — from_sq * 64 + to_sq
// ---------------------------------------------------------------------------
int DualHeadNetwork::move_to_policy_index(Move_t move)
{
    return move.from() * 64 + move.to();
}

// ---------------------------------------------------------------------------
// forward_trunk — shared representation: 768 -> 256 (ReLU) -> 128 (ReLU)
// ---------------------------------------------------------------------------
void DualHeadNetwork::forward_trunk(const float features[INPUT_SIZE],
                                    float trunk_out[TRUNK2_OUT]) const
{
    // Layer 1: 768 -> 256 with ReLU
    float l1_out[TRUNK1_OUT];
    for (int i = 0; i < TRUNK1_OUT; ++i)
    {
        float sum = b_trunk1_[i];
        for (int j = 0; j < INPUT_SIZE; ++j)
        {
            sum += w_trunk1_[i * INPUT_SIZE + j] * features[j];
        }
        l1_out[i] = std::max(0.0F, sum);  // ReLU
    }

    // Layer 2: 256 -> 128 with ReLU
    for (int i = 0; i < TRUNK2_OUT; ++i)
    {
        float sum = b_trunk2_[i];
        for (int j = 0; j < TRUNK1_OUT; ++j)
        {
            sum += w_trunk2_[i * TRUNK1_OUT + j] * l1_out[j];
        }
        trunk_out[i] = std::max(0.0F, sum);  // ReLU
    }
}

// ---------------------------------------------------------------------------
// forward_value — value head: 128 -> 32 (ReLU) -> 1 (tanh)
// ---------------------------------------------------------------------------
float DualHeadNetwork::forward_value(const float trunk_out[TRUNK2_OUT]) const
{
    // Hidden layer: 128 -> 32 with ReLU
    float hidden[VALUE_HIDDEN];
    for (int i = 0; i < VALUE_HIDDEN; ++i)
    {
        float sum = b_value1_[i];
        for (int j = 0; j < TRUNK2_OUT; ++j)
        {
            sum += w_value1_[i * TRUNK2_OUT + j] * trunk_out[j];
        }
        hidden[i] = std::max(0.0F, sum);  // ReLU
    }

    // Output: 32 -> 1 with tanh
    float raw = b_value2_;
    for (int i = 0; i < VALUE_HIDDEN; ++i)
    {
        raw += w_value2_[i] * hidden[i];
    }
    return std::tanh(raw);
}

// ---------------------------------------------------------------------------
// forward_policy — policy head: 128 -> 4096 (linear, no activation)
// ---------------------------------------------------------------------------
void DualHeadNetwork::forward_policy(const float trunk_out[TRUNK2_OUT],
                                     float logits[POLICY_OUT]) const
{
    for (int i = 0; i < POLICY_OUT; ++i)
    {
        float sum = b_policy_[i];
        for (int j = 0; j < TRUNK2_OUT; ++j)
        {
            sum += w_policy_[i * TRUNK2_OUT + j] * trunk_out[j];
        }
        logits[i] = sum;
    }
}

// ---------------------------------------------------------------------------
// evaluate — full forward pass returning value and policy for legal moves
// ---------------------------------------------------------------------------
void DualHeadNetwork::evaluate(const Board& board,
                               const std::vector<Move_t>& moves,
                               std::vector<float>& policy,
                               float& value)
{
    assert(loaded_);

    // Extract features
    float features[INPUT_SIZE];
    extract_features(board, features);

    // Shared trunk
    float trunk_out[TRUNK2_OUT];
    forward_trunk(features, trunk_out);

    // Value head
    value = forward_value(trunk_out);

    // Policy head
    float logits[POLICY_OUT];
    forward_policy(trunk_out, logits);

    // Mask to legal moves and apply softmax
    int num_moves = static_cast<int>(moves.size());
    policy.resize(static_cast<size_t>(num_moves));

    if (num_moves == 0)
    {
        return;
    }

    // Gather logits for legal moves, find max for numerical stability
    float max_logit = -std::numeric_limits<float>::infinity();
    for (int i = 0; i < num_moves; ++i)
    {
        int idx = move_to_policy_index(moves[static_cast<size_t>(i)]);
        assert(idx >= 0 && idx < POLICY_OUT);
        policy[static_cast<size_t>(i)] = logits[idx];
        if (logits[idx] > max_logit)
        {
            max_logit = logits[idx];
        }
    }

    // Softmax with numerical stability (subtract max)
    float sum_exp = 0.0F;
    for (int i = 0; i < num_moves; ++i)
    {
        policy[static_cast<size_t>(i)] = std::exp(policy[static_cast<size_t>(i)] - max_logit);
        sum_exp += policy[static_cast<size_t>(i)];
    }

    // Normalize
    if (sum_exp > 0.0F)
    {
        for (int i = 0; i < num_moves; ++i)
        {
            policy[static_cast<size_t>(i)] /= sum_exp;
        }
    }
    else
    {
        // Fallback: uniform distribution
        float uniform = 1.0F / static_cast<float>(num_moves);
        for (int i = 0; i < num_moves; ++i)
        {
            policy[static_cast<size_t>(i)] = uniform;
        }
    }
}

// ---------------------------------------------------------------------------
// evaluate_value — value-only evaluation, returns centipawn-scale int
// Maps tanh output [-1, +1] to roughly [-600, +600] centipawns.
// ---------------------------------------------------------------------------
int DualHeadNetwork::evaluate_value(const Board& board)
{
    if (!loaded_)
    {
        return 0;
    }

    float features[INPUT_SIZE];
    extract_features(board, features);

    float trunk_out[TRUNK2_OUT];
    forward_trunk(features, trunk_out);

    float value = forward_value(trunk_out);

    // Convert [-1, +1] to centipawns using inverse tanh scaling
    // atanh is undefined at exactly ±1, so clamp
    value = std::max(-0.999F, std::min(0.999F, value));
    return static_cast<int>(std::atanh(value) * 400.0F);
}

// ---------------------------------------------------------------------------
// load — read network weights from binary file
// Layer order: trunk1 w/b, trunk2 w/b, value1 w/b, value2 w/b, policy w/b
// ---------------------------------------------------------------------------
bool DualHeadNetwork::load(const std::string& weights_path)
{
    std::ifstream file(weights_path, std::ios::binary);
    if (!file.is_open())
    {
        return false;
    }

    auto read = [&](void* dst, std::size_t bytes) -> bool
    {
        file.read(reinterpret_cast<char*>(dst), static_cast<std::streamsize>(bytes));
        return !file.fail();
    };

    // Shared trunk
    if (!read(w_trunk1_, sizeof(w_trunk1_)))
        return false;
    if (!read(b_trunk1_, sizeof(b_trunk1_)))
        return false;
    if (!read(w_trunk2_, sizeof(w_trunk2_)))
        return false;
    if (!read(b_trunk2_, sizeof(b_trunk2_)))
        return false;

    // Value head
    if (!read(w_value1_, sizeof(w_value1_)))
        return false;
    if (!read(b_value1_, sizeof(b_value1_)))
        return false;
    if (!read(w_value2_, sizeof(w_value2_)))
        return false;
    if (!read(&b_value2_, sizeof(b_value2_)))
        return false;

    // Policy head
    if (!read(w_policy_, sizeof(w_policy_)))
        return false;
    if (!read(b_policy_, sizeof(b_policy_)))
        return false;

    loaded_ = true;
    return true;
}
