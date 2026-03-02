/*
 * File:   SelfPlay.h
 *
 * Self-play mode for generating training data.
 *
 * Two modes:
 *   1. AlphaBeta self-play (original): records HalfKP features + search score + game result.
 *   2. MCTS self-play: records HalfKP features + MCTS visit distribution (policy target)
 *      + game outcome (value target). Supports temperature schedule and Dirichlet noise.
 */

#ifndef SELFPLAY_H
#define SELFPLAY_H

#include <fstream>
#include <random>
#include <string>
#include <vector>

#include "Board.h"
#include "Evaluator.h"
#include "MCTS.h"
#include "Search.h"
#include "Types.h"

// Training data entry for AlphaBeta self-play (original format)
struct TrainingEntry
{
    float features[768];   // HalfKP input features
    float search_score;    // Search evaluation (side-to-move perspective)
    float game_result;     // 1.0=win, 0.5=draw, 0.0=loss (side-to-move perspective)
};

// Training data entry for MCTS self-play (AlphaZero-style)
struct MCTSTrainingEntry
{
    float features[768];   // HalfKP input features
    float policy[256];     // Visit count distribution over moves (padded, normalized)
    float value;           // Game outcome: +1=win, 0=draw, -1=loss (side-to-move)
    int num_moves;         // Number of legal moves (valid entries in policy[])
    int move_indices[256]; // Move encoding for each policy slot (from*64+to)
};

class SelfPlay
{
public:
    SelfPlay(Board& board, Search& search);

    /// AlphaBeta self-play: play N games and write training data
    void generate_training_data(int num_games,
                                int search_depth,
                                double randomization,
                                const std::string& output_path);

    /// MCTS self-play: play N games using MCTS, recording policy + value targets.
    /// @param num_games       Number of games to play
    /// @param simulations     MCTS simulations per move
    /// @param c_puct          Exploration constant
    /// @param temperature     Initial temperature for move selection
    /// @param temp_drop_ply   Ply at which temperature drops to near-zero
    /// @param dirichlet_alpha Dirichlet noise alpha (0 to disable)
    /// @param dirichlet_eps   Fraction of noise mixed into root priors
    /// @param output_path     Output file path
    void generate_mcts_training_data(int num_games,
                                     int simulations,
                                     double c_puct,
                                     double temperature,
                                     int temp_drop_ply,
                                     double dirichlet_alpha,
                                     double dirichlet_eps,
                                     const std::string& output_path);

    /// Reload NNUE weights at runtime (for iterative training loops)
    bool reload_weights(const std::string& weights_path);

private:
    // --- AlphaBeta self-play helpers ---
    float play_game(int search_depth,
                    double randomization,
                    std::vector<TrainingEntry>& positions);

    Move_t select_move_with_temperature(const Board& board,
                                        int search_depth,
                                        double temperature,
                                        int& search_score);

    void write_training_data(const std::vector<TrainingEntry>& entries,
                             const std::string& output_path);

    // --- MCTS self-play helpers ---
    float play_mcts_game(int simulations,
                         double c_puct,
                         double temperature,
                         int temp_drop_ply,
                         double dirichlet_alpha,
                         double dirichlet_eps,
                         std::vector<MCTSTrainingEntry>& positions);

    /// Select a move from MCTS visit counts using temperature.
    /// Also fills policy[] with the normalized visit distribution.
    Move_t select_mcts_move(MCTSNode* root,
                            double temperature,
                            float policy_out[256],
                            int move_indices_out[256],
                            int& num_moves);

    /// Add Dirichlet noise to root node priors.
    void add_dirichlet_noise(MCTSNode* root, double alpha, double epsilon);

    void write_mcts_training_data(const std::vector<MCTSTrainingEntry>& entries,
                                  const std::string& output_path);

    // --- Shared helpers ---
    void extract_features(const Board& board, float features[768]);
    void setup_starting_position();

    Board& board_;
    Search& search_;
    std::mt19937 rng_;
};

#endif /* SELFPLAY_H */
