/*
 * File:   main.cpp
 *
 */

#include <cstdlib>
#include <ctime>
#include <iostream>
#include <string>

#ifdef _WIN32
#    include <windows.h>
#endif

#include "Board.h"
#include "Book.h"
#include "CLIUtils.h"
#include "CmdLineArgs.h"
#include "MoveGenerator.h"
#include "MoveList.h"
#include "Output.h"
#include "Parser.h"
#include "Perft.h"
#include "Search.h"
#include "TestPositions.h"
#include "ValidateMove.h"
#include "Xboard.h"

using std::cin;
using std::cout;
using std::endl;
using std::string;
using std::vector;

static void print_help(void);
static void usage(const string& prog_name);

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

    // Parse book configuration (shared across all modes)
    Book book;
    bool book_enabled = false;
    if (!cmd_line_args.cmd_option_exists("--no-book") && cmd_line_args.cmd_option_exists("--book"))
    {
        string book_path = cmd_line_args.get_cmd_option("--book");
        if (!book_path.empty() && book.open(book_path))
        {
            book_enabled = true;
        }
    }
    if (cmd_line_args.cmd_option_exists("--book-depth"))
    {
        string depth_str = cmd_line_args.get_cmd_option("--book-depth");
        if (!depth_str.empty())
        {
            book.set_max_depth(std::stoi(depth_str));
        }
    }

    if (cmd_line_args.cmd_option_exists("--xboard"))
    {
        Xboard xboard;
        if (book_enabled)
        {
            xboard.set_book(std::move(book));
        }
        xboard.run();
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

    bool computer_plays[2] = { false };
    int game_ply = 0;

    string fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    Board board = Parser::parse_fen(fen);
    Search search(board, board.get_evaluator(), board.get_tt());

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
            // Check opening book first
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

            Move_t move;
            move = search.search(MAX_SEARCH_PLY);
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
            "--test-positions path-to-epd     Run test positions\n"
            "--book <path>                    Use opening book at <path>\n"
            "--no-book                        Disable opening book\n"
            "--book-depth <N>                 Stop using book after N plies\n";
}
