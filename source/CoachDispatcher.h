/*
 * File:   CoachDispatcher.h
 *
 * Plugs into the UCI command loop to recognize `coach`-prefixed commands
 * and route them to the appropriate handler. Owns response framing
 * (BEGIN/END markers + JSON envelope).
 */

#ifndef COACHDISPATCHER_H
#define COACHDISPATCHER_H

#include <string>

class Board;
class Search;
class NNUEEvaluator;

class CoachDispatcher {
public:
    CoachDispatcher(Board& board, Search& search, NNUEEvaluator* nnue);

    /// Main entry point: parse subcommand and route.
    void dispatch(const std::string& args);

private:
    void cmd_ping();
    void cmd_eval(const std::string& args);
    void cmd_compare(const std::string& args);

    /// Write BEGIN_COACH_RESPONSE / json / END_COACH_RESPONSE to stdout.
    void send_response(const std::string& json);

    /// Build an error envelope and send it.
    void send_error(const std::string& code, const std::string& message);

    /// Build the JSON envelope: {"protocol":"coaching","version":"1.0.0","type":...,"data":...}
    std::string wrap_envelope(const std::string& type, const std::string& data_json) const;

    Board& board_;
    Search& search_;
    NNUEEvaluator* nnue_;
};

#endif /* COACHDISPATCHER_H */
