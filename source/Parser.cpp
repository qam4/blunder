/*
 * File:   Parser.cpp
 *
 */

#include "Parser.h"

static vector<string> split(const string& s, const char delimiter)
{
    vector<string> tokens;
    string token;
    istringstream tokenStream(s);
    while (getline(tokenStream, token, delimiter))
    {
        tokens.push_back(token);
    }
    return tokens;
}

class Board Parser::parse_fen(string fen)
{
    Board board = Board();

    vector<string> tokens = split(fen, ' ');
    size_t num_tokens = tokens.size();
    size_t len;
    size_t pos;  // position in string
    string token;

    if (num_tokens > 0)
    {
        token = tokens[0];
        pos = 0;
        // 8 rows of pieces
        for (int row = 7; row >= 0; row--)
        {
            while (token[pos] == '/')
                pos++;
            for (int col = 0; col < 8; col++)
            {
                char c = token[pos++];
                // cout << "[" << row<< "," << col << "]=" << c << endl;
                //  if number skip ahead that many columns
                if (c >= '1' && c <= '8')
                {
                    col += c - '1';
                }
                else
                {  // find piece
                    U8 piece = parse_piece(c);
                    if (piece)
                        board.add_piece(piece, (row * 8) + col);
                }
            }
        }
    }

    if (num_tokens > 1)
    {
        // side to move
        token = tokens[1];
        pos = 0;
        U8 side_to_move = Parser::side(token[pos++]);
        board.set_side_to_move(side_to_move);
    }

    if (num_tokens > 2)
    {
        // castling rights
        token = tokens[2];
        pos = 0;
        len = token.length();
        U8 rights = 0;
        if (token[pos] == '-')
        {
            // do nothing
        }
        else
        {
            while (pos < len)
            {
                rights = rights | Parser::castling_right(token[pos++]);
            }
        }
        board.set_castling_rights(rights);
    }

    if (num_tokens > 3)
    {
        // ep square
        token = tokens[3];
        pos = 0;
        if (token[pos] == '-')
        {
            board.set_ep_square(NULL_SQUARE);
        }
        else
        {
            char square[2] = { token[pos], token[pos + 1] };
            board.set_ep_square(Parser::square(square));
        }
    }

    if (num_tokens > 4)
    {
        // half-move-count
        token = tokens[4];
        pos = 0;
        len = token.length();
        int half_move_count = 0;
        while (token[pos] >= '0' && token[pos] <= '9' && pos < len)
        {
            half_move_count = half_move_count * 10 + (token[pos] - '0');
            pos++;
        }
        board.set_half_move_count(static_cast<U8>(half_move_count));
    }

    if (num_tokens > 5)
    {
        // full-move-count
        token = tokens[5];
        pos = 0;
        len = token.length();
        int full_move_count = 0;
        while (token[pos] >= '0' && token[pos] <= '9' && pos < len)
        {
            full_move_count = full_move_count * 10 + (token[pos] - '0');
            pos++;
        }
        board.set_full_move_count(static_cast<U8>(full_move_count));
    }

    return board;
}

U8 Parser::parse_piece(char piece)
{
    for (U8 i = 2; i < 14; i++)
    {
        if (PIECE_CHARS[i] == piece)
            return i;
    }
    return EMPTY;
}

U8 Parser::side(char c)
{
    if (c == 'b' || c == 'B')
        return BLACK;
    return WHITE;
}

U8 Parser::castling_right(char c)
{
    switch (c)
    {
        case 'K':
            return WHITE_KING_SIDE;
        case 'Q':
            return WHITE_QUEEN_SIDE;
        case 'k':
            return BLACK_KING_SIDE;
        case 'q':
            return BLACK_QUEEN_SIDE;
    }
    return 0U;
}

U8 Parser::square(char sq[])
{
    int col = sq[0] - 'a';
    int row = sq[1] - '1';
    return static_cast<U8>((row * 8) + col);
}

// [fm] TODO
U32 Parser::move(string str, Board& board)
{
    size_t len = str.length();
    U8 from, to, piece, capture;
    U32 move = 0;

    // note: is it ok to return 0 for errors?
    if (len != 4)
        return 0;

    from = square(&str[0]);
    to = square(&str[2]);
    piece = board[from];
    capture = board[to];
    if ((from == NULL_SQUARE) || (to == NULL_SQUARE))
        return 0;
    // TODO
    U8 flags = NO_FLAGS;
    (void)piece;
    // U8 flags = NO_FLAGS | board.last_move_sideways();
    // Check if tie-fighter sideway move
    // cout << "piece=" << ((piece&(~1)) == TIEFIGHTER)
    // << ((to&(~C64(0x7))) == (from&(~C64(0x7))))
    // << ", from=" << (int)from << "-" << (int)(from&(~C64(0x7)))
    // << ", to=" << (int)to << "-" << (int)(to&(~C64(0x7))) << endl;
    // if (((piece & (~1)) == TIEFIGHTER) && ((to & (~C64(0x7))) == (from & (~C64(0x7)))))
    // {
    //     // cout << "this is a move sideways"  << endl;
    //     flags |= MOVED_SIDEWAYS;
    // }
    move |= build_move_all(from, to, flags, capture);
    // cout << "move=" << hex <<move << dec << endl;
    return move;
}
