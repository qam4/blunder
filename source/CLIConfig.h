/*
 * File:   CLIConfig.h
 *
 * Shared CLI configuration struct and parser.
 * Eliminates duplicated MCTS/AlphaZero/NNUE parsing across
 * xboard, UCI, and selfplay modes in main.cpp.
 */

#ifndef CLICONFIG_H
#define CLICONFIG_H

#include <string>

class CmdLineArgs;

struct CLIConfig
{
    // MCTS
    bool use_mcts = false;
    int mcts_simulations = 800;
    double mcts_cpuct = 1.41;

    // AlphaZero
    bool alphazero_mode = false;

    // NNUE
    std::string nnue_path;

    // Book
    std::string book_path;
    bool no_book = false;
    int book_depth = -1;  // -1 = use default

    // Self-play
    int selfplay_games = 100;
    int selfplay_depth = 6;
    double selfplay_randomization = 0.5;
    std::string selfplay_output = "training_data.bin";
    double selfplay_temperature = 1.0;
    int selfplay_temp_drop = 30;
    double selfplay_dirichlet_alpha = 0.3;
    double selfplay_dirichlet_eps = 0.25;
};

/// Parse shared MCTS/AlphaZero/NNUE/Book/Selfplay flags from command line.
void parse_shared_config(const CmdLineArgs& args, CLIConfig& config);

#endif /* CLICONFIG_H */
