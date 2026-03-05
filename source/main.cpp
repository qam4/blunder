/*
 * File:   main.cpp
 *
 */

#include <cstdlib>
#include <ctime>
#include <iostream>
#include <memory>
#include <string>

#ifdef _WIN32
#    include <windows.h>
#endif

#include "Board.h"
#include "Book.h"
#include "CLIConfig.h"
#include "CLIUtils.h"
#include "CmdLineArgs.h"
#include "DualHeadNetwork.h"
#include "MCTS.h"
#include "MoveGenerator.h"
#include "MoveList.h"
#include "NNUEEvaluator.h"
#include "Output.h"
#include "Parser.h"
#include "Perft.h"
#include "Search.h"
#include "SelfPlay.h"
#include "TestPositions.h"
#include "UCI.h"
#include "ValidateMove.h"
#include "Xboard.h"

using std::cin;
using std::cout;
using std::endl;
using std::string;
using std::vector;

static void print_help();
static void usage(const string& prog_name);

/// Configure MCTS/AlphaZero mode on a protocol handler (Xboard or UCI).
/// Returns the DualHeadNetwork (caller must keep alive while handler runs).
template<typename Handler>
static std::unique_ptr<DualHeadNetwork> configure_mcts(Handler& handler, const CLIConfig& cfg)
{
    auto dual_head = std::make_unique<DualHeadNetwork>();

    if (cfg.alphazero_mode)
    {
        if (!cfg.nnue_path.empty() && dual_head->load(cfg.nnue_path))
        {
            cout << "AlphaZero: loaded dual-head network from " << cfg.nnue_path << endl;
            handler.set_alphazero_mode(dual_head.get(), cfg.mcts_simulations, cfg.mcts_cpuct);
        }
        else
        {
            if (cfg.nnue_path.empty())
            {
                cout << "AlphaZero: no --nnue path provided, "
                     << "falling back to MCTS with handcrafted eval" << endl;
            }
            else
            {
                cout << "AlphaZero: failed to load dual-head network, "
                     << "falling back to MCTS with handcrafted eval" << endl;
            }
            handler.set_mcts_mode(cfg.mcts_simulations, cfg.mcts_cpuct);
        }
    }
    else if (cfg.use_mcts)
    {
        handler.set_mcts_mode(cfg.mcts_simulations, cfg.mcts_cpuct);
    }

    return dual_head;
}

/// Load opening book from config. Returns true if book was loaded.
static bool load_book(Book& book, const CLIConfig& cfg)
{
    if (!cfg.no_book && !cfg.book_path.empty() && book.open(cfg.book_path))
    {
        if (cfg.book_depth >= 0)
        {
            book.set_max_depth(cfg.book_depth);
        }
        return true;
    }
    return false;
}

/// Load NNUE weights. Returns true if loaded successfully.
static bool load_nnue(NNUEEvaluator& nnue, const CLIConfig& cfg)
{
    if (!cfg.nnue_path.empty())
    {
        if (nnue.load(cfg.nnue_path))
        {
            cout << "NNUE: loaded weights from " << cfg.nnue_path << endl;
            return true;
        }
        cout << "NNUE: failed to load weights from " << cfg.nnue_path
             << ", falling back to hand-crafted evaluation" << endl;
    }
    return false;
}

int main(int argc, char** argv)
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    CmdLineArgs cmd_line_args(argc, argv);
    if (cmd_line_args.cmd_option_exists("-h") || cmd_line_args.cmd_option_exists("--help"))
    {
        usage(cmd_line_args.get_program_name());
        return 0;
    }

    if (cmd_line_args.cmd_option_exists("--gen-lookup-tables"))
    {
        cout << "Generating lookup tables..." << endl;
        MoveGenerator::generate_move_lookup_tables();
        return 0;
    }

    if (cmd_line_args.cmd_option_exists("--perft"))
    {
        cout << "Perft..." << endl;
        perft_benchmark("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 6);
        return 0;
    }

    // Parse shared configuration once
    CLIConfig cfg;
    parse_shared_config(cmd_line_args, cfg);

    Book book;
    bool book_enabled = load_book(book, cfg);

    NNUEEvaluator nnue;
    bool nnue_loaded = load_nnue(nnue, cfg);

    if (cmd_line_args.cmd_option_exists("--xboard"))
    {
        Xboard xboard;
        if (book_enabled)
        {
            xboard.set_book(std::move(book));
        }
        auto dual_head = configure_mcts(xboard, cfg);
        if (nnue_loaded && !cfg.alphazero_mode)
        {
            xboard.set_nnue(&nnue);
        }
        xboard.run();
        return 0;
    }

    if (cmd_line_args.cmd_option_exists("--uci"))
    {
        UCI uci;
        if (book_enabled)
        {
            uci.set_book(std::move(book));
        }
        auto dual_head = configure_mcts(uci, cfg);
        if (nnue_loaded && !cfg.alphazero_mode)
        {
            uci.set_nnue(&nnue);
        }
        uci.run();
        return 0;
    }

    if (cmd_line_args.cmd_option_exists("--test-positions"))
    {
        string epd_path = cmd_line_args.get_cmd_option("--test-positions");
        if (epd_path.empty())
        {
            cout << "Error: path to EPD file required " << endl;
            return 1;
        }
        test_positions_benchmark(epd_path);
        return 0;
    }

    if (cmd_line_args.cmd_option_exists("--selfplay"))
    {
        Board board = Parser::parse_fen(DEFAULT_FEN);
        if (nnue_loaded)
        {
            board.set_nnue(&nnue);
            nnue.refresh(board);
        }
        Search search(board);

        if (cfg.use_mcts)
        {
            SelfPlay selfplay(board, search);

            auto selfplay_dual_head = std::make_unique<DualHeadNetwork>();
            if (cfg.alphazero_mode && !cfg.nnue_path.empty()
                && selfplay_dual_head->load(cfg.nnue_path))
            {
                selfplay.set_dual_head_network(selfplay_dual_head.get());
                cout << "AlphaZero self-play: loaded dual-head network from " << cfg.nnue_path
                     << endl;
            }
            else if (cfg.alphazero_mode)
            {
                cout << "AlphaZero self-play: failed to load dual-head network, "
                     << "using uniform priors" << endl;
            }

            selfplay.generate_mcts_training_data(cfg.selfplay_games,
                                                 cfg.mcts_simulations,
                                                 cfg.mcts_cpuct,
                                                 cfg.selfplay_temperature,
                                                 cfg.selfplay_temp_drop,
                                                 cfg.selfplay_dirichlet_alpha,
                                                 cfg.selfplay_dirichlet_eps,
                                                 cfg.selfplay_output);
        }
        else
        {
            SelfPlay selfplay(board, search);
            selfplay.generate_training_data(cfg.selfplay_games,
                                            cfg.selfplay_depth,
                                            cfg.selfplay_randomization,
                                            cfg.selfplay_output);
        }
        return 0;
    }

    // Interactive mode
    bool computer_plays[2] = { false };
    int game_ply = 0;

    Board board = Parser::parse_fen(DEFAULT_FEN);
    if (nnue_loaded)
    {
        board.set_nnue(&nnue);
        nnue.refresh(board);
    }
    Search search(board);

    while (true)
    {
        cout << Output::board(board);
        if (board.is_game_over())
        {
            if (!MoveGenerator::in_check(board, board.side_to_move()) || board.is_draw())
            {
                cout << "Draw" << endl;
            }
            else if (board.side_to_move() == WHITE)
            {
                cout << "Black player won" << endl;
            }
            else
            {
                cout << "White player won" << endl;
            }
            break;
        }

        if (computer_plays[board.side_to_move()])
        {
            if (book_enabled && book.within_depth(game_ply) && book.has_move(board))
            {
                Move_t book_move = book.get_move(board);
                if (book_move != Move(0))
                {
                    cout << "Book move: " << Output::move_san(book_move, board) << endl;
                    board.do_move(book_move);
                    game_ply++;
                    continue;
                }
            }

            clock_t tic = clock();
            cout << "Thinking..." << endl;
            Move_t move = search.search(MAX_SEARCH_PLY);
            clock_t toc = clock();
            double elapsed_secs = double(toc - tic) / CLOCKS_PER_SEC;
            cout << "time: " << elapsed_secs << "s" << endl;
            cout << "searched moves: " << search.get_searched_moves() << endl;
            cout << "searched moves per second: " << int(search.get_searched_moves() / elapsed_secs)
                 << endl;
            cout << "Computer move: " << Output::move_san(move, board) << endl;
            board.do_move(move);
            game_ply++;
            continue;
        }

        string line;
        cout << "> ";
        getline(cin, line);

        vector<string> tokens = split(line, ' ');
        if (tokens.empty())
        {
            continue;
        }
        if (tokens[0] == "quit")
        {
            break;
        }
        else if (tokens[0] == "help")
        {
            print_help();
        }
        else if (tokens[0] == "move")
        {
            if (tokens.size() < 2)
            {
                cout << "Usage: move <move>" << endl;
                continue;
            }
            auto opt = Parser::move(tokens[1], board);
            if (opt && is_valid_move(*opt, board, true))
            {
                board.do_move(*opt);
                game_ply++;
            }
        }
        else if (tokens[0] == "play")
        {
            if (tokens.size() < 2)
            {
                cout << "Usage: play <white|black>" << endl;
                continue;
            }
            if (tokens[1] == "white")
            {
                computer_plays[WHITE] = true;
                cout << "Computer plays white" << endl;
            }
            else if (tokens[1] == "black")
            {
                computer_plays[BLACK] = true;
                cout << "Computer plays black" << endl;
            }
            else
            {
                cout << "<color> should be black or white" << endl;
            }
        }
        else if (tokens[0] == "colors")
        {
            if (tokens.size() < 2)
            {
                cout << "Usage: colors <on|off>" << endl;
                continue;
            }
            if (tokens[1] == "on")
            {
                Output::set_colors_enabled(true);
            }
            else if (tokens[1] == "off")
            {
                Output::set_colors_enabled(false);
            }
        }
        else if (tokens[0] == "unicode")
        {
            if (tokens.size() < 2)
            {
                cout << "Usage: unicode <on|off>" << endl;
                continue;
            }
            if (tokens[1] == "on")
            {
                Output::set_unicode_enabled(true);
            }
            else if (tokens[1] == "off")
            {
                Output::set_unicode_enabled(false);
            }
        }
    }
    cout << "Goodbye." << endl;
}

void print_help()
{
    cout << "Commands:\n"
            "  quit                      Exit this program\n"
            "  help                      Display this screen\n"
            "  move <move>               Play <move> on the board\n"
            "  play <color>              Let computer play <color>\n"
            "  colors <on|off>\n"
            "  unicode <on|off>          Use unicode chess pieces\n";
}

void usage(const string& prog_name)
{
    cout << prog_name
         << "\n"
            "Options:\n"
            "-h|--help                        Print this help\n"
            "--gen-lookup-tables              Generate lookup tables\n"
            "--perft                          Run perft benchmark\n"
            "--xboard                         xboard interface\n"
            "--uci                            UCI interface\n"
            "--test-positions path-to-epd     Run test positions\n"
            "--book <path>                    Use opening book at <path>\n"
            "--no-book                        Disable opening book\n"
            "--book-depth <N>                 Stop using book after N plies\n"
            "--nnue <path>                    Use NNUE weights file at <path>\n"
            "--mcts                           Use MCTS search instead of AlphaBeta\n"
            "--alphazero                      Use MCTS with dual-head network (requires --nnue)\n"
            "--mcts-simulations <N>           MCTS simulations per move (default: 800)\n"
            "--mcts-cpuct <F>                 MCTS exploration constant (default: 1.41)\n"
            "\n"
            "Self-play (AlphaBeta):\n"
            "--selfplay                       Generate training data via self-play\n"
            "--selfplay-games <N>             Number of self-play games (default: 100)\n"
            "--selfplay-depth <D>             Search depth per move (default: 6)\n"
            "--selfplay-randomization <T>     Temperature for move selection (default: 0.5)\n"
            "--selfplay-output <path>         Output file path (default: training_data.bin)\n"
            "\n"
            "Self-play (MCTS) — combine --selfplay with --mcts:\n"
            "--selfplay --mcts                MCTS self-play with policy+value targets\n"
            "--selfplay-temperature <T>       Initial temperature (default: 1.0)\n"
            "--selfplay-temp-drop <N>         Ply to drop temperature (default: 30)\n"
            "--selfplay-dirichlet-alpha <F>   Dirichlet noise alpha (default: 0.3)\n"
            "--selfplay-dirichlet-eps <F>     Noise mixing fraction (default: 0.25)\n";
}
