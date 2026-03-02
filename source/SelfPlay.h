/*
 * File:   SelfPlay.h
 *
 * Self-play mode for generating training data for NNUE.
 * The engine plays games against itself, recording positions with their
 * search evaluations and final game outcomes.
 */

#ifndef SELFPLAY_H
#define SELFPLAY_H

#include <string>
#include <vector>
#include <fstream>
#include <random>

#include "Board.h"
#include "Search.h"
#include "Types.h"

// Training data entry: position features + search score + game result
struct TrainingEntry
{
    float features[768];  // HalfKP input features
    float search_score;   // Search evaluation (from side-to-move perspective)
    float game_result;    // 1.0 = win, 0.5 = draw, 0.0 = loss (from side-to-move perspective)
};

class SelfPlay
{
public:
    SelfPlay(Board& board, Search& search);

    /// Play N self-play games and write training data to output_path
    /// search_depth: depth to search each move
    /// randomization: temperature for move selection (0.0 = deterministic, higher = more random)
    void generate_training_data(int num_games, 
                                 int search_depth,
                                 double randomization,
                                 const std::string& output_path);

private:
    /// Play a single game, returning the game result from White's perspective
    /// (1.0 = White win, 0.0 = Black win, 0.5 = draw)
    /// Populates positions with recorded positions and their search scores
    float play_game(int search_depth, double randomization, std::vector<TrainingEntry>& positions);

    /// Extract HalfKP features from the current board position
    void extract_features(const Board& board, float features[768]);

    /// Select a move using temperature-based randomization
    /// temperature = 0.0: always pick best move
    /// temperature > 0.0: sample from softmax distribution over move scores
    Move_t select_move_with_temperature(const Board& board, 
                                         int search_depth,
                                         double temperature,
                                         int& search_score);

    /// Write training data to binary file
    void write_training_data(const std::vector<TrainingEntry>& entries,
                             const std::string& output_path);

    /// Set up the starting chess position on the board
    void setup_starting_position();

    Board& board_;
    Search& search_;
    std::mt19937 rng_;
};

#endif /* SELFPLAY_H */
