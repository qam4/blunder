/*
    Xboard.h
*/

#ifndef XBOARD_H
#define XBOARD_H

#include <functional>
#include <string>
#include <unordered_map>

#include "Board.h"
#include "Common.h"
#include "Move.h"
#include "Search.h"

// Side-to-move constants for the Xboard protocol loop
constexpr int STM_WHITE = 0;
constexpr int STM_BLACK = 1;
constexpr int STM_NONE = 2;
constexpr int STM_ANALYZE = 3;

constexpr Move_t INVALID_MOVE = 0x9999;

constexpr int MAXMOVES = 500;

constexpr int OFF = 0;
constexpr int ON = 1;

class Xboard
{
  public:
    Xboard();
    void run();

  private:
    // Mutable state shared across command handlers during the main loop
    struct RunState
    {
        int stm = 0;
        int engine_side = STM_NONE;
        int time_left = 0;
        int mps = 0;
        int time_control = 0;
        int time_per_move = -1;
        float inc = 0;
        int max_depth = MAX_SEARCH_PLY;
        Move_t move = INVALID_MOVE;
        Move_t ponder_move = INVALID_MOVE;
        bool quit = false;
        bool skip_ponder = false;
    };

    using Handler = std::function<void(const std::string& args, RunState& rs)>;

    void init_handlers();

    // Helper methods
    int make_move(int stm, Move_t move);
    void un_make(Move_t move);
    int setup(const std::string& fen);
    void set_memory_size(int n);
    std::string move_to_text(Move_t move);
    Move_t parse_move(const std::string& move_text);
    int search_best_move(int stm,
                         int time_left,
                         int mps,
                         int time_control,
                         float inc,
                         int time_per_move,
                         int max_depth,
                         Move_t* move,
                         Move_t* ponder_move);
    void ponder_until_input(int stm);
    int take_back(int n);
    void print_result(int stm, int score);

    Board board_;
    Search search_;

    int move_nr_ = 0;
    Move_t game_move_[MAXMOVES] {};

    int ponder_ = OFF;
    int randomize_ = OFF;
    int post_thinking_ = OFF;
    int resign_ = 0;
    int contempt_factor_ = 0;
    std::string setup_fen_;

    std::unordered_map<std::string, Handler> handlers_;
};

#endif  // XBOARD_H
