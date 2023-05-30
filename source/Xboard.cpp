/*
    Xboard.cpp
*/

#include <cstring>

#include "Xboard.h"

#include "Parser.h"

Xboard::Xboard() {}

int Xboard::MakeMove(int stm, Move_t move)
{
    (void)stm;
    board.do_move(move);
    return board.side_to_move();
}

void Xboard::UnMake(Move_t move)
{
    board.undo_move(move);
    return;
}

int Xboard::Setup(char* fen)
{
    board = Parser::parse_fen(fen);
}

void Xboard::SetMemorySize(int n) {}

char* Xboard::MoveToText(Move_t move) {}

Move_t Xboard::ParseMove(char* moveText)
{
    return Parser::move(moveText, board);
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
}

void Xboard::PonderUntilInput(int stm) {}

int Xboard::TakeBack(int n)
{  // reset the game and then replay it to the desired point
    int last, stm;
    stm = Setup(NULL);
    last = moveNr - n;
    if (last < 0)
        last = 0;
    for (moveNr = 0; moveNr < last; moveNr++)
        stm = MakeMove(stm, gameMove[moveNr]);
    return stm;
}

void Xboard::PrintResult(int stm, int score)
{
    if (score == 0)
        printf("1/2-1/2\n");
    if (score > 0 && stm == WHITE || score < 0 && stm == BLACK)
        printf("1-0\n");
    else
        printf("0-1\n");
}

void Xboard::run()
{
    int stm;                                 // side to move
    int engineSide = STM_NONE;               // side played by engine
    int timeLeft;                            // timeleft on engine's clock
    int mps, timeControl, inc, timePerMove;  // time-control parameters, to be used by Search
    int maxDepth;                            // used by search
    Move_t move, ponderMove;
    int i, score;
    char inBuf[80], command[80];

    while (1)
    {  // infinite loop

        fflush(stdout);  // make sure everything is printed
                         // before we do something that might take time

        if (stm == engineSide)
        {  // if it is the engine's turn to move, set it thinking, and let it move

            score = SearchBestMove(
                stm, timeLeft, mps, timeControl, inc, timePerMove, &move, &ponderMove);

            if (move == INVALID)
            {                           // game apparently ended
                engineSide = STM_NONE;  // so stop playing
                PrintResult(stm, score);
            }
            else
            {
                stm = MakeMove(stm, move);  // assumes MakeMove returns new side to move
                gameMove[moveNr++] = move;  // remember game
                printf("move %s\n", MoveToText(move));
            }
        }

        fflush(
            stdout);  // make sure everything is printed before we do something that might take time

        // now it is not our turn (anymore)
        if (engineSide == STM_ANALYZE)
        {  // in analysis, we always ponder the position
            PonderUntilInput(stm);
        }
        else if (engineSide != STM_NONE && ponder == ON && moveNr != 0)
        {  // ponder while waiting for input
            if (ponderMove == INVALID)
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
        for (i = 0; (inBuf[i] = getchar()) != '\n'; i++)
            ;
        inBuf[i + 1] = 0;

        // extract the first word
        sscanf(inBuf, "%s", command);

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
        if (!strcmp(command, "STM_ANALYZE"))
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
            printf("feature ping=1 setboard=1 colors=0 usermove=1 memory=1 debug=1");
            printf("feature option=\"Resign -check 0\"");  // example of an engine-defined option
            printf("feature option=\"Contempt -spin 0 -200 200\"");  // and another one
            printf("feature done=1");
            continue;
        }
        if (!strcmp(command, "option"))
        {  // setting of engine-define option; find out which
            if (sscanf(inBuf + 7, "Resign=%d", &resign) == 1)
                continue;
            if (sscanf(inBuf + 7, "Contempt=%d", &contemptFactor) == 1)
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
            printf("pong%s", inBuf + 4);
            continue;
        }
        //  if(!strcmp(command, ""))        { sscanf(inBuf, " %d", &); continue; }
        if (!strcmp(command, "new"))
        {
            engineSide = BLACK;
            stm = Setup(DEFAULT_FEN);
            maxDepth = MAXPLY;
            randomize = OFF;
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
            ponder = OFF;
            continue;
        }
        if (!strcmp(command, "hard"))
        {
            ponder = ON;
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
            postThinking = ON;
            continue;
        }
        if (!strcmp(command, "nopost"))
        {
            postThinking = OFF;
            continue;
        }
        if (!strcmp(command, "random"))
        {
            randomize = ON;
            continue;
        }
        if (!strcmp(command, "hint"))
        {
            if (ponderMove != INVALID)
                printf("Hint: %s\n", MoveToText(ponderMove));
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
            int move = ParseMove(inBuf + 9);
            if (move == INVALID)
                printf("Illegal move\n");
            else
            {
                stm = MakeMove(stm, move);
                ponderMove = INVALID;
                gameMove[moveNr++] = move;  // remember game
            }
            continue;
        }
        printf("Error: unknown command\n");
    }

    return;
}