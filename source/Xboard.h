/*
    Xboard.h
*/

#ifndef XBOARD_H
#define XBOARD_H

#include "Board.h"
#include "Common.h"
#include "Move.h"

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
    int  MakeMove(int stm, Move_t move);      // performs move, and returns new side to move
    void UnMake(Move_t move);                 // unmakes the move;
    int  Setup(const char *fen);                  // sets up the position from the given FEN, and returns the new side to move
    void SetMemorySize(int n);              // if n is different from last time, resize all tables to make memory usage below n MB
    string MoveToText(Move_t move);            // converts the move from your internal format to text like e2e2, e1g1, a7a8q.
    Move_t ParseMove(const char *moveText);         // converts a long-algebraic text move to your internal move format
    int  SearchBestMove(int stm, int timeLeft, int mps, int timeControl, int inc, int timePerMove, Move_t *move, Move_t *ponderMove);
    void PonderUntilInput(int stm);         // Search current position for stm, deepening forever until there is input.
    int TakeBack(int n);
    void PrintResult(int stm, int score);

    Board board;

    int moveNr;              // part of game state; incremented by MakeMove
    Move_t gameMove[MAXMOVES]; // holds the game history

    // Some global variables that control your engine's behavior
    int ponder;
    int randomize;
    int postThinking;
    int resign;         // engine-defined option
    int contemptFactor; // likewise
    string setup_fen;
};

#endif  // XBOARD_H
