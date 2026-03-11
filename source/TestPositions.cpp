#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "TestPositions.h"

#include "CLIUtils.h"
#include "Move.h"
#include "MoveGenerator.h"
#include "Parser.h"
#include "Search.h"

using std::cout;
using std::endl;
using std::ifstream;
using std::setprecision;
using std::stoi;
using std::string;
using std::tuple;
using std::vector;

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
    long long total_nodes = 0;
    clock_t wall_start = clock();
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
                auto best_move_opt = Parser::parse_san(move_str, board);
                assert(best_move_opt.has_value());
                best_moves.push_back({ *best_move_opt, best_move_score });
            }
        }
        else if (!bm.empty())
        {
            // Get best move (opcode "bm")
            auto best_move_opt = Parser::parse_san(bm, board);
            assert(best_move_opt.has_value());
            best_moves.push_back({ *best_move_opt, 1 });
            max_score++;
        }
        else
        {
            cout << "Error: No best move found" << endl;
            return 0;
        }

        // Search with max depth, infinite time, max 1000000 moves searched
        Search search(board);
        Move_t move = search.search(MAX_SEARCH_PLY, -1, TEST_POSITIONS_MAX_NODES_VISITED);
        total_nodes += search.get_stats().nodes_visited;
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

    clock_t wall_end = clock();
    double total_time_secs = static_cast<double>(wall_end - wall_start) / CLOCKS_PER_SEC;
    int nps = (total_time_secs > 0.0)
        ? static_cast<int>(static_cast<double>(total_nodes) / total_time_secs)
        : 0;
    cout << "NPS=" << nps << endl;
    cout << "Nodes=" << total_nodes << " Time=" << std::fixed << std::setprecision(2)
         << total_time_secs << "s" << endl;

    return double(score) / max_score;
}

double test_positions_benchmark(string path_to_epd)
{
    clock_t tic = clock();
    double score = test_positions(path_to_epd);
    clock_t toc = clock();

    long double elapsed_secs = static_cast<long double>(toc - tic) / CLOCKS_PER_SEC;
    cout << "time: " << elapsed_secs << "s" << endl;
    return score;
}

// Extract STS category from id string like "STS(v1.0) Undermine.001"
static string extract_category(const string& id_str)
{
    // Find ") " which precedes the category name
    auto pos = id_str.find(") ");
    if (pos == string::npos)
        return "";
    string rest = id_str.substr(pos + 2);
    // Category is everything before the last '.'
    auto dot = rest.rfind('.');
    if (dot == string::npos)
        return rest;
    return rest.substr(0, dot);
}

// Parse best moves from an EPD line (shared logic)
static bool parse_best_moves(Board& board, vector<tuple<Move_t, int>>& best_moves, int& max_score)
{
    string bm = board.epd_op("bm");
    string c0 = board.epd_op("c0");
    string c7 = board.epd_op("c7");
    string c8 = board.epd_op("c8");
    string c9 = board.epd_op("c9");

    if (!c0.empty() && !c7.empty() && !c8.empty() && !c9.empty())
    {
        vector<string> tokens = split(c0, ' ');
        size_t num_tokens = tokens.size();

        for (size_t i = 0; i < num_tokens; i++)
        {
            string token = tokens[i];
            size_t len = token.length();
            size_t pos = 0;
            if (token[pos] == '"')
            {
                if (pos++ == len)
                    return false;
            }
            string move_str;
            char c = token[pos];
            while (c != '=')
            {
                move_str += c;
                if (pos++ == len)
                    return false;
                c = token[pos];
            }
            string score_str;
            if (pos++ == len)
                return false;
            c = token[pos];
            while (c != ',' && c != '"')
            {
                score_str += c;
                if (pos++ == len)
                    return false;
                c = token[pos];
            }
            int best_move_score = stoi(score_str);
            if (i == 0)
            {
                max_score += best_move_score;
            }
            auto best_move_opt = Parser::parse_san(move_str, board);
            assert(best_move_opt.has_value());
            best_moves.push_back({ *best_move_opt, best_move_score });
        }
    }
    else if (!bm.empty())
    {
        auto best_move_opt = Parser::parse_san(bm, board);
        assert(best_move_opt.has_value());
        best_moves.push_back({ *best_move_opt, 1 });
        max_score++;
    }
    else
    {
        return false;
    }
    return true;
}

BenchmarkResult test_positions_ex(const string& path_to_epd, SearchMode mode, int param)
{
    BenchmarkResult result;

    ifstream infile(path_to_epd);
    if (!infile.is_open())
    {
        cout << "Error: Could not open " << path_to_epd << endl;
        return result;
    }

    clock_t wall_start = clock();
    string line;

    while (std::getline(infile, line))
    {
        Board board = Parser::parse_epd(line);

        // Extract category from id field
        string id_str = board.epd_op("id");
        string category = extract_category(id_str);

        // Parse best moves
        vector<tuple<Move_t, int>> best_moves;
        int line_max = 0;
        if (!parse_best_moves(board, best_moves, line_max))
        {
            cout << "Error: No best move found in: " << id_str << endl;
            continue;
        }
        result.max_score += line_max;

        // Search based on mode
        Search search(board);
        Move_t move = 0;
        switch (mode)
        {
            case SearchMode::FixedNodes:
                move = search.search(MAX_SEARCH_PLY, -1, param);
                break;
            case SearchMode::FixedDepth:
                move = search.search(param, -1, -1);
                break;
            case SearchMode::FixedTime:
                // param is milliseconds, search() expects microseconds
                move = search.search(MAX_SEARCH_PLY, param * 1000, -1);
                break;
        }

        result.total_nodes += search.get_stats().nodes_visited;

        // Score the move
        int move_score = 0;
        for (const auto& bm : best_moves)
        {
            if (move == std::get<0>(bm))
            {
                move_score = std::get<1>(bm);
                break;
            }
        }
        result.score += move_score;

        // Track per-category
        if (!category.empty())
        {
            auto& cat = result.categories[category];
            cat.score += move_score;
            if (!best_moves.empty())
            {
                cat.max_score += std::get<1>(best_moves[0]);  // max is first entry
            }
            cat.positions++;
        }
    }

    clock_t wall_end = clock();
    result.total_time_secs = static_cast<double>(wall_end - wall_start) / CLOCKS_PER_SEC;

    if (result.max_score > 0)
    {
        result.score_pct = (100.0 * result.score) / result.max_score;
        result.elo = static_cast<int>(44.523 * result.score_pct - 242.85);
    }
    if (result.total_time_secs > 0.0)
    {
        result.nps =
            static_cast<int>(static_cast<double>(result.total_nodes) / result.total_time_secs);
    }

    // Print summary
    cout << "Score=" << std::fixed << setprecision(2) << result.score_pct << "% (" << result.score
         << "/" << result.max_score << ")" << endl;
    cout << "ELO=" << result.elo << endl;
    cout << "NPS=" << result.nps << endl;
    cout << "Nodes=" << result.total_nodes << " Time=" << std::fixed << setprecision(2)
         << result.total_time_secs << "s" << endl;

    // Print per-category breakdown if any
    if (!result.categories.empty())
    {
        cout << endl << "Category breakdown:" << endl;
        for (const auto& [name, cat] : result.categories)
        {
            double pct = (cat.max_score > 0) ? (100.0 * cat.score) / cat.max_score : 0.0;
            cout << "  " << name << ": " << std::fixed << setprecision(1) << pct << "% ("
                 << cat.score << "/" << cat.max_score << ", " << cat.positions << " pos)" << endl;
        }
    }

    return result;
}
