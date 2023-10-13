/*
 * File:   Parser.cpp
 *
 */

#include <cstring>

#include "Parser.h"

#include "CLIUtils.h"
#include "MoveGenerator.h"
#include "MoveList.h"

// FEN (Forsyth Edwards Notation) parser
// https://www.chessprogramming.org/Forsyth-Edwards_Notation
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
        int half_move_count = str2int(token);
        board.set_half_move_count(static_cast<U8>(half_move_count));
    }

    if (num_tokens > 5)
    {
        // full-move-count
        token = tokens[5];
        int full_move_count = str2int(token);
        board.set_full_move_count(static_cast<U8>(full_move_count));
    }
    board.update_hash();

    return board;
}

// EPD (Extended Position Description) parser
// https://www.chessprogramming.org/Extended_Position_Description
class Board Parser::parse_epd(string epd)
{
    Board board = Board();
    size_t len = epd.length();
    size_t pos = 0;  // position in string

    // 8 rows of pieces
    for (int row = 7; row >= 0; row--)
    {
        while (epd[pos] == '/')
            pos++;
        for (int col = 0; col < 8; col++)
        {
            char c = epd[pos++];
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
    while (epd[pos] != ' ')
        if (pos++ >= len)
            return board;
    while (epd[pos] == ' ')
        if (pos++ >= len)
            return board;

    // side to move
    U8 side_to_move = Parser::side(epd[pos++]);
    board.set_side_to_move(side_to_move);

    while (epd[pos] == ' ')
        if (pos++ >= len)
            return board;

    // castling rights
    U8 rights = 0;
    while (epd[pos] != ' ')
    {
        rights |= Parser::castling_right(epd[pos++]);
    }
    board.set_castling_rights(rights);

    while (epd[pos] == ' ')
        if (pos++ >= len)
            return board;

    // ep square
    if (epd[pos] == '-')
    {
        pos++;
    }
    else
    {
        char square[2] = { epd[pos], epd[pos + 1] };
        board.set_ep_square(Parser::square(square));
        while (epd[pos] != ' ')
            if (pos++ >= len)
                return board;
    }

    while (epd[pos] == ' ')
        if (pos++ >= len)
            return board;

    // Operations
    while (pos < len)
    {
        string opcode;
        while (epd[pos] != ' ')
        {
            opcode += epd[pos++];
        }
        while (epd[pos] == ' ')
            if (pos++ >= len)
                return board;
        cout << "opcode=" << opcode << endl;

        string operand;
        while (epd[pos] != ';')
        {
            operand += epd[pos++];
        }
        cout << "operand=" << operand << endl;

        board.set_epd_op(opcode, operand);

        while (epd[pos] == ';' || epd[pos] == ' ')
            if (pos++ >= len)
                return board;
    }

    // halfmove clock (default 0)
    int half_move_count = 0;
    if (!board.epd_op("hmvc").empty())
    {
        half_move_count = str2int(board.epd_op("hmvc"));
    }
    board.set_half_move_count(half_move_count);

    // fullmove number (default 1)
    int full_move_count = 1;
    if (!board.epd_op("fmvn").empty())
    {
        full_move_count = str2int(board.epd_op("fmvn"));
    }
    board.set_full_move_count(full_move_count);

    board.update_hash();

    return board;
}

// SAN
// https://cfajohnson.com/chess/SAN/
Move_t Parser::parse_san(string str, const Board& board)
{
    Move_t move_found = 0;
    size_t len = str.length();

    if ((str.compare("0-0") == 0) || (str.compare("O-O") == 0))
    {
        return build_castle(KING_CASTLE);
    }
    if ((str.compare("0-0-0") == 0) || (str.compare("O-O-O") == 0))
    {
        return build_castle(QUEEN_CASTLE);
    }

    size_t pos = 0;
    int from_rank = 0;
    int from_file = 0;
    U8 to = 0;

    // Piece
    U8 piece = PAWN;
    char c = str[pos];
    if (c == 'K' || c == 'Q' || c == 'R' || c == 'B' || c == 'N')
    {
        piece = Parser::parse_piece(c) & (0xFEU);
        if (pos++ >= len)
            return move_found;
    }

    // optional "from" rank
    c = str[pos];
    if (c >= '1' && c <= '8')
    {
        from_rank = c - '1';
        if (pos++ >= len)
            return move_found;
    }

    // optional 'x'
    if (str[pos] == 'x')
    {
        if (pos++ >= len)
            return move_found;
    }

    c = str[pos];
    if (c >= 'a' && c <= 'h')
    {
        // This could be
        // - an optional "from" file, then the next char should be the "to" file
        // - or the "to" file, then the next char should be the "to" rank
        // so, we need to look ahead
        from_file = c - 'a';

        if (pos++ >= len)
            return move_found;
        // ignore 'x'
        if (str[pos] == 'x')
        {
            if (pos++ >= len)
                return move_found;
        }
        c = str[pos];
        if (c >= '1' && c <= '8')
        {
            // found the "to" rank
            from_file = 0;  // reset this
            to = square(&str[pos - 1]);
        }
        else if (c >= 'a' && c <= 'h')
        {
            // found the "to" file
            if (pos++ >= len)
                return move_found;
            c = str[pos];
            if (c < '1' && c > '8')
                return move_found;
            to = square(&str[pos - 1]);
        }
        else
        {
            return move_found;
        }
    }
    else
    {
        return move_found;
    }

    U8 promo = 0;
    if (pos++ < len)
    {
        c = str[pos];
        if (c == 'Q' || c == 'N' || c == 'B' || c == 'R')
        {
            promo = Parser::parse_piece(c) & (0xFEU);
        }
    }
    // ignore the rest of the string

    MoveList list;
    MoveGenerator::add_all_moves(list, board, board.side_to_move());
    int n = list.length();

    for (int i = 0; i < n; i++)
    {
        Move_t move = list[i];
        // check if same "to" square
        if (move_to(move) != to)
        {
            continue;
        }
        // check if same piece moved
        U8 from = move_from(move);
        if (board[from] != piece)
        {
            continue;
        }
        // check promotion
        if (promo && move_promote_to(move) != promo)
        {
            continue;
        }
        // check "from" file
        if (from_file && (from & 7) != from_file)
        {
            continue;
        }
        // check "from" rank
        if (from_rank && (from >> 3) != from_rank)
        {
            continue;
        }
        move_found = move;
        break;
    }
    return move_found;
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

Move_t Parser::move(string str, const Board& board)
{
    U8 side, from, to, piece, capture;
    Move_t move = 0;
    side = board.side_to_move();

    // Remove trailing new line
    if (!str.empty() && str[str.length() - 1] == '\n')
    {
        str.erase(str.length() - 1);
    }
    if ((str.compare("O-O") == 0) || ((side == WHITE) && (str.compare("e1g1") == 0))
        || ((side == BLACK) && (str.compare("e8g8") == 0)))
    {
        return build_castle(KING_CASTLE);
    }
    if ((str.compare("O-O-O") == 0) || ((side == WHITE) && (str.compare("e1c1") == 0))
        || ((side == BLACK) && (str.compare("e8c8") == 0)))
    {
        return build_castle(QUEEN_CASTLE);
    }

    from = square(&str[0]);
    to = square(&str[2]);
    piece = board[from];
    capture = board[to];
    if ((from == NULL_SQUARE) || (to == NULL_SQUARE))
        return 0;

    if (piece == PAWN)
    {
        // Promotion
        // e7e8q
        if (((side == WHITE) && ((from & 56) == 6 * 8) && ((to & 56) == 7 * 8))
            || ((side == BLACK) && ((from & 56) == 1 * 8) && ((to & 56) == 0 * 8)))
        {
            U8 promo = Parser::parse_piece(str[4]) & (0xFEU);
            if (capture != EMPTY)
            {
                return build_capture_promotion(from, to, capture, promo | side);
            }
            else
            {
                return build_promotion(from, to, promo | side);
            }
        }

        // Double push
        if (((side == WHITE) && ((from & 56) == 1 * 8) && ((to & 56) == 3 * 8)
             && ((to - from) == 16))
            || ((side == BLACK) && ((from & 56) == 6 * 8) && ((to & 56) == 5 * 8)
                && ((from - to) == 16)))
        {
            return build_pawn_double_push(from, to);
        }

        // EP capture
        if (((side == WHITE) && ((from & 56) == 4 * 8) && ((to & 56) == 5 * 8)
             && (((to - from) == 7) || ((to - from) == 9)))
            || ((side == BLACK) && ((from & 56) == 3 * 8) && ((to & 56) == 2 * 8)
                && (((from - to) == 7) || ((from - to) == 9))))
        {
            return build_ep_capture(from, to, PAWN | !side);
        }
    }

    move |= build_move_all(from, to, capture, NO_FLAGS);
    return move;
}
