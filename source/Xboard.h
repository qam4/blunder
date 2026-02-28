/*
    Xboard.h
*/

#ifndef XBOARD_H
#define XBOARD_H

#include "Board.h"
#include "Common.h"
#include "Move.h"
#include "Search.h"

// four different constants, with values for WHITE and BLACK that suit your engine
#define STM_WHITE    0
#define STM_BLACK    1
#define STM_NONE     2
#define STM_ANALYZE  3

// some value that cannot occur as a valid move
#define INVALID_MOVE 0x9999

// some parameter of your engine
#define MAXMOVES 500  /* maximum game length  */

#define OFF 0
#define ON  1

class Xboard
{
  public:
    Xboard();
    void run();

  private:
    // Some routines your engine should have to do the various essential things
    int  make_move(int stm, Move_t move);      // performs move, and returns new side to move
    void un_make(Move_t move);                 // unmakes the move;
    int  setup(const char *fen);               // sets up the position from the given FEN, and returns the new side to move
    void set_memory_size(int n);               // if n is different from last time, resize all tables to make memory usage below n MB
    std::string move_to_text(Move_t move);          // converts the move from your internal format to text like e2e2, e1g1, a7a8q.
    Move_t parse_move(const char *move_text);  // converts a long-algebraic text move to your internal move format
    int  search_best_move(int stm, int time_left, int mps, int time_control, float inc, int time_per_move, int max_depth, Move_t *move, Move_t *ponder_move);
    void ponder_until_input(int stm);          // Search current position for stm, deepening forever until there is input.
    int take_back(int n);
    void print_result(int stm, int score);

    Board board_;
    Search search_;

    int move_nr_;                // part of game state; incremented by make_move
    Move_t game_move_[MAXMOVES]; // holds the game history

    // Some global variables that control your engine's behavior
    int ponder_;
    int randomize_;
    int post_thinking_;
    int resign_;          // engine-defined option
    int contempt_factor_; // likewise
    std::string setup_fen_;
};

#endif  // XBOARD_H
