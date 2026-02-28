/*
    Xboard.cpp
*/

#define _CRT_SECURE_NO_WARNINGS  // disables error C4996: 'sscanf': This function or variable may be
                                 // unsafe.
#include <cstring>

#include "Xboard.h"

#include "Hash.h"
#include "Output.h"
#include "Parser.h"
#include "ValidateMove.h"

Xboard::Xboard() {}

int Xboard::make_move(int stm, Move_t move)
{
    (void)stm;
    board_.do_move(move);
    return board_.side_to_move();
}

void Xboard::un_make(Move_t move)
{
    board_.undo_move(move);
    return;
}

int Xboard::setup(const char* fen)
{
    if (fen != NULL)
    {
        setup_fen_ = fen;
    }
    board_ = Parser::parse_fen(setup_fen_);
    reset_hash_table();
    return board_.side_to_move();
}

void Xboard::set_memory_size(int n)
{
    (void)n;
}

string Xboard::move_to_text(Move_t move)
{
    return Output::move(move, board_);
}

Move_t Xboard::parse_move(const char* move_text)
{
    Move_t move = Parser::move(move_text, board_);
    if (is_valid_move(move, board_, true))
    {
        return move;
    }
    else
    {
        return INVALID_MOVE;
    }
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

    // https://mediocrechess.blogspot.com/2007/01/guide-time-management.html
    // Use 2.25% of the time + half of the increment
    int time_for_this_move;  // time for this move in centi-seconds
    time_for_this_move = time_left / 40 + (int(inc * 100) / 2);

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

    Move_t best_move = board_.search(max_depth, time_for_this_move * 10000, -1, true);
    *move = best_move;

    return board_.get_search_best_score();
}

void Xboard::ponder_until_input(int stm)
{
    (void)stm;
}

int Xboard::take_back(int n)
{  // reset the game and then replay it to the desired point
    int last, stm;
    stm = setup(NULL);
    last = move_nr_ - n;
    if (last < 0)
        last = 0;
    for (move_nr_ = 0; move_nr_ < last; move_nr_++)
        stm = make_move(stm, game_move_[move_nr_]);
    return stm;
}

void Xboard::print_result(int stm, int score)
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
    int stm = 0;                 // side to move
    int engine_side = STM_NONE;  // side played by engine
    int time_left = 0;           // time left on engine's clock (in centi-seconds)
    int mps = 0;                 // time-control parameters, to be used by search
    int time_control = 0;
    int time_per_move = -1;
    float inc = 0;
    int max_depth = MAX_SEARCH_PLY;  // used by search
    Move_t move = INVALID_MOVE;
    Move_t ponder_move = INVALID_MOVE;
    char in_buf[80], command[80];

    while (1)
    {
        // infinite loop
        cout << "# FEN: " << Output::board_to_fen(board_) << endl;

        fflush(stdout);  // make sure everything is printed
                         // before we do something that might take time

        if (stm == engine_side)
        {  // if it is the engine's turn to move, set it thinking, and let it move

            cout << "# Thinking..." << endl;
            int score;
            score = search_best_move(stm,
                                     time_left,
                                     mps,
                                     time_control,
                                     inc,
                                     time_per_move,
                                     max_depth,
                                     &move,
                                     &ponder_move);

            if (!is_valid_move(move, board_, true))
            {  // game apparently ended
                cout << "# Invalid move" << endl;
                engine_side = STM_NONE;  // so stop playing
                print_result(stm, score);
            }
            else
            {
                stm = make_move(stm, move);  // assumes make_move returns new side to move
                assert(move_nr_ < MAXMOVES);
                game_move_[move_nr_++] = move;  // remember game
                cout << "move " << move_to_text(move) << endl;
            }
        }

        fflush(stdout);  // make sure everything is printed
                         // before we do something that might take time

        // now it is not our turn (anymore)
        if (engine_side == STM_ANALYZE)
        {  // in analysis, we always ponder the position
            ponder_until_input(stm);
        }
        else if (engine_side != STM_NONE && ponder_ == ON && move_nr_ != 0)
        {  // ponder while waiting for input
            if (ponder_move == INVALID_MOVE)
            {  // if we have no move to ponder on, ponder the position
                ponder_until_input(stm);
            }
            else
            {
                int new_stm = make_move(stm, ponder_move);
                ponder_until_input(new_stm);
                un_make(ponder_move);
            }
        }

    no_ponder:
        // wait for input, and read it until we have collected a complete line
        int i;
        for (i = 0; (in_buf[i] = static_cast<char>(getchar())) != '\n'; i++)
        {
            ;
        }
        in_buf[i] = '\0';

        // extract the first word
        sscanf(in_buf, "%s", command);
        cout << "# input command: " << command << endl;

        // recognize the command,and execute it
        if (!strcmp(command, "quit"))
        {
            break;
        }  // breaks out of infinite loop
        if (!strcmp(command, "force"))
        {
            engine_side = STM_NONE;
            continue;
        }
        if (!strcmp(command, "result"))
        {
            engine_side = STM_NONE;
            continue;
        }
        if (!strcmp(command, "analyze"))
        {
            engine_side = STM_ANALYZE;
            continue;
        }
        if (!strcmp(command, "exit"))
        {
            engine_side = STM_NONE;
            continue;
        }
        if (!strcmp(command, "otim"))
        {
            goto no_ponder;
        }  // do not start pondering after receiving time commands, as move will follow immediately
        if (!strcmp(command, "time"))
        {
            sscanf(in_buf, "time %d", &time_left);
            goto no_ponder;
        }
        if (!strcmp(command, "level"))
        {
            int min, sec = 0;
            sscanf(in_buf, "level %d %d %f", &mps, &min, &inc) == 3
                ||  // if this does not work, it must be min:sec format
                sscanf(in_buf, "level %d %d:%d %f", &mps, &min, &sec, &inc);
            time_control = 60 * min + sec;
            time_per_move = -1;
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
            continue;
        }
        if (!strcmp(command, "option"))
        {  // setting of engine-define option; find out which
            if (sscanf(in_buf + 7, "Resign=%d", &resign_) == 1)
                continue;
            if (sscanf(in_buf + 7, "Contempt=%d", &contempt_factor_) == 1)
                continue;
            continue;
        }
        if (!strcmp(command, "sd"))
        {
            sscanf(in_buf, "sd %d", &max_depth);
            continue;
        }
        if (!strcmp(command, "st"))
        {
            sscanf(in_buf, "st %d", &time_per_move);
            continue;
        }
        if (!strcmp(command, "memory"))
        {
            set_memory_size(atoi(in_buf + 7));
            continue;
        }
        if (!strcmp(command, "ping"))
        {
            cout << "pong" << in_buf + 4 << endl;
            continue;
        }
        if (!strcmp(command, "new"))
        {
            engine_side = BLACK;
            stm = setup(DEFAULT_FEN);
            max_depth = MAX_SEARCH_PLY;
            randomize_ = OFF;
            move_nr_ = 0;
            continue;
        }
        if (!strcmp(command, "setboard"))
        {
            engine_side = STM_NONE;
            stm = setup(in_buf + 9);
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
            stm = take_back(1);
            continue;
        }
        if (!strcmp(command, "remove"))
        {
            stm = take_back(2);
            continue;
        }
        if (!strcmp(command, "go"))
        {
            engine_side = stm;
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
            if (ponder_move != INVALID_MOVE)
            {
                cout << "Hint: " << move_to_text(ponder_move) << endl;
            }
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
        if (!strcmp(command, "?"))
        {
            continue;
        }
        if (!strcmp(command, ""))
        {
            continue;
        }
        if (!strcmp(command, "usermove"))
        {
            move = parse_move(in_buf + 9);
            cout << "# parsed usermove=" << Output::move_san(move, board_) << endl;
            if (move == INVALID_MOVE)
            {
                cout << "Illegal move" << endl;
            }
            else
            {
                stm = make_move(stm, move);
                ponder_move = INVALID_MOVE;
                assert(move_nr_ < MAXMOVES);
                game_move_[move_nr_++] = move;  // remember game
            }
            continue;
        }
        cout << "Error: (unknown command): " << command << endl;
    }

    return;
}