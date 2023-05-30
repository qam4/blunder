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
    ss << "  ABCDEFGH  " << endl;
    ss << "side to move: " << ((board.side_to_move() == WHITE) ? "WHITE" : "BLACK") << endl;
    ss << "castling rights: " << Output::castling_rights(board.castling_rights()) << endl;
    ss << "en-passant: "
       << ((board.ep_square() == NULL_SQUARE) ? "-" : Output::square(board.ep_square())) << endl;
    ss << "halfmove clock: " << int(board.half_move_count()) << endl;
    ss << "fullmove clock: " << int(board.full_move_count()) << endl;
    ss << "ply: " << board.get_game_ply() << endl;
    ss << "FEN: " << Output::board_to_fen(board) << endl;
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
    if (is_castle(move))
    {
        if (move == build_castle(KING_CASTLE))
            ss << "O-O";
        else
            ss << "O-O-O";
        return ss.str();
    }
    U8 from = move_from(move);
    ss << Output::square(from);
    ss << Output::square(move_to(move));
    if (is_promotion(move))
    {
        ss << Output::piece(move_promote_to(move) | (0x1U));
    }
    return ss.str();
}

string Output::movelist(const class MoveList& list, const class Board& board)
{
    stringstream ss;
    if (list.length() == 0)
    {
        ss << "NO MOVES";
        return ss.str();
    }
    for (int i = 0; i < list.length(); i++)
    {
        ss << Output::move(list[i], board) << ", ";
        if (i % 8 == 7 || i == list.length() - 1)
        {
            ss << endl;
        }
    }
    ss << endl;
    return ss.str();
}

string Output::castling_rights(U8 castling_rights)
{
    stringstream ss;
    if (castling_rights & WHITE_KING_SIDE)
    {
        ss << 'K';
    }
    if (castling_rights & WHITE_QUEEN_SIDE)
    {
        ss << 'Q';
    }
    if (castling_rights & BLACK_KING_SIDE)
    {
        ss << 'k';
    }
    if (castling_rights & BLACK_QUEEN_SIDE)
    {
        ss << 'q';
    }

    return ss.str();
}

string Output::board_to_fen(const class Board board)
{
    stringstream fen;

    for (int row = 0; row < 8; row++)
    {
        int empties = 0;
        for (int col = 0; col < 8; col++)
        {
            U8 piece = board[(7 - row) * 8 + col];
            if (piece == EMPTY)
            {
                empties += 1;
            }
            else
            {
                if (empties > 0)
                {
                    fen << empties;
                    empties = 0;
                }
                fen << PIECE_CHARS[piece];
            }
        }
        if (empties > 0)
        {
            fen << empties;
        }
        if (row != 7)
        {
            fen << '/';
        }
    }

    fen << ' ';
    fen << ((board.side_to_move() == BLACK) ? 'b' : 'w');
    fen << ' ';
    U8 castling_rights = board.castling_rights();
    if (castling_rights == 0)
    {
        fen << '-';
    }
    if (castling_rights & WHITE_KING_SIDE)
    {
        fen << 'K';
    }
    if (castling_rights & WHITE_QUEEN_SIDE)
    {
        fen << 'Q';
    }
    if (castling_rights & BLACK_KING_SIDE)
    {
        fen << 'k';
    }
    if (castling_rights & BLACK_QUEEN_SIDE)
    {
        fen << 'q';
    }
    fen << ' ';
    U8 ep_square = board.ep_square();
    if (ep_square == NULL_SQUARE)
    {
        fen << '-';
    }
    else
    {
        fen << Output::square(ep_square);
    }
    fen << ' ';
    fen << int(board.half_move_count());
    fen << ' ';
    fen << int(board.full_move_count());

    return fen.str();
}
