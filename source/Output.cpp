/*
 * File:   Output.cpp
 *
 */

#include "Output.h"

#include "MoveList.h"

#define MAX_ROW 7
#define MAX_COL 7

bool Output::colors_enabled { false };

string Output::board(const class Board board)
{
    stringstream ss;
    ss << "  ABCDEFGH " << endl;
    for (int row = MAX_ROW; row >= 0; row--)
    {
        ss << row + 1 << "|";
        for (int col = 0; col <= MAX_COL; col++)
        {
            int square = (row * 8) + col;
            ss << Output::piece(board[square]);
        }
        ss << '|' << row + 1 << endl;
    }
    ss << " ABCDEFGH  " << endl;
    ss << ((board.side_to_move() == WHITE) ? "WHITE" : "BLACK") << " turn" << endl;
    ss << "Ply: " << board.get_game_ply() << endl;
    return ss.str();
}

string Output::bitboard(U64 bb)
{
    stringstream ss;
    ss << "  ABCDEFGH  " << endl;
    for (int row = MAX_ROW; row >= 0; row--)
    {
        ss << row + 1 << "|";
        for (int col = 0; col <= MAX_COL; col++)
        {
            int square = (row * 8) + col;
            ss << (bb & (1ULL << square) ? 'X' : '-');
        }
        ss << '|' << row + 1 << endl;
    }
    ss << "  ABCDEFGH  " << endl;
    return ss.str();
}

string Output::piece(U8 piece)
{
    stringstream ss;
    if (Output::is_colors_enabled())
    {
        if (piece & BLACK)
        {
            ss << "\033[91m";  // Red
        }
        else if (piece)
        {
            ss << "\033[97m";  // White
        }
        ss << PIECE_CHARS[piece];
        ss << "\033[0m";
    }
    else
    {
        ss << PIECE_CHARS[piece];
    }

    return ss.str();
}

string Output::square(U8 square)
{
    const string ROWS = "12345678";
    const string FILES = "abcdefgh";
    stringstream ss;
    ss << FILES[square & 7] << ROWS[static_cast<size_t>(square >> 3)];
    return ss.str();
}

string Output::move(Move_t move, const class Board& board)
{
    (void)board;
    stringstream ss;
    if(is_castle(move))
    {
        if(move == KING_CASTLE) ss << "0-0";
        else ss << "0-0-0";
        return ss.str();
    }
    U8 from = move_from(move);
    ss << Output::piece(board[from]&(~1));
    ss << Output::square(from);
    if(is_capture(move))
    {
        ss << 'x' << Output::piece(move_captured(move)&(~1));
    }
    ss << Output::square(move_to(move));
    if(is_ep_capture(move))
    {
        ss << "ep";
    }
    if(is_promotion(move))
    {
        ss << "(" << Output::piece(move_promote_to(move)&(~1)) << ")";
    }
    return ss.str();
}

string Output::movelist(const class MoveList& list,
                        const class Board& board)
{
    stringstream ss;
    if(list.length() == 0)
    {
        ss << "NO MOVES";
        return ss.str();
    }
    for(int i = 0; i < list.length(); i++)
    {
        ss << Output::move(list[i], board) << ", ";
        if (i % 8 == 7 || i == list.length()-1)
        {
            ss << endl;
        }
    }
    ss << endl;
    return ss.str();
}
