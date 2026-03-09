/*
 * File:   UCI.h
 *
 * UCI (Universal Chess Interface) protocol handler.
 * Implements the standard UCI protocol for GUI communication.
 */

#ifndef UCI_H
#define UCI_H

#include <atomic>
#include <functional>
#include <string>
#include <thread>
#include <unordered_map>

#include "Board.h"
#include "Book.h"
#include "CoachDispatcher.h"
#include "DualHeadNetwork.h"
#include "MCTS.h"
#include "NNUEEvaluator.h"
#include "Search.h"

class UCI
{
public:
    UCI();
    ~UCI();

    void run();

    /// Transfer an already-opened Book into the UCI instance.
    void set_book(Book&& book)
    {
        book_ = std::move(book);
        book_enabled_ = book_.is_open();
    }

    /// Enable MCTS search mode.
    void set_mcts_mode(int simulations, double c_puct)
    {
        use_mcts_ = true;
        mcts_simulations_ = simulations;
        mcts_c_puct_ = c_puct;
    }

    /// Enable AlphaZero mode: MCTS with dual-head neural network.
    void set_alphazero_mode(DualHeadNetwork* network, int simulations, double c_puct)
    {
        use_mcts_ = true;
        dual_head_ = network;
        mcts_simulations_ = simulations;
        mcts_c_puct_ = c_puct;
    }

    /// Set the NNUE evaluator.
    void set_nnue(NNUEEvaluator* nnue)
    {
        nnue_ = nnue;
        board_.set_nnue(nnue);
        if (nnue && nnue->is_loaded())
        {
            nnue->refresh(board_);
        }
    }

private:
    using Handler = std::function<void(const std::string& args)>;

    void init_handlers();

    // UCI command handlers
    void cmd_uci();
    void cmd_isready();
    void cmd_ucinewgame();
    void cmd_position(const std::string& args);
    void cmd_go(const std::string& args);
    void cmd_stop();
    void cmd_setoption(const std::string& args);

    // Search helpers
    void start_search(int depth, int wtime, int btime, int winc, int binc,
                      int movestogo, int movetime, int nodes, bool infinite);
    void send_bestmove(Move_t move, Move_t ponder_move);
    void send_info(int depth, int score_cp, int nodes, int nps,
                   int time_ms, const std::string& pv);

    // Move helpers
    std::string move_to_uci(Move_t move) const;
    Move_t parse_uci_move(const std::string& move_str);

    Board board_;
    Search search_;
    Book book_;
    bool book_enabled_ = false;
    NNUEEvaluator* nnue_ = nullptr;
    CoachDispatcher coach_dispatcher_;

    // MCTS configuration
    bool use_mcts_ = false;
    DualHeadNetwork* dual_head_ = nullptr;
    int mcts_simulations_ = 800;
    double mcts_c_puct_ = 1.41;

    // Search thread
    std::thread search_thread_;
    std::atomic<bool> searching_{false};

    // Game state
    int move_nr_ = 0;

    // Options
    int hash_size_mb_ = 16;
    int multipv_count_ = 1;

    std::unordered_map<std::string, Handler> handlers_;
};

#endif  // UCI_H
