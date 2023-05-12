/*
 * File:   main.cpp
 *
 */

#include <cstdlib>
#include <ctime>
#include <iostream>
#include <string>

#include "Board.h"
#include "MoveGenerator.h"
#include "MoveList.h"
#include "Output.h"
#include "Parser.h"
#include "ValidateMove.h"

using namespace std;
#define SEARCH_DEPTH 64

static void print_help(void);

vector<string> split(const string& s, const char delimiter)
{
   vector<string> tokens;
   string token;
   istringstream tokenStream(s);
   while (getline(tokenStream, token, delimiter))
   {
      tokens.push_back(token);
   }
   return tokens;
}

int main()
{
    bool computer_plays[2] = { false };

    string fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    Board board = Parser::parse_fen(fen);

    while (true)
    {
        cout << Output::board(board);
        if (board.is_game_over())
        {
            if (board.side_to_move() == WHITE)
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
            move = board.search(SEARCH_DEPTH);
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
            Move_t move = Parser::move(line.substr(5), board);
            if (is_valid_move(move, board))
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
