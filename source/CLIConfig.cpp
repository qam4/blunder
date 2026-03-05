/*
 * File:   CLIConfig.cpp
 */

#include <string>

#include "CLIConfig.h"

#include "CmdLineArgs.h"

void parse_shared_config(const CmdLineArgs& args, CLIConfig& config)
{
    // MCTS
    config.use_mcts = args.cmd_option_exists("--mcts");
    config.alphazero_mode = args.cmd_option_exists("--alphazero");

    if (args.cmd_option_exists("--mcts-simulations"))
    {
        std::string s = args.get_cmd_option("--mcts-simulations");
        if (!s.empty())
        {
            config.mcts_simulations = std::stoi(s);
        }
    }
    if (args.cmd_option_exists("--mcts-cpuct"))
    {
        std::string s = args.get_cmd_option("--mcts-cpuct");
        if (!s.empty())
        {
            config.mcts_cpuct = std::stod(s);
        }
    }

    // NNUE
    if (args.cmd_option_exists("--nnue"))
    {
        config.nnue_path = args.get_cmd_option("--nnue");
    }

    // Book
    config.no_book = args.cmd_option_exists("--no-book");
    if (args.cmd_option_exists("--book"))
    {
        config.book_path = args.get_cmd_option("--book");
    }
    if (args.cmd_option_exists("--book-depth"))
    {
        std::string s = args.get_cmd_option("--book-depth");
        if (!s.empty())
        {
            config.book_depth = std::stoi(s);
        }
    }

    // Self-play
    if (args.cmd_option_exists("--selfplay-games"))
    {
        std::string s = args.get_cmd_option("--selfplay-games");
        if (!s.empty())
        {
            config.selfplay_games = std::stoi(s);
        }
    }
    if (args.cmd_option_exists("--selfplay-depth"))
    {
        std::string s = args.get_cmd_option("--selfplay-depth");
        if (!s.empty())
        {
            config.selfplay_depth = std::stoi(s);
        }
    }
    if (args.cmd_option_exists("--selfplay-randomization"))
    {
        std::string s = args.get_cmd_option("--selfplay-randomization");
        if (!s.empty())
        {
            config.selfplay_randomization = std::stod(s);
        }
    }
    if (args.cmd_option_exists("--selfplay-output"))
    {
        config.selfplay_output = args.get_cmd_option("--selfplay-output");
    }
    if (args.cmd_option_exists("--selfplay-temperature"))
    {
        std::string s = args.get_cmd_option("--selfplay-temperature");
        if (!s.empty())
        {
            config.selfplay_temperature = std::stod(s);
        }
    }
    if (args.cmd_option_exists("--selfplay-temp-drop"))
    {
        std::string s = args.get_cmd_option("--selfplay-temp-drop");
        if (!s.empty())
        {
            config.selfplay_temp_drop = std::stoi(s);
        }
    }
    if (args.cmd_option_exists("--selfplay-dirichlet-alpha"))
    {
        std::string s = args.get_cmd_option("--selfplay-dirichlet-alpha");
        if (!s.empty())
        {
            config.selfplay_dirichlet_alpha = std::stod(s);
        }
    }
    if (args.cmd_option_exists("--selfplay-dirichlet-eps"))
    {
        std::string s = args.get_cmd_option("--selfplay-dirichlet-eps");
        if (!s.empty())
        {
            config.selfplay_dirichlet_eps = std::stod(s);
        }
    }
}
