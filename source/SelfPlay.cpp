/*
 * File:   SelfPlay.cpp
 *
 * Implementation of self-play training data generation.
 * Supports both AlphaBeta and MCTS self-play modes.
 */

#include <algorithm>
#include <cmath>
#include <iostream>
#include <numeric>

#include "SelfPlay.h"

#include "Constants.h"
#include "DualHeadNetwork.h"
#include "MoveGenerator.h"
#include "MoveList.h"
#include "NNUEEvaluator.h"
#include "Output.h"

using std::cout;
using std::endl;

SelfPlay::SelfPlay(Board& board, Search& search)
    : board_(board)
    , search_(search)
{
    // Seed RNG with current time
    rng_.seed(static_cast<unsigned int>(std::time(nullptr)));
}

// ---------------------------------------------------------------------------
// AlphaBeta self-play (original)
// ---------------------------------------------------------------------------

void SelfPlay::generate_training_data(int num_games,
                                      int search_depth,
                                      double randomization,
                                      const std::string& output_path)
{
    cout << "Generating training data: " << num_games << " games, depth " << search_depth
         << ", randomization " << randomization << endl;

    std::vector<TrainingEntry> all_entries;
    all_entries.reserve(static_cast<size_t>(num_games) * 80);  // Estimate ~80 positions per game

    for (int game_num = 0; game_num < num_games; ++game_num)
    {
        // Reset board to starting position
        board_.reset();
        setup_starting_position();

        std::vector<TrainingEntry> game_positions;
        float game_result = play_game(search_depth, randomization, game_positions);

        // Fill in game results for all positions in this game
        for (auto& entry : game_positions)
        {
            entry.game_result = game_result;
        }

        all_entries.insert(all_entries.end(), game_positions.begin(), game_positions.end());

        if ((game_num + 1) % 10 == 0)
        {
            cout << "Completed " << (game_num + 1) << " / " << num_games << " games ("
                 << all_entries.size() << " positions)" << endl;
        }
    }

    cout << "Writing " << all_entries.size() << " training positions to " << output_path << endl;
    write_training_data(all_entries, output_path);
    cout << "Training data generation complete." << endl;
}

float SelfPlay::play_game(int search_depth,
                          double randomization,
                          std::vector<TrainingEntry>& positions)
{
    // Limit game length to avoid overflow of move_stack_ (size MAX_SEARCH_PLY = 64)
    // We need some headroom for the search depth, so limit to 60 moves
    int max_moves = 60;
    int move_count = 0;

    while (!board_.is_game_over() && move_count < max_moves)
    {
        // Record current position
        TrainingEntry entry;
        extract_features(board_, entry.features);

        // Search and select move
        int search_score;
        Move_t move =
            select_move_with_temperature(board_, search_depth, randomization, search_score);

        if (move == Move(0))
        {
            break;  // No legal moves
        }

        // Store search score from side-to-move perspective
        entry.search_score = static_cast<float>(search_score);
        entry.game_result = 0.0f;  // Will be filled in later
        positions.push_back(entry);

        // Make the move
        board_.do_move(move);
        move_count++;
    }

    // Determine game result from White's perspective
    float result;
    if (board_.is_draw(false))
    {
        result = 0.5f;  // Draw
    }
    else if (MoveGenerator::in_check(board_, board_.side_to_move()))
    {
        // Checkmate: side to move lost
        result = (board_.side_to_move() == WHITE) ? 0.0f : 1.0f;
    }
    else
    {
        // Stalemate or max moves reached
        result = 0.5f;
    }

    // Convert result to side-to-move perspective for each position
    // We need to go back through and adjust based on who was to move
    for (size_t i = 0; i < positions.size(); ++i)
    {
        // Determine who was to move at this position
        // Position i was recorded before move i was made
        // If i is even, White to move; if odd, Black to move
        bool white_to_move = (i % 2 == 0);
        if (white_to_move)
        {
            positions[i].game_result = result;  // White's perspective
        }
        else
        {
            positions[i].game_result = 1.0f - result;  // Black's perspective (flip)
        }
    }

    return result;
}

Move_t SelfPlay::select_move_with_temperature(const Board& board,
                                              int search_depth,
                                              double temperature,
                                              int& search_score)
{
    // Generate all legal moves
    MoveList moves;
    MoveGenerator::add_all_moves(moves, board, board.side_to_move());

    if (moves.length() == 0)
    {
        search_score = 0;
        return Move(0);
    }

    // If only one legal move, return it
    if (moves.length() == 1)
    {
        // Do a quick search to get the score
        board_.do_move(moves[0]);
        search_score = -search_.alphabeta(-MAX_SCORE, MAX_SCORE, search_depth - 1, IS_PV, DO_NULL);
        board_.undo_move(moves[0]);
        return moves[0];
    }

    // Search each move to get scores
    std::vector<std::pair<Move_t, int>> move_scores;
    move_scores.reserve(static_cast<size_t>(moves.length()));

    for (int i = 0; i < moves.length(); ++i)
    {
        board_.do_move(moves[i]);
        // Search from opponent's perspective, then negate
        int score = -search_.alphabeta(-MAX_SCORE, MAX_SCORE, search_depth - 1, IS_PV, DO_NULL);
        board_.undo_move(moves[i]);
        move_scores.push_back({ moves[i], score });
    }

    // Sort by score (best first)
    std::sort(move_scores.begin(),
              move_scores.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    // If temperature is 0, return best move
    if (temperature <= 0.0)
    {
        search_score = move_scores[0].second;
        return move_scores[0].first;
    }

    // Apply temperature-based selection using softmax
    std::vector<double> probabilities;
    probabilities.reserve(move_scores.size());

    double max_score = static_cast<double>(move_scores[0].second);
    double sum = 0.0;

    for (const auto& ms : move_scores)
    {
        double exp_val = std::exp((ms.second - max_score) / temperature);
        probabilities.push_back(exp_val);
        sum += exp_val;
    }

    // Normalize probabilities
    for (auto& p : probabilities)
    {
        p /= sum;
    }

    // Sample from the distribution
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    double r = dist(rng_);
    double cumulative = 0.0;

    for (size_t i = 0; i < probabilities.size(); ++i)
    {
        cumulative += probabilities[i];
        if (r <= cumulative)
        {
            search_score = move_scores[i].second;
            return move_scores[i].first;
        }
    }

    // Fallback: return best move
    search_score = move_scores[0].second;
    return move_scores[0].first;
}

void SelfPlay::write_training_data(const std::vector<TrainingEntry>& entries,
                                   const std::string& output_path)
{
    std::ofstream out(output_path, std::ios::binary);
    if (!out)
    {
        std::cerr << "Error: could not open " << output_path << " for writing" << endl;
        return;
    }

    // Write each entry: 768 floats (features) + 1 float (search_score) + 1 float (game_result)
    for (const auto& entry : entries)
    {
        out.write(reinterpret_cast<const char*>(entry.features), sizeof(entry.features));
        out.write(reinterpret_cast<const char*>(&entry.search_score), sizeof(entry.search_score));
        out.write(reinterpret_cast<const char*>(&entry.game_result), sizeof(entry.game_result));
    }

    out.close();
}

// ---------------------------------------------------------------------------
// Shared helpers
// ---------------------------------------------------------------------------

void SelfPlay::extract_features(const Board& board, float features[768])
{
    // Initialize all features to 0
    for (int i = 0; i < 768; ++i)
    {
        features[i] = 0.0f;
    }

    // HalfKP encoding: 6 piece types × 2 colors × 64 squares = 768 features
    // Feature index = piece_type * 128 + color * 64 + square
    // piece_type: 0=pawn, 1=knight, 2=bishop, 3=rook, 4=queen, 5=king
    // color: 0=white, 1=black
    // square: 0-63

    for (int square = 0; square < 64; ++square)
    {
        U8 piece = board[square];
        if (piece == EMPTY)
        {
            continue;
        }

        int piece_type;
        int color;

        // Map piece to type and color
        // Piece values: WHITE_PAWN=2, BLACK_PAWN=3, WHITE_KNIGHT=4, BLACK_KNIGHT=5, ...
        // They're interleaved, so we need to map them correctly
        switch (piece)
        {
            case WHITE_PAWN:
                piece_type = 0;
                color = 0;
                break;
            case BLACK_PAWN:
                piece_type = 0;
                color = 1;
                break;
            case WHITE_KNIGHT:
                piece_type = 1;
                color = 0;
                break;
            case BLACK_KNIGHT:
                piece_type = 1;
                color = 1;
                break;
            case WHITE_BISHOP:
                piece_type = 2;
                color = 0;
                break;
            case BLACK_BISHOP:
                piece_type = 2;
                color = 1;
                break;
            case WHITE_ROOK:
                piece_type = 3;
                color = 0;
                break;
            case BLACK_ROOK:
                piece_type = 3;
                color = 1;
                break;
            case WHITE_QUEEN:
                piece_type = 4;
                color = 0;
                break;
            case BLACK_QUEEN:
                piece_type = 4;
                color = 1;
                break;
            case WHITE_KING:
                piece_type = 5;
                color = 0;
                break;
            case BLACK_KING:
                piece_type = 5;
                color = 1;
                break;
            default:
                continue;  // Invalid piece
        }

        // Calculate feature index
        int feature_idx = piece_type * 128 + color * 64 + square;
        features[feature_idx] = 1.0f;
    }
}

void SelfPlay::setup_starting_position()
{
    // Set up pawns
    for (int i = 0; i < 8; ++i)
    {
        board_.add_piece(WHITE_PAWN, 8 + i);   // Rank 2
        board_.add_piece(BLACK_PAWN, 48 + i);  // Rank 7
    }

    // Set up white pieces (rank 1)
    board_.add_piece(WHITE_ROOK, 0);
    board_.add_piece(WHITE_KNIGHT, 1);
    board_.add_piece(WHITE_BISHOP, 2);
    board_.add_piece(WHITE_QUEEN, 3);
    board_.add_piece(WHITE_KING, 4);
    board_.add_piece(WHITE_BISHOP, 5);
    board_.add_piece(WHITE_KNIGHT, 6);
    board_.add_piece(WHITE_ROOK, 7);

    // Set up black pieces (rank 8)
    board_.add_piece(BLACK_ROOK, 56);
    board_.add_piece(BLACK_KNIGHT, 57);
    board_.add_piece(BLACK_BISHOP, 58);
    board_.add_piece(BLACK_QUEEN, 59);
    board_.add_piece(BLACK_KING, 60);
    board_.add_piece(BLACK_BISHOP, 61);
    board_.add_piece(BLACK_KNIGHT, 62);
    board_.add_piece(BLACK_ROOK, 63);

    // Update the hash after setting up all pieces
    board_.update_hash();
}

// ---------------------------------------------------------------------------
// MCTS self-play (AlphaZero-style)
// ---------------------------------------------------------------------------

void SelfPlay::generate_mcts_training_data(int num_games,
                                           int simulations,
                                           double c_puct,
                                           double temperature,
                                           int temp_drop_ply,
                                           double dirichlet_alpha,
                                           double dirichlet_eps,
                                           const std::string& output_path)
{
    cout << "MCTS self-play: " << num_games << " games, " << simulations << " sims/move"
         << ", c_puct=" << c_puct << ", temp=" << temperature << ", temp_drop=" << temp_drop_ply
         << ", dirichlet_alpha=" << dirichlet_alpha << ", dirichlet_eps=" << dirichlet_eps << endl;

    std::vector<MCTSTrainingEntry> all_entries;
    all_entries.reserve(static_cast<size_t>(num_games) * 80);

    for (int game_num = 0; game_num < num_games; ++game_num)
    {
        board_.reset();
        setup_starting_position();

        // Refresh NNUE accumulator if loaded
        NNUEEvaluator* nnue = board_.get_nnue();
        if (nnue != nullptr && nnue->is_loaded())
        {
            nnue->refresh(board_);
        }

        std::vector<MCTSTrainingEntry> game_positions;
        float game_result = play_mcts_game(simulations,
                                           c_puct,
                                           temperature,
                                           temp_drop_ply,
                                           dirichlet_alpha,
                                           dirichlet_eps,
                                           game_positions);

        // Fill in game outcome for each position (from side-to-move perspective)
        for (size_t i = 0; i < game_positions.size(); ++i)
        {
            bool white_to_move = (i % 2 == 0);
            if (white_to_move)
            {
                // game_result is from White's perspective: 1.0=W win, 0.0=B win, 0.5=draw
                // Convert to [-1, +1]: win=+1, draw=0, loss=-1
                game_positions[i].value = 2.0f * game_result - 1.0f;
            }
            else
            {
                game_positions[i].value = 2.0f * (1.0f - game_result) - 1.0f;
            }
        }

        all_entries.insert(all_entries.end(), game_positions.begin(), game_positions.end());

        if ((game_num + 1) % 10 == 0 || game_num == 0)
        {
            cout << "Completed " << (game_num + 1) << " / " << num_games << " games ("
                 << all_entries.size() << " positions)" << endl;
        }
    }

    cout << "Writing " << all_entries.size() << " MCTS training positions to " << output_path
         << endl;
    write_mcts_training_data(all_entries, output_path);
    cout << "MCTS training data generation complete." << endl;
}

float SelfPlay::play_mcts_game(int simulations,
                               double c_puct,
                               double temperature,
                               int temp_drop_ply,
                               double dirichlet_alpha,
                               double dirichlet_eps,
                               std::vector<MCTSTrainingEntry>& positions)
{
    int max_moves = 200;
    int move_count = 0;
    Evaluator& eval = board_.get_evaluator();

    while (!board_.is_game_over() && move_count < max_moves)
    {
        // Create MCTS instance — use DualHeadNetwork when available for
        // AlphaZero-style policy priors and value evaluation, otherwise
        // fall back to the handcrafted evaluator with uniform priors.
        std::unique_ptr<MCTS> mcts;
        if (dual_head_ != nullptr && dual_head_->is_loaded())
        {
            mcts = std::make_unique<MCTS>(board_, *dual_head_, c_puct, simulations);
        }
        else
        {
            mcts = std::make_unique<MCTS>(board_, eval, c_puct, simulations);
        }

        // Add Dirichlet noise to root priors for exploration
        // We need to expand root first, then add noise, then run simulations.
        // Since search_return_root() expands root internally, we use it directly
        // and accept that noise is applied after expansion in a future refinement.
        // For now, run the search to get the tree.
        auto root = mcts->search_return_root(nullptr);

        if (root->children.empty())
        {
            break;
        }

        // Apply Dirichlet noise to root priors (post-hoc for this iteration)
        if (dirichlet_alpha > 0.0)
        {
            add_dirichlet_noise(root.get(), dirichlet_alpha, dirichlet_eps);
        }

        // Record position features
        MCTSTrainingEntry entry {};
        extract_features(board_, entry.features);
        entry.value = 0.0f;  // Filled in after game ends

        // Determine effective temperature
        double eff_temp = (move_count < temp_drop_ply) ? temperature : 0.05;

        // Select move and extract policy from visit distribution
        Move_t chosen = select_mcts_move(
            root.get(), eff_temp, entry.policy, entry.move_indices, entry.num_moves);

        if (chosen == Move(0))
        {
            break;
        }

        positions.push_back(entry);
        board_.do_move(chosen);
        move_count++;
    }

    // Determine game result from White's perspective
    if (board_.is_draw(false))
    {
        return 0.5f;
    }
    if (MoveGenerator::in_check(board_, board_.side_to_move()))
    {
        return (board_.side_to_move() == WHITE) ? 0.0f : 1.0f;
    }
    return 0.5f;
}

Move_t SelfPlay::select_mcts_move(MCTSNode* root,
                                  double temperature,
                                  float policy_out[256],
                                  int move_indices_out[256],
                                  int& num_moves)
{
    // Zero out policy
    for (int i = 0; i < 256; ++i)
    {
        policy_out[i] = 0.0f;
        move_indices_out[i] = 0;
    }

    int n = static_cast<int>(root->children.size());
    if (n == 0)
    {
        num_moves = 0;
        return Move(0);
    }
    num_moves = (n < 256) ? n : 256;

    // Collect visit counts
    std::vector<double> visits(static_cast<size_t>(num_moves));
    int total_visits = 0;
    for (int i = 0; i < num_moves; ++i)
    {
        visits[static_cast<size_t>(i)] =
            static_cast<double>(root->children[static_cast<size_t>(i)]->visits);
        total_visits += root->children[static_cast<size_t>(i)]->visits;
        Move_t m = root->children[static_cast<size_t>(i)]->move;
        move_indices_out[i] = m.from() * 64 + m.to();
    }

    if (total_visits == 0)
    {
        // No visits — pick uniformly
        std::uniform_int_distribution<int> dist(0, num_moves - 1);
        int idx = dist(rng_);
        policy_out[idx] = 1.0f;
        return root->children[static_cast<size_t>(idx)]->move;
    }

    // Apply temperature
    if (temperature <= 0.05)
    {
        // Near-zero temperature: pick the most visited move deterministically
        int best_idx = 0;
        for (int i = 1; i < num_moves; ++i)
        {
            if (visits[static_cast<size_t>(i)] > visits[static_cast<size_t>(best_idx)])
            {
                best_idx = i;
            }
        }

        // Policy is normalized visit counts (for training target)
        for (int i = 0; i < num_moves; ++i)
        {
            policy_out[i] = static_cast<float>(visits[static_cast<size_t>(i)] / total_visits);
        }

        return root->children[static_cast<size_t>(best_idx)]->move;
    }

    // Temperature-scaled selection: raise visit counts to power 1/T, then normalize
    double inv_temp = 1.0 / temperature;
    std::vector<double> probs(static_cast<size_t>(num_moves));
    double sum = 0.0;
    for (int i = 0; i < num_moves; ++i)
    {
        probs[static_cast<size_t>(i)] = std::pow(visits[static_cast<size_t>(i)], inv_temp);
        sum += probs[static_cast<size_t>(i)];
    }

    // Normalize and fill policy output
    for (int i = 0; i < num_moves; ++i)
    {
        probs[static_cast<size_t>(i)] /= sum;
        policy_out[i] = static_cast<float>(probs[static_cast<size_t>(i)]);
    }

    // Sample from distribution
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    double r = dist(rng_);
    double cumulative = 0.0;
    for (int i = 0; i < num_moves; ++i)
    {
        cumulative += probs[static_cast<size_t>(i)];
        if (r <= cumulative)
        {
            return root->children[static_cast<size_t>(i)]->move;
        }
    }

    return root->children[static_cast<size_t>(num_moves - 1)]->move;
}

void SelfPlay::add_dirichlet_noise(MCTSNode* root, double alpha, double epsilon)
{
    int n = static_cast<int>(root->children.size());
    if (n == 0)
    {
        return;
    }

    // Sample from Dirichlet distribution (using gamma distribution)
    std::gamma_distribution<double> gamma(alpha, 1.0);
    std::vector<double> noise(static_cast<size_t>(n));
    double noise_sum = 0.0;

    for (int i = 0; i < n; ++i)
    {
        noise[static_cast<size_t>(i)] = gamma(rng_);
        noise_sum += noise[static_cast<size_t>(i)];
    }

    // Normalize and mix with existing priors
    for (int i = 0; i < n; ++i)
    {
        noise[static_cast<size_t>(i)] /= noise_sum;
        root->children[static_cast<size_t>(i)]->prior =
            (1.0 - epsilon) * root->children[static_cast<size_t>(i)]->prior
            + epsilon * noise[static_cast<size_t>(i)];
    }
}

void SelfPlay::write_mcts_training_data(const std::vector<MCTSTrainingEntry>& entries,
                                        const std::string& output_path)
{
    std::ofstream out(output_path, std::ios::binary);
    if (!out)
    {
        std::cerr << "Error: could not open " << output_path << " for writing" << endl;
        return;
    }

    // File format (per entry):
    //   768 floats: features
    //   1 int:      num_moves
    //   num_moves floats: policy (normalized visit distribution)
    //   num_moves ints:   move_indices (from*64+to encoding)
    //   1 float:   value (game outcome)
    for (const auto& entry : entries)
    {
        out.write(reinterpret_cast<const char*>(entry.features), sizeof(entry.features));
        out.write(reinterpret_cast<const char*>(&entry.num_moves), sizeof(entry.num_moves));
        out.write(
            reinterpret_cast<const char*>(entry.policy),
            static_cast<std::streamsize>(static_cast<size_t>(entry.num_moves) * sizeof(float)));
        out.write(reinterpret_cast<const char*>(entry.move_indices),
                  static_cast<std::streamsize>(static_cast<size_t>(entry.num_moves) * sizeof(int)));
        out.write(reinterpret_cast<const char*>(&entry.value), sizeof(entry.value));
    }

    out.close();
}

bool SelfPlay::reload_weights(const std::string& weights_path)
{
    NNUEEvaluator* nnue = board_.get_nnue();
    if (nnue == nullptr)
    {
        return false;
    }

    if (!nnue->load(weights_path))
    {
        return false;
    }

    // Refresh accumulator for current board state
    nnue->refresh(board_);
    return true;
}
