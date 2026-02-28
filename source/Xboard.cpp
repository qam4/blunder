/*
    Xboard.cpp
*/

#include <iostream>
#include <sstream>
#include <string>

#include "Xboard.h"

#include "Output.h"
#include "Parser.h"
#include "ValidateMove.h"

Xboard::Xboard()
    : search_(board_, board_.get_evaluator(), board_.get_tt())
{
    init_handlers();
}

int Xboard::make_move(int stm, Move_t move)
{
    (void)stm;
    board_.do_move(move);
    return board_.side_to_move();
}

void Xboard::un_make(Move_t move)
{
    board_.undo_move(move);
}

int Xboard::setup(const std::string& fen)
{
    if (!fen.empty())
    {
        setup_fen_ = fen;
    }
    board_ = Parser::parse_fen(setup_fen_);
    board_.get_tt().clear();
    return board_.side_to_move();
}

void Xboard::set_memory_size(int n)
{
    (void)n;
}

std::string Xboard::move_to_text(Move_t move)
{
    return Output::move(move, board_);
}

Move_t Xboard::parse_move(const std::string& move_text)
{
    auto opt = Parser::move(move_text, board_);
    if (opt && is_valid_move(*opt, board_, true))
    {
        return *opt;
    }
    return INVALID_MOVE;
}

int Xboard::search_best_move(int stm,
                             int time_left,
                             int mps,
                             int time_control,
                             float inc,
                             int time_per_move,
                             int max_depth,
                             Move_t* move,
                             Move_t* ponder_move)
{
    (void)stm;
    (void)mps;
    (void)time_control;
    (void)time_per_move;
    (void)ponder_move;

    // Check the opening book first
    if (book_enabled_ && book_.within_depth(move_nr_) && book_.has_move(board_))
    {
        Move_t book_move = book_.get_move(board_);
        if (book_move != Move(0))
        {
            *move = book_move;
            return 0;  // book move, score is 0
        }
    }

    // https://mediocrechess.blogspot.com/2007/01/guide-time-management.html
    // Use 2.25% of the time + half of the increment
    int time_for_this_move = time_left / 40 + (static_cast<int>(inc * 100) / 2);

    // If the increment puts us above the total time left
    // use the timeleft - 0.5 seconds
    if (time_for_this_move >= time_left)
    {
        time_for_this_move = time_left - 500;
    }

    // If 0.5 seconds puts us below 0
    // use 0.1 seconds to at least get some move.
    if (time_for_this_move < 0)
    {
        time_for_this_move = 100;
    }

    Move_t best_move = search_.search(max_depth, time_for_this_move * 10000, -1, true);
    *move = best_move;

    return search_.get_search_best_score();
}

void Xboard::ponder_until_input(int stm)
{
    (void)stm;
}

int Xboard::take_back(int n)
{
    int stm = setup("");
    int last = move_nr_ - n;
    if (last < 0)
    {
        last = 0;
    }
    for (move_nr_ = 0; move_nr_ < last; move_nr_++)
    {
        stm = make_move(stm, game_move_[move_nr_]);
    }
    return stm;
}

void Xboard::print_result(int stm, int score)
{
    if (score == 0)
    {
        std::cout << "1/2-1/2" << std::endl;
    }
    if (((score > 0) && (stm == WHITE)) || ((score < 0) && (stm == BLACK)))
    {
        std::cout << "1-0" << std::endl;
    }
    else
    {
        std::cout << "0-1" << std::endl;
    }
}

void Xboard::init_handlers()
{
    // Commands that just set engine_side
    handlers_["force"] = [](const std::string& /*args*/, RunState& rs)
    { rs.engine_side = STM_NONE; };
    handlers_["result"] = [](const std::string& /*args*/, RunState& rs)
    { rs.engine_side = STM_NONE; };
    handlers_["analyze"] = [](const std::string& /*args*/, RunState& rs)
    { rs.engine_side = STM_ANALYZE; };
    handlers_["exit"] = [](const std::string& /*args*/, RunState& rs)
    { rs.engine_side = STM_NONE; };

    // Time commands — skip pondering so move follows immediately
    handlers_["otim"] = [](const std::string& /*args*/, RunState& rs) { rs.skip_ponder = true; };
    handlers_["time"] = [](const std::string& args, RunState& rs)
    {
        std::istringstream iss(args);
        iss >> rs.time_left;
        rs.skip_ponder = true;
    };

    handlers_["level"] = [](const std::string& args, RunState& rs)
    {
        std::istringstream iss(args);
        int min = 0;
        int sec = 0;
        std::string time_str;
        iss >> rs.mps >> time_str >> rs.inc;
        auto colon = time_str.find(':');
        if (colon != std::string::npos)
        {
            min = std::stoi(time_str.substr(0, colon));
            sec = std::stoi(time_str.substr(colon + 1));
        }
        else
        {
            min = std::stoi(time_str);
        }
        rs.time_control = 60 * min + sec;
        rs.time_per_move = -1;
    };

    handlers_["protover"] = [](const std::string& /*args*/, RunState& /*rs*/)
    {
        std::cout << "feature done=0 myname=\"blunder\" ping=1 memory=1 setboard=1 debug=1"
                     " sigint=0 sigterm=0"
                  << std::endl;
        std::cout << "feature name=1 ics=1" << std::endl;
        std::cout << "feature usermove=1" << std::endl;
        std::cout << "feature done=1" << std::endl;
    };

    handlers_["option"] = [this](const std::string& args, RunState& /*rs*/)
    {
        std::istringstream iss(args);
        std::string opt;
        iss >> opt;
        if (opt.find("Resign=") == 0)
        {
            resign_ = std::stoi(opt.substr(7));
        }
        else if (opt.find("Contempt=") == 0)
        {
            contempt_factor_ = std::stoi(opt.substr(9));
        }
    };

    handlers_["sd"] = [](const std::string& args, RunState& rs)
    {
        std::istringstream iss(args);
        iss >> rs.max_depth;
    };
    handlers_["st"] = [](const std::string& args, RunState& rs)
    {
        std::istringstream iss(args);
        iss >> rs.time_per_move;
    };

    handlers_["memory"] = [this](const std::string& args, RunState& /*rs*/)
    {
        std::istringstream iss(args);
        int n = 0;
        iss >> n;
        set_memory_size(n);
    };

    handlers_["ping"] = [](const std::string& args, RunState& /*rs*/)
    { std::cout << "pong " << args << std::endl; };

    handlers_["new"] = [this](const std::string& /*args*/, RunState& rs)
    {
        rs.engine_side = BLACK;
        rs.stm = setup(DEFAULT_FEN);
        rs.max_depth = MAX_SEARCH_PLY;
        randomize_ = OFF;
        move_nr_ = 0;
    };

    handlers_["setboard"] = [this](const std::string& args, RunState& rs)
    {
        rs.engine_side = STM_NONE;
        rs.stm = setup(args);
    };

    handlers_["easy"] = [this](const std::string& /*args*/, RunState& /*rs*/) { ponder_ = OFF; };
    handlers_["hard"] = [this](const std::string& /*args*/, RunState& /*rs*/) { ponder_ = ON; };

    handlers_["undo"] = [this](const std::string& /*args*/, RunState& rs)
    { rs.stm = take_back(1); };
    handlers_["remove"] = [this](const std::string& /*args*/, RunState& rs)
    { rs.stm = take_back(2); };

    handlers_["go"] = [](const std::string& /*args*/, RunState& rs) { rs.engine_side = rs.stm; };

    handlers_["post"] = [this](const std::string& /*args*/, RunState& /*rs*/)
    { post_thinking_ = ON; };
    handlers_["nopost"] = [this](const std::string& /*args*/, RunState& /*rs*/)
    { post_thinking_ = OFF; };
    handlers_["random"] = [this](const std::string& /*args*/, RunState& /*rs*/)
    { randomize_ = ON; };

    handlers_["hint"] = [this](const std::string& /*args*/, RunState& rs)
    {
        if (rs.ponder_move != INVALID_MOVE)
        {
            std::cout << "Hint: " << move_to_text(rs.ponder_move) << std::endl;
        }
    };

    handlers_["usermove"] = [this](const std::string& args, RunState& rs)
    {
        rs.move = parse_move(args);
        std::cout << "# parsed usermove=" << Output::move_san(rs.move, board_) << std::endl;
        if (rs.move == INVALID_MOVE)
        {
            std::cout << "Illegal move" << std::endl;
        }
        else
        {
            rs.stm = make_move(rs.stm, rs.move);
            rs.ponder_move = INVALID_MOVE;
            assert(move_nr_ < MAXMOVES);
            game_move_[move_nr_++] = rs.move;
        }
    };

    // Ignored commands — no-ops
    for (const auto& cmd : { "book",
                             "xboard",
                             "computer",
                             "name",
                             "ics",
                             "accepted",
                             "rejected",
                             "variant",
                             "?",
                             "" })
    {
        handlers_[cmd] = [](const std::string& /*args*/, RunState& /*rs*/) {};
    }
}

void Xboard::run()
{
    RunState rs;

    while (!rs.quit)
    {
        std::cout << "# FEN: " << Output::board_to_fen(board_) << std::endl;
        std::cout.flush();

        if (rs.stm == rs.engine_side)
        {
            std::cout << "# Thinking..." << std::endl;
            int score = search_best_move(rs.stm,
                                         rs.time_left,
                                         rs.mps,
                                         rs.time_control,
                                         rs.inc,
                                         rs.time_per_move,
                                         rs.max_depth,
                                         &rs.move,
                                         &rs.ponder_move);

            if (!is_valid_move(rs.move, board_, true))
            {
                std::cout << "# Invalid move" << std::endl;
                rs.engine_side = STM_NONE;
                print_result(rs.stm, score);
            }
            else
            {
                rs.stm = make_move(rs.stm, rs.move);
                assert(move_nr_ < MAXMOVES);
                game_move_[move_nr_++] = rs.move;
                std::cout << "move " << move_to_text(rs.move) << std::endl;
            }
        }

        std::cout.flush();

        // Pondering
        rs.skip_ponder = false;
        if (rs.engine_side == STM_ANALYZE)
        {
            ponder_until_input(rs.stm);
        }
        else if (rs.engine_side != STM_NONE && ponder_ == ON && move_nr_ != 0)
        {
            if (rs.ponder_move == INVALID_MOVE)
            {
                ponder_until_input(rs.stm);
            }
            else
            {
                int new_stm = make_move(rs.stm, rs.ponder_move);
                ponder_until_input(new_stm);
                un_make(rs.ponder_move);
            }
        }

        if (rs.skip_ponder)
        {
            // Time commands already set skip_ponder; fall through to read input
        }

        // Read a line of input
        std::string line;
        if (!std::getline(std::cin, line))
        {
            break;
        }

        // Extract command and arguments
        std::istringstream iss(line);
        std::string command;
        iss >> command;
        std::string args;
        if (iss.peek() == ' ')
        {
            iss.get();
        }
        std::getline(iss, args);

        std::cout << "# input command: " << command << std::endl;

        if (command == "quit")
        {
            break;
        }

        auto it = handlers_.find(command);
        if (it != handlers_.end())
        {
            it->second(args, rs);
        }
        else
        {
            std::cout << "Error: (unknown command): " << command << std::endl;
        }
    }
}
