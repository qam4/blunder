#include <fstream>
#include <iomanip>
#include <iostream>

#include "TestPositions.h"

#include "CLIUtils.h"
#include "Move.h"
#include "MoveGenerator.h"
#include "Parser.h"

// Test positions
// https://www.chessprogramming.org/Test-Positions

const int TEST_POSITIONS_MAX_NODES_VISITED = 1000000;

double test_positions(string path_to_epd)
{
    std::ifstream infile(path_to_epd);
    if (!infile.is_open())
    {
        cout << "Error: Could not open " << path_to_epd << endl;
        return 0;
    }

    std::string line;
    int score = 0;
    int max_score = 0;
    while (std::getline(infile, line))
    {
        std::istringstream iss(line);
        Board board = Parser::parse_epd(line);

        // Get list of best moves with associated score
        std::vector<tuple<Move_t, int>> best_moves;

        // https://github.com/fsmosca/STS-Rating
        // Format:
        // c0 "d4=100, h3=67, Bh3=45, h4=26, a3=24, Kg1=21, Be3=12, Bf1=12, a4=6, cxb4=1";
        // c7 "d4 h3 Bh3 h4 a3 Kg1 Be3 Bf1 a4 cxb4";
        // c8 "100 67 45 26 24 21 12 12 6 1";
        // c9 "d3d4 h2h3 g2h3 h2h4 a2a3 h1g1 c1e3 g2f1 a2a4 c3b4"
        string bm = board.epd_op("bm");
        string c0 = board.epd_op("c0");
        string c7 = board.epd_op("c7");
        string c8 = board.epd_op("c7");
        string c9 = board.epd_op("c9");
        if (!c0.empty() && !c7.empty() && !c8.empty() && !c9.empty())
        {
            vector<string> tokens = split(c0, ' ');
            size_t num_tokens = tokens.size();

            size_t pos;  // position in string
            string token;
            for (size_t i = 0; i < num_tokens; i++)
            {
                token = tokens[i];
                size_t len = token.length();
                pos = 0;
                if (token[pos] == '"')
                {
                    if (pos++ == len)
                        return 0;
                }
                string move_str;
                char c = token[pos];
                while (c != '=')
                {
                    move_str += c;
                    if (pos++ == len)
                        return 0;
                    c = token[pos];
                }
                string score_str;
                if (pos++ == len)
                    return 0;
                c = token[pos];
                while (c != ',' && c != '"')
                {
                    score_str += c;
                    if (pos++ == len)
                        return 0;
                    c = token[pos];
                }
                int best_move_score = std::stoi(score_str);
                if (i == 0)
                {
                    max_score += best_move_score;
                }
                Move_t best_move = Parser::parse_san(move_str, board);
                assert(best_move != 0);
                best_moves.push_back({ best_move, best_move_score });
            }
        }
        else if (!bm.empty())
        {
            // Get best move (opcode "bm")
            Move_t best_move = Parser::parse_san(bm, board);
            assert(best_move != 0);
            best_moves.push_back({ best_move, 1 });
            max_score++;
        }
        else
        {
            cout << "Error: No best move found" << endl;
            return 0;
        }

        // Search with max depth, infinite time, max 1000000 moves searched
        Move_t move = board.search(MAX_SEARCH_PLY, -1, TEST_POSITIONS_MAX_NODES_VISITED);
        for (auto best_move : best_moves)
        {
            if (move == std::get<0>(best_move))
            {
                score += std::get<1>(best_move);
            }
        }
    }

    // ELO estimate, using https://github.com/fsmosca/STS-Rating/blob/master/sts_rating.py
    double score_percent = (100.0 * score) / max_score;
    double elo_estimate = 44.523 * score_percent - 242.85;

    cout << "Score=" << std::fixed << std::setprecision(2) << score_percent << "% (" << score << "/"
         << max_score << ")" << endl;
    cout << "ELO=" << int(elo_estimate) << endl;
    return double(score) / max_score;
}

double test_positions_benchmark(string path_to_epd)
{
    clock_t tic = clock();
    double score = test_positions(path_to_epd);
    clock_t toc = clock();

    int elapsed_secs = int(double(toc - tic) / CLOCKS_PER_SEC);
    cout << "time: " << elapsed_secs << "s" << endl;
    return score;
}
