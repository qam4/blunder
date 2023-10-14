/*
    Xboard.cpp
*/

#define _CRT_SECURE_NO_WARNINGS  // disables error C4996: 'sscanf': This function or variable may be
                                 // unsafe.
#include <cstring>

#include "Xboard.h"

#include "Output.h"
#include "Parser.h"

Xboard::Xboard() {}

int Xboard::MakeMove(int stm, Move_t move)
{
    (void)stm;
    board_.do_move(move);
    return board_.side_to_move();
}

void Xboard::UnMake(Move_t move)
{
    board_.undo_move(move);
    return;
}

int Xboard::Setup(const char* fen)
{
    if (fen != NULL)
    {
        setup_fen_ = fen;
    }
    board_ = Parser::parse_fen(setup_fen_);
    return board_.side_to_move();
}

void Xboard::SetMemorySize(int n)
{
    (void)n;
}

string Xboard::MoveToText(Move_t move)
{
    return Output::move(move, board_);
}

Move_t Xboard::ParseMove(const char* moveText)
{
    return Parser::move(moveText, board_);
}

int Xboard::SearchBestMove(int stm,
                           int timeLeft,
                           int mps,
                           int timeControl,
                           int inc,
                           int timePerMove,
                           Move_t* move,
                           Move_t* ponderMove)
{
    (void)stm;
    (void)timeLeft;
    (void)mps;
    (void)timeControl;
    (void)inc;
    (void)timePerMove;
    (void)ponderMove;

    Move_t best_move = board_.search(MAX_SEARCH_PLY, DEFAULT_SEARCH_TIME, -1, true);
    *move = best_move;

    return board_.get_search_best_score();
}

void Xboard::PonderUntilInput(int stm)
{
    (void)stm;
}

int Xboard::TakeBack(int n)
{  // reset the game and then replay it to the desired point
    int last, stm;
    stm = Setup(NULL);
    last = move_nr_ - n;
    if (last < 0)
        last = 0;
    for (move_nr_ = 0; move_nr_ < last; move_nr_++)
        stm = MakeMove(stm, game_move_[move_nr_]);
    return stm;
}

void Xboard::PrintResult(int stm, int score)
{
    if (score == 0)
    {
        cout << "1/2-1/2" << endl;
    }
    if (((score > 0) && (stm == WHITE)) || ((score < 0) && (stm == BLACK)))
    {
        cout << "1-0" << endl;
    }
    else
    {
        cout << "0-1" << endl;
    }
}

void Xboard::run()
{
    int stm = 0;                             // side to move
    int engineSide = STM_NONE;               // side played by engine
    int timeLeft = 0;                        // timeleft on engine's clock
    int mps, timeControl, inc, timePerMove;  // time-control parameters, to be used by Search
    int maxDepth;                            // used by search
    Move_t move, ponderMove;
    int i, score;
    char inBuf[80], command[80];

    while (1)
    {
        // infinite loop
        cout << "# FEN: " << Output::board_to_fen(board_) << endl;

        fflush(stdout);  // make sure everything is printed
                         // before we do something that might take time

        if (stm == engineSide)
        {  // if it is the engine's turn to move, set it thinking, and let it move

            cout << "# Thinking..." << endl;
            score = SearchBestMove(
                stm, timeLeft, mps, timeControl, inc, timePerMove, &move, &ponderMove);

            if (move == INVALID_MOVE)
            {                           // game apparently ended
                engineSide = STM_NONE;  // so stop playing
                PrintResult(stm, score);
            }
            else
            {
                stm = MakeMove(stm, move);      // assumes MakeMove returns new side to move
                game_move_[move_nr_++] = move;  // remember game
                cout << "move " << MoveToText(move) << endl;
            }
        }

        fflush(stdout);  // make sure everything is printed
                         // before we do something that might take time

        // now it is not our turn (anymore)
        if (engineSide == STM_ANALYZE)
        {  // in analysis, we always ponder the position
            PonderUntilInput(stm);
        }
        else if (engineSide != STM_NONE && ponder_ == ON && move_nr_ != 0)
        {  // ponder while waiting for input
            if (ponderMove == INVALID_MOVE)
            {  // if we have no move to ponder on, ponder the position
                PonderUntilInput(stm);
            }
            else
            {
                int newStm = MakeMove(stm, ponderMove);
                PonderUntilInput(newStm);
                UnMake(ponderMove);
            }
        }

    noPonder:
        // wait for input, and read it until we have collected a complete line
        for (i = 0; (inBuf[i] = static_cast<char>(getchar())) != '\n'; i++)
            ;
        inBuf[i + 1] = 0;

        // extract the first word
        sscanf(inBuf, "%s", command);
        cout << "# input command: " << command << endl;

        // recognize the command,and execute it
        if (!strcmp(command, "quit"))
        {
            break;
        }  // breaks out of infinite loop
        if (!strcmp(command, "force"))
        {
            engineSide = STM_NONE;
            continue;
        }
        if (!strcmp(command, "result"))
        {
            engineSide = STM_NONE;
            continue;
        }
        if (!strcmp(command, "analyze"))
        {
            engineSide = STM_ANALYZE;
            continue;
        }
        if (!strcmp(command, "exit"))
        {
            engineSide = STM_NONE;
            continue;
        }
        if (!strcmp(command, "otim"))
        {
            goto noPonder;
        }  // do not start pondering after receiving time commands, as move will follow immediately
        if (!strcmp(command, "time"))
        {
            sscanf(inBuf, "time %d", &timeLeft);
            goto noPonder;
        }
        if (!strcmp(command, "level"))
        {
            int min, sec = 0;
            sscanf(inBuf, "level %d %d %d", &mps, &min, &inc) == 3
                ||  // if this does not work, it must be min:sec format
                sscanf(inBuf, "level %d %d:%d %d", &mps, &min, &sec, &inc);
            timeControl = 60 * min + sec;
            timePerMove = -1;
            continue;
        }
        if (!strcmp(command, "protover"))
        {
            cout << "feature done=0 myname=\"blunder\" ping=1 memory=1 setboard=1 debug=1 sigint=0 "
                    "sigterm=0"
                 << endl;                            // always support these!
            cout << "feature name=1 ics=1" << endl;  // likely you are not interested in receiving
                                                     // these, and leave out this line
            cout << "feature usermove=1" << endl;  // when you are not confident you can distinguish
                                                   // moves from other commands
            // cout << "feature egt=\"syzygy,scorpio\"" << endl;           // when you support
            // end-game tables of the mentioned flavor(s) cout << "feature
            // variants=\"normal,suicide,foo\"" << endl;  // when you support variants other than
            // orthodox chess cout << "feature nps=1" << endl;                          // when you
            // support node-count-based time controls cout << "feature smp=1" << endl; // when you
            // support multi-threaded parallel search cout << "feature exclude=1" << endl; // when
            // you support move exclusion in analysis cout << "feature option=\"MultiPV -spin 1 1
            // 100\"" << endl; // 3 examples of engine-defined options, first is sort of standard
            // cout << "feature option=\"Resign -check 0\"" << endl;
            // cout << "feature option=\"Clear Hash -button\"" << endl;
            cout << "feature done=1" << endl;  // never forget this one!
            cout << "# debug message" << endl;
            continue;
        }
        if (!strcmp(command, "option"))
        {  // setting of engine-define option; find out which
            if (sscanf(inBuf + 7, "Resign=%d", &resign_) == 1)
                continue;
            if (sscanf(inBuf + 7, "Contempt=%d", &contempt_factor_) == 1)
                continue;
            continue;
        }
        if (!strcmp(command, "sd"))
        {
            sscanf(inBuf, "sd %d", &maxDepth);
            continue;
        }
        if (!strcmp(command, "st"))
        {
            sscanf(inBuf, "st %d", &timePerMove);
            continue;
        }
        if (!strcmp(command, "memory"))
        {
            SetMemorySize(atoi(inBuf + 7));
            continue;
        }
        if (!strcmp(command, "ping"))
        {
            cout << "pong" << inBuf + 4 << endl;
            continue;
        }
        //  if(!strcmp(command, ""))        { sscanf(inBuf, " %d", &); continue; }
        if (!strcmp(command, "new"))
        {
            engineSide = BLACK;
            stm = Setup(DEFAULT_FEN);
            maxDepth = MAX_SEARCH_PLY;
            randomize_ = OFF;
            continue;
        }
        if (!strcmp(command, "setboard"))
        {
            engineSide = STM_NONE;
            stm = Setup(inBuf + 9);
            continue;
        }
        if (!strcmp(command, "easy"))
        {
            ponder_ = OFF;
            continue;
        }
        if (!strcmp(command, "hard"))
        {
            ponder_ = ON;
            continue;
        }
        if (!strcmp(command, "undo"))
        {
            stm = TakeBack(1);
            continue;
        }
        if (!strcmp(command, "remove"))
        {
            stm = TakeBack(2);
            continue;
        }
        if (!strcmp(command, "go"))
        {
            engineSide = stm;
            continue;
        }
        if (!strcmp(command, "post"))
        {
            post_thinking_ = ON;
            continue;
        }
        if (!strcmp(command, "nopost"))
        {
            post_thinking_ = OFF;
            continue;
        }
        if (!strcmp(command, "random"))
        {
            randomize_ = ON;
            continue;
        }
        if (!strcmp(command, "hint"))
        {
            if (ponderMove != INVALID_MOVE)
                cout << "Hint: " << MoveToText(ponderMove) << endl;
            continue;
        }
        if (!strcmp(command, "book"))
        {
            continue;
        }
        // ignored commands:
        if (!strcmp(command, "xboard"))
        {
            continue;
        }
        if (!strcmp(command, "computer"))
        {
            continue;
        }
        if (!strcmp(command, "name"))
        {
            continue;
        }
        if (!strcmp(command, "ics"))
        {
            continue;
        }
        if (!strcmp(command, "accepted"))
        {
            continue;
        }
        if (!strcmp(command, "rejected"))
        {
            continue;
        }
        if (!strcmp(command, "variant"))
        {
            continue;
        }
        if (!strcmp(command, ""))
        {
            continue;
        }
        if (!strcmp(command, "usermove"))
        {
            move = ParseMove(inBuf + 9);
            if (move == INVALID_MOVE)
                cout << "Illegal move" << endl;
            else
            {
                stm = MakeMove(stm, move);
                ponderMove = INVALID_MOVE;
                game_move_[move_nr_++] = move;  // remember game
            }
            continue;
        }
        cout << "Error: unknown command" << endl;
    }

    return;
}