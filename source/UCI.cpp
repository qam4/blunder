/*
 * File:   UCI.cpp
 *
 * UCI protocol implementation.
 */

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "UCI.h"

#include "MCTS.h"
#include "Output.h"
#include "Parser.h"
#include "ValidateMove.h"

UCI::UCI()
    : search_(board_)
    , coach_dispatcher_(board_, search_, nullptr)
{
    init_handlers();
}

UCI::~UCI()
{
    if (search_thread_.joinable())
    {
        search_.set_abort(true);
        search_thread_.join();
    }
}

void UCI::init_handlers()
{
    handlers_["uci"] = [this](const std::string& /*args*/) { cmd_uci(); };
    handlers_["isready"] = [this](const std::string& /*args*/) { cmd_isready(); };
    handlers_["ucinewgame"] = [this](const std::string& /*args*/) { cmd_ucinewgame(); };
    handlers_["position"] = [this](const std::string& args) { cmd_position(args); };
    handlers_["go"] = [this](const std::string& args) { cmd_go(args); };
    handlers_["stop"] = [this](const std::string& /*args*/) { cmd_stop(); };
    handlers_["setoption"] = [this](const std::string& args) { cmd_setoption(args); };
    handlers_["quit"] = [](const std::string& /*args*/) {};  // handled in run()
    handlers_["coach"] = [this](const std::string& args) { coach_dispatcher_.dispatch(args); };
}

void UCI::cmd_uci()
{
    std::cout << "id name blunder" << std::endl;
    std::cout << "id author qam4" << std::endl;
    std::cout << std::endl;
    std::cout << "option name Hash type spin default 16 min 1 max 1024" << std::endl;
    std::cout << "option name Book type check default " << (book_enabled_ ? "true" : "false")
              << std::endl;
    std::cout << "option name Mobility type check default true" << std::endl;
    std::cout << "option name Tempo type check default true" << std::endl;
    std::cout << "option name MultiPV type spin default 1 min 1 max 256" << std::endl;
    std::cout << "uciok" << std::endl;
}

void UCI::cmd_isready()
{
    std::cout << "readyok" << std::endl;
}

void UCI::cmd_ucinewgame()
{
    // Wait for any ongoing search to finish
    if (search_thread_.joinable())
    {
        search_.set_abort(true);
        search_thread_.join();
    }

    board_ = Parser::parse_fen(DEFAULT_FEN);
    board_.get_tt().clear();
    if (nnue_ && nnue_->is_loaded())
    {
        board_.set_nnue(nnue_);
        nnue_->refresh(board_);
    }
    move_nr_ = 0;
}

void UCI::cmd_position(const std::string& args)
{
    std::istringstream iss(args);
    std::string token;
    iss >> token;

    // Parse position
    if (token == "startpos")
    {
        board_ = Parser::parse_fen(DEFAULT_FEN);
    }
    else if (token == "fen")
    {
        // Collect FEN parts (up to 6 tokens before "moves")
        std::string fen;
        while (iss >> token)
        {
            if (token == "moves")
            {
                break;
            }
            if (!fen.empty())
            {
                fen += " ";
            }
            fen += token;
        }
        board_ = Parser::parse_fen(fen);
    }

    if (nnue_ && nnue_->is_loaded())
    {
        board_.set_nnue(nnue_);
        nnue_->refresh(board_);
    }

    // Parse moves
    // If we already consumed "moves" token above, start reading moves.
    // Otherwise, look for "moves" keyword.
    if (token != "moves")
    {
        while (iss >> token)
        {
            if (token == "moves")
            {
                break;
            }
        }
    }

    // Apply moves
    // Reset search_ply before each move to avoid overflowing move_stack_[MAX_SEARCH_PLY].
    // These moves are never undone, so we don't need the saved irreversible state.
    move_nr_ = 0;
    while (iss >> token)
    {
        Move_t move = parse_uci_move(token);
        if (move != Move(0))
        {
            board_.set_search_ply(0);
            board_.do_move(move);
            move_nr_++;
        }
    }
    board_.set_search_ply(0);
}

void UCI::cmd_go(const std::string& args)
{
    // Wait for any previous search to finish
    if (search_thread_.joinable())
    {
        search_.set_abort(true);
        search_thread_.join();
    }

    // Parse go parameters
    std::istringstream iss(args);
    std::string token;

    int depth = MAX_SEARCH_PLY;
    int wtime = -1;
    int btime = -1;
    int winc = 0;
    int binc = 0;
    int movestogo = 0;
    int movetime = -1;
    int nodes = -1;
    bool infinite = false;

    while (iss >> token)
    {
        if (token == "depth")
        {
            iss >> depth;
        }
        else if (token == "wtime")
        {
            iss >> wtime;
        }
        else if (token == "btime")
        {
            iss >> btime;
        }
        else if (token == "winc")
        {
            iss >> winc;
        }
        else if (token == "binc")
        {
            iss >> binc;
        }
        else if (token == "movestogo")
        {
            iss >> movestogo;
        }
        else if (token == "movetime")
        {
            iss >> movetime;
        }
        else if (token == "nodes")
        {
            iss >> nodes;
        }
        else if (token == "infinite")
        {
            infinite = true;
        }
    }

    start_search(depth, wtime, btime, winc, binc, movestogo, movetime, nodes, infinite);
}

void UCI::cmd_stop()
{
    search_.set_abort(true);
    if (search_thread_.joinable())
    {
        search_thread_.join();
    }
}

void UCI::cmd_setoption(const std::string& args)
{
    // Format: "name <name> value <value>"
    std::istringstream iss(args);
    std::string token;
    std::string name;
    std::string value;

    // Skip "name" keyword
    iss >> token;
    if (token != "name")
    {
        return;
    }

    // Read option name (may be multi-word, up to "value" keyword)
    while (iss >> token)
    {
        if (token == "value")
        {
            break;
        }
        if (!name.empty())
        {
            name += " ";
        }
        name += token;
    }

    // Read value
    std::getline(iss, value);
    // Trim leading space
    if (!value.empty() && value[0] == ' ')
    {
        value = value.substr(1);
    }

    if (name == "Hash")
    {
        hash_size_mb_ = std::stoi(value);
        // TODO: resize transposition table when TT supports dynamic sizing
    }
    else if (name == "Book")
    {
        if (value == "false")
        {
            book_enabled_ = false;
        }
        else if (value == "true")
        {
            book_enabled_ = true;
        }
    }
    else if (name == "Mobility")
    {
        EvalConfig cfg = board_.get_hce().config();
        cfg.mobility_enabled = (value == "true");
        board_.get_hce().set_config(cfg);
    }
    else if (name == "Tempo")
    {
        EvalConfig cfg = board_.get_hce().config();
        cfg.tempo_enabled = (value == "true");
        board_.get_hce().set_config(cfg);
    }
    else if (name == "MultiPV")
    {
        int n = std::stoi(value);
        if (n < 1)
            n = 1;
        if (n > 256)
            n = 256;
        multipv_count_ = n;
    }
}

void UCI::start_search(int depth,
                       int wtime,
                       int btime,
                       int winc,
                       int binc,
                       int movestogo,
                       int movetime,
                       int nodes,
                       bool infinite)
{
    searching_ = true;
    search_.set_abort(false);

    search_thread_ = std::thread(
        [this, depth, wtime, btime, winc, binc, movestogo, movetime, nodes, infinite]()
        {
            Move_t best_move = 0;
            Move_t ponder_move = 0;

            // Check opening book first
            if (book_enabled_ && book_.within_depth(move_nr_) && book_.has_move(board_))
            {
                Move_t book_move = book_.get_move(board_);
                if (book_move != Move(0))
                {
                    send_bestmove(book_move, 0);
                    searching_ = false;
                    return;
                }
            }

            // MCTS search path
            if (use_mcts_)
            {
                TimeManager mcts_tm;
                if (movetime > 0)
                {
                    // movetime is in milliseconds, convert to centiseconds for allocate
                    mcts_tm.start(movetime * 1000);  // microseconds
                }
                else if (!infinite)
                {
                    int time_ms = (board_.side_to_move() == WHITE) ? wtime : btime;
                    int inc_ms = (board_.side_to_move() == WHITE) ? winc : binc;
                    if (time_ms > 0)
                    {
                        // allocate expects centiseconds
                        mcts_tm.allocate(time_ms / 10, inc_ms / 10, movestogo);
                    }
                    else
                    {
                        mcts_tm.start(-1, -1);  // no limit
                    }
                }
                else
                {
                    mcts_tm.start(-1, -1);  // infinite
                }

                if (dual_head_ != nullptr && dual_head_->is_loaded())
                {
                    MCTS mcts(board_, *dual_head_, mcts_c_puct_, mcts_simulations_);
                    best_move = mcts.search(&mcts_tm);
                }
                else
                {
                    Evaluator& eval = board_.get_evaluator();
                    MCTS mcts(board_, eval, mcts_c_puct_, mcts_simulations_);
                    best_move = mcts.search(&mcts_tm);
                }

                send_bestmove(best_move, 0);
                searching_ = false;
                return;
            }

            // Alpha-beta search path
            if (infinite)
            {
                search_.get_tm().start(-1, -1);  // no time limit
            }
            else if (movetime > 0)
            {
                // movetime in ms → microseconds
                search_.get_tm().start(movetime * 1000);
            }
            else
            {
                // Clock-based time management
                int time_ms = (board_.side_to_move() == WHITE) ? wtime : btime;
                int inc_ms = (board_.side_to_move() == WHITE) ? winc : binc;
                if (time_ms > 0)
                {
                    search_.get_tm().allocate(time_ms / 10, inc_ms / 10, movestogo);
                }
                else
                {
                    search_.get_tm().start(-1, nodes);
                }
            }

            search_.set_verbose(true);
            search_.set_output_mode(Search::OutputMode::UCI);
            best_move = search_.search(depth, -1, nodes, false, multipv_count_);

            // Extract bestmove and ponder from top PV line
            const auto& results = search_.get_multipv_results();
            if (!results.empty())
            {
                best_move = results[0].best_move();
                ponder_move = results[0].ponder_move();
            }

            send_bestmove(best_move, ponder_move);
            searching_ = false;
        });
}

void UCI::send_bestmove(Move_t move, Move_t ponder_move)
{
    std::string out = "bestmove " + move_to_uci(move);
    if (ponder_move != Move(0) && ponder_move != Move(0x9999))
    {
        out += " ponder " + move_to_uci(ponder_move);
    }
    std::cout << out << std::endl;
}

void UCI::send_info(
    int depth, int score_cp, int nodes_count, int nps, int time_ms, const std::string& pv)
{
    std::cout << "info depth " << depth << " score cp " << score_cp << " nodes " << nodes_count
              << " nps " << nps << " time " << time_ms << " pv " << pv << std::endl;
}

std::string UCI::move_to_uci(Move_t move) const
{
    if (move == Move(0))
    {
        return "0000";
    }

    Move m(move);

    // Castling: from/to are 0 in the encoding, output king's actual squares
    if (m.is_castle())
    {
        // Determine side from board state at time of move
        bool white = (board_.side_to_move() == WHITE);
        if (m.flags() & (KING_CASTLE >> FLAGS_SHIFT))
        {
            return white ? "e1g1" : "e8g8";
        }
        else
        {
            return white ? "e1c1" : "e8c8";
        }
    }

    std::string result;

    int from = m.from();
    int to = m.to();

    result += static_cast<char>('a' + (from % 8));
    result += static_cast<char>('1' + (from / 8));
    result += static_cast<char>('a' + (to % 8));
    result += static_cast<char>('1' + (to / 8));

    // Add promotion piece
    if (m.is_promotion())
    {
        U8 promoted = m.promote_to();
        switch (promoted)
        {
            case QUEEN:
                result += 'q';
                break;
            case ROOK:
                result += 'r';
                break;
            case BISHOP:
                result += 'b';
                break;
            case KNIGHT:
                result += 'n';
                break;
            default:
                result += 'q';
                break;
        }
    }

    return result;
}

Move_t UCI::parse_uci_move(const std::string& move_str)
{
    // UCI moves are in format: e2e4, e7e8q (with optional promotion)
    // Use Parser::move which handles coordinate notation
    auto opt = Parser::move(move_str, board_);
    if (opt && is_valid_move(*opt, board_, true))
    {
        return *opt;
    }
    return 0;
}

void UCI::run()
{
    std::string line;
    while (std::getline(std::cin, line))
    {
        if (line.empty())
        {
            continue;
        }

        std::istringstream iss(line);
        std::string command;
        iss >> command;

        if (command == "quit")
        {
            cmd_stop();
            break;
        }

        std::string args;
        if (iss.peek() == ' ')
        {
            iss.get();
        }
        std::getline(iss, args);

        auto it = handlers_.find(command);
        if (it != handlers_.end())
        {
            it->second(args);
        }
        // Unknown commands are silently ignored per UCI spec
    }
}
