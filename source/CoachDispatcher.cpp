/*
 * File:   CoachDispatcher.cpp
 *
 * Implementation of the CoachDispatcher — routes `coach` subcommands
 * and owns response framing (markers + JSON envelope).
 */

#include <iostream>
#include <sstream>
#include <string>

#include "CoachDispatcher.h"

#include "Board.h"
#include "CoachJson.h"
#include "MoveComparator.h"
#include "NNUEEvaluator.h"
#include "Parser.h"
#include "PositionAnalyzer.h"
#include "Search.h"
#include "ValidateMove.h"

CoachDispatcher::CoachDispatcher(Board& board, Search& search, NNUEEvaluator* nnue)
    : board_(board)
    , search_(search)
    , nnue_(nnue)
{
}

void CoachDispatcher::dispatch(const std::string& args)
{
    // Coaching analysis always runs at full strength — temporarily disable
    // skill noise so eval/compare results are accurate.
    bool was_analysis = search_.is_analysis_mode();
    search_.set_analysis_mode(true);

    try
    {
        std::istringstream iss(args);
        std::string subcommand;
        iss >> subcommand;

        std::string remaining;
        if (iss.peek() == ' ')
            iss.get();
        std::getline(iss, remaining);

        if (subcommand == "ping")
            cmd_ping();
        else if (subcommand == "eval")
            cmd_eval(remaining);
        else if (subcommand == "compare")
            cmd_compare(remaining);
        else
            send_error("unknown_command", "Unknown coaching command: " + subcommand);
    } catch (const std::exception& e)
    {
        send_error("internal_error", e.what());
    } catch (...)
    {
        send_error("internal_error", "Unknown error occurred");
    }

    search_.set_analysis_mode(was_analysis);
}

// ---------------------------------------------------------------------------
// Stub command handlers — filled in by tasks 6.2, 6.3, 6.4
// ---------------------------------------------------------------------------

void CoachDispatcher::cmd_ping()
{
    std::string data = CoachJson::serialize_pong("Blunder", "0.8.0");
    send_response(wrap_envelope("pong", data));
}

void CoachDispatcher::cmd_eval(const std::string& args)
{
    std::istringstream iss(args);
    std::string token;
    std::string fen;
    int multipv = 3;
    int depth = 12;
    int movetime = -1;  // milliseconds, -1 = not specified

    // Expect "fen" keyword
    iss >> token;
    if (token != "fen")
    {
        send_error("invalid_fen", "Expected 'fen' keyword");
        return;
    }

    // Collect FEN parts and optional parameters
    while (iss >> token)
    {
        if (token == "multipv")
        {
            iss >> multipv;
        }
        else if (token == "depth")
        {
            iss >> depth;
        }
        else if (token == "movetime")
        {
            iss >> movetime;
        }
        else
        {
            if (!fen.empty())
                fen += " ";
            fen += token;
        }
    }

    if (fen.empty())
    {
        send_error("invalid_fen", "No FEN string provided");
        return;
    }

    // Validate and set up position
    try
    {
        board_ = Parser::parse_fen(fen);
    } catch (...)
    {
        send_error("invalid_fen", "Could not parse FEN: '" + fen + "'");
        return;
    }

    // Validate board has both kings (parse_fen doesn't throw on garbage input)
    if (pop_count(board_.bitboard(WHITE_KING)) != 1 || pop_count(board_.bitboard(BLACK_KING)) != 1)
    {
        send_error("invalid_fen", "Could not parse FEN: '" + fen + "'");
        return;
    }

    // Set up NNUE if available
    if (nnue_ && nnue_->is_loaded())
    {
        board_.set_nnue(nnue_);
        nnue_->refresh(board_);
    }

    // Run MultiPV search with caller-specified limits
    search_.set_verbose(false);
    search_.set_output_mode(Search::OutputMode::NORMAL);
    search_.set_abort(false);
    if (movetime > 0)
    {
        search_.get_tm().start(movetime * 1000);  // ms → µs
    }
    else
    {
        search_.get_tm().start(-1, -1);  // no time limit, depth-limited
    }
    search_.search(depth, -1, -1, false, multipv);

    const auto& raw_lines = search_.get_multipv_results();
    std::vector<PVLine> pv_lines;
    pv_lines.reserve(raw_lines.size());
    for (const auto& line : raw_lines)
    {
        if (!line.moves.empty())
        {
            pv_lines.push_back(line);
        }
    }

    // Analyze position
    PositionReport report = PositionAnalyzer::analyze(board_, pv_lines);

    // Override FEN with the input FEN
    report.fen = fen;

    // Serialize and send
    std::string data = CoachJson::serialize_position_report(report);
    send_response(wrap_envelope("position_report", data));
}

void CoachDispatcher::cmd_compare(const std::string& args)
{
    std::istringstream iss(args);
    std::string token;
    std::string fen;
    std::string move_str;
    int depth = 12;
    int movetime = -1;  // milliseconds, -1 = not specified

    // Expect "fen" keyword
    iss >> token;
    if (token != "fen")
    {
        send_error("invalid_fen", "Expected 'fen' keyword");
        return;
    }

    // Collect FEN parts and optional parameters
    while (iss >> token)
    {
        if (token == "move")
        {
            iss >> move_str;
        }
        else if (token == "depth")
        {
            iss >> depth;
        }
        else if (token == "movetime")
        {
            iss >> movetime;
        }
        else
        {
            if (!fen.empty())
                fen += " ";
            fen += token;
        }
    }

    if (fen.empty())
    {
        send_error("invalid_fen", "No FEN string provided");
        return;
    }

    if (move_str.empty())
    {
        send_error("invalid_move", "No move provided");
        return;
    }

    // Validate and set up position
    try
    {
        board_ = Parser::parse_fen(fen);
    } catch (...)
    {
        send_error("invalid_fen", "Could not parse FEN: '" + fen + "'");
        return;
    }

    // Validate board has both kings
    if (pop_count(board_.bitboard(WHITE_KING)) != 1 || pop_count(board_.bitboard(BLACK_KING)) != 1)
    {
        send_error("invalid_fen", "Could not parse FEN: '" + fen + "'");
        return;
    }

    // Set up NNUE if available
    if (nnue_ && nnue_->is_loaded())
    {
        board_.set_nnue(nnue_);
        nnue_->refresh(board_);
    }

    // Parse and validate move
    auto opt_move = Parser::move(move_str, board_);
    if (!opt_move || !is_valid_move(*opt_move, board_, true))
    {
        send_error("invalid_move", "Illegal move: '" + move_str + "'");
        return;
    }

    Move_t user_move = *opt_move;

    // Run comparison
    ComparisonReport report =
        MoveComparator::compare(board_, search_, user_move, 3, depth, movetime);

    // Override FEN with the input FEN
    report.fen = fen;

    // Serialize and send
    std::string data = CoachJson::serialize_comparison_report(report);
    send_response(wrap_envelope("comparison_report", data));
}

// ---------------------------------------------------------------------------
// Response framing
// ---------------------------------------------------------------------------

void CoachDispatcher::send_response(const std::string& json)
{
    std::cout << "BEGIN_COACH_RESPONSE"
              << "\n"
              << json << "\n"
              << "END_COACH_RESPONSE"
              << "\n";
}

void CoachDispatcher::send_error(const std::string& code, const std::string& message)
{
    std::string data_json = CoachJson::serialize_error(code, message);
    send_response(wrap_envelope("error", data_json));
}

std::string CoachDispatcher::wrap_envelope(const std::string& type,
                                           const std::string& data_json) const
{
    return CoachJson::object({ { "protocol", CoachJson::to_json(std::string("coaching")) },
                               { "version", CoachJson::to_json(std::string("1.0.0")) },
                               { "type", CoachJson::to_json(type) },
                               { "data", data_json } });
}
