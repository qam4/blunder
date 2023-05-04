/*
 * File:   Parser.cpp
 *
 */

#include "Parser.h"

class Board Parser::parse_fen(string fen)
{
    Board board = Board();

    size_t len = fen.length();

    size_t pos = 0;  // position in string
    //[not used]int square = A1;

    // 8 rows of pieces
    for (int row = 7; row >= 0; row--)
    {
        while (fen[pos] == '/')
            pos++;
        for (int col = 0; col < 8; col++)
        {
            char c = fen[pos++];
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
    while (fen[pos] != ' ')
        if (pos++ >= len)
            return board;
    while (fen[pos] == ' ')
        if (pos++ >= len)
            return board;

    // side to move
    U8 side_to_move = Parser::side(fen[pos++]);
    board.set_side_to_move(side_to_move);

    while (fen[pos] == ' ')
        if (pos++ >= len)
            return board;

    // castling rights
    U8 rights = 0;
    while (fen[pos] != ' ')
    {
        rights |= Parser::castling_right(fen[pos++]);
    }
    board.set_castling_rights(rights);

    while (fen[pos] == ' ')
        if (pos++ >= len)
            return board;

    // ep square
    if (fen[pos] == '-')
    {
        pos++;
    }
    else
    {
        char square[2] = { fen[pos], fen[pos + 1] };
        board.set_ep_square(Parser::square(square));
        while (fen[pos] != ' ')
            if (pos++ >= len)
                return board;
    }

    while (fen[pos] == ' ')
        if (pos++ >= len)
            return board;

    // half-move-count
    int half_move_count = 0;
    while (fen[pos] >= '0' && fen[pos] <= '9')
    {
        half_move_count = half_move_count * 10 + (fen[pos] - '0');
        if (pos++ >= len)
            return board;
    }

    board.set_half_move_count(static_cast<U8>(half_move_count));

    while (fen[pos] == ' ')
        if (pos++ >= len)
            return board;

    // full-move-count
    int full_move_count = 0;
    while (fen[pos] != ' ')
    {
        full_move_count = full_move_count * 10 + (fen[pos] - '0');
        if (pos++ >= len)
            return board;
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
    return 0;
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
    // cout << "move=" << hex<<move << endl;
    return move;
}
