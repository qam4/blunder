/*
 * File:   main.cpp
 *
 */

#include <cstdlib>
#include <ctime>
#include <iostream>
#include <string>

#include "Board.h"
#include "CLIUtils.h"
#include "CmdLineArgs.h"
#include "MoveGenerator.h"
#include "MoveList.h"
#include "Output.h"
#include "Parser.h"
#include "Perft.h"
#include "ValidateMove.h"
#include "Xboard.h"

using namespace std;

static void print_help(void);
static void usage(const string& prog_name);

int main(int argc, char** argv)
{
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

    if (cmd_line_args.cmd_option_exists("--xboard"))
    {
        Xboard xboard;
        xboard.run();
        return 0;
    }

    bool computer_plays[2] = { false };

    string fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    Board board = Parser::parse_fen(fen);

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
            clock_t tic = clock();
            cout << "Thinking..." << endl;

            Move_t move;
            move = board.search(MAX_SEARCH_PLY);
            clock_t toc = clock();
            double elapsed_secs = double(toc - tic) / CLOCKS_PER_SEC;
            cout << "time: " << elapsed_secs << "s" << endl;
            cout << "searched moves: " << board.get_searched_moves() << endl;
            cout << "searched moves per second: " << int(board.get_searched_moves() / elapsed_secs)
                 << endl;

            cout << "Computer move: " << Output::move(move, board) << endl;
            board.do_move(move);
            continue;
        }

        string line;
        cout << "> ";
        getline(cin, line);

        vector<string> tokens = split(line, ' ');
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
            Move_t move = Parser::move(tokens[1], board);
            if (is_valid_move(move, board, true))
            {
                board.do_move(move);
            }
        }
        else if (tokens[0] == "play")
        {
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
            if (tokens[1] == "on")
            {
                Output::set_colors_enabled(true);
            }
            else if (tokens[1] == "off")
            {
                Output::set_colors_enabled(false);
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
            "  colors <on|off>\n";
}

void usage(const string& prog_name)
{
    cout << prog_name
         << "\n"
            "Options:\n"
            "-h|--help           Print this help\n"
            "--gen-lookup-tables Generate lookup tables\n"
            "--perft             Run perft benchmark\n"
            "--xboard            xboard interface\n";
}
