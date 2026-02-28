/*
 * File:   Parser.cpp
 */

#include <cstring>

#include "Parser.h"

#include "CLIUtils.h"
#include "Log.h"
#include "MoveGenerator.h"
#include "MoveList.h"

using std::cout;
using std::endl;
using std::stoi;
using std::string;
using std::vector;

// FEN (Forsyth Edwards Notation) parser
// https://www.chessprogramming.org/Forsyth-Edwards_Notation
class Board Parser::parse_fen(const std::string& fen)
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
        rights = rights | Parser::castling_right(epd[pos++]);
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
std::optional<Move_t> Parser::parse_san(const std::string& str, const Board& board)
{
    size_t len = str.length();

    if (str == "0-0" || str == "O-O")
    {
        return build_castle(KING_CASTLE);
    }
    if (str == "0-0-0" || str == "O-O-O")
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
            return std::nullopt;
    }

    // optional "from" rank
    c = str[pos];
    if (c >= '1' && c <= '8')
    {
        from_rank = c - '1';
        if (pos++ >= len)
            return std::nullopt;
    }

    // optional 'x'
    if (str[pos] == 'x')
    {
        if (pos++ >= len)
            return std::nullopt;
    }

    c = str[pos];
    if (c >= 'a' && c <= 'h')
    {
        from_file = c - 'a';

        if (pos++ >= len)
            return std::nullopt;
        if (str[pos] == 'x')
        {
            if (pos++ >= len)
                return std::nullopt;
        }
        c = str[pos];
        if (c >= '1' && c <= '8')
        {
            from_file = 0;
            to = square(&str[pos - 1]);
        }
        else if (c >= 'a' && c <= 'h')
        {
            if (pos++ >= len)
                return std::nullopt;
            c = str[pos];
            if (c < '1' && c > '8')
                return std::nullopt;
            to = square(&str[pos - 1]);
        }
        else
        {
            return std::nullopt;
        }
    }
    else
    {
        return std::nullopt;
    }

    U8 promo = 0;
    if (pos++ < len)
    {
        if (str[pos] == '=')
        {
            if (pos++ >= len)
                return std::nullopt;
        }
        c = str[pos];
        if (c == 'Q' || c == 'N' || c == 'B' || c == 'R')
        {
            promo = Parser::parse_piece(c) & (0xFEU);
        }
    }

    MoveList list;
    MoveGenerator::add_all_moves(list, board, board.side_to_move());
    int n = list.length();

    for (int i = 0; i < n; i++)
    {
        Move_t move = list[i];
        if (move_to(move) != to)
            continue;
        U8 from = move_from(move);
        if ((board[from] & (0xFEU)) != piece)
            continue;
        if (promo && ((move_promote_to(move) & (0xFEU)) != promo))
            continue;
        if (from_file && (from & 7) != from_file)
            continue;
        if (from_rank && (from >> 3) != from_rank)
            continue;
        return move;
    }
    Log::warning("parse_san: no matching move for '" + str + "'");
    return std::nullopt;
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

U8 Parser::square(const char sq[])
{
    int col = sq[0] - 'a';
    int row = sq[1] - '1';
    return static_cast<U8>((row * 8) + col);
}

std::optional<Move_t> Parser::move(const std::string& str, const Board& board)
{
    size_t len = str.length();

    if (str == "0-0" || str == "O-O")
    {
        return build_castle(KING_CASTLE);
    }
    if (str == "0-0-0" || str == "O-O-O")
    {
        return build_castle(QUEEN_CASTLE);
    }

    U8 side = board.side_to_move();
    U8 from = square(&str[0]);
    U8 to = square(&str[2]);
    U8 piece = board[from];

    if (((side == WHITE) && (piece == WHITE_KING) && (str == "e1g1"))
        || ((side == BLACK) && (piece == BLACK_KING) && (str == "e8g8")))
    {
        return build_castle(KING_CASTLE);
    }
    if (((side == WHITE) && (piece == WHITE_KING) && (str == "e1c1"))
        || ((side == BLACK) && (piece == BLACK_KING) && (str == "e8c8")))
    {
        return build_castle(QUEEN_CASTLE);
    }

    U8 promo = 0;
    if (len >= 5)
    {
        char c = str[4];
        if (c == 'q' || c == 'n' || c == 'b' || c == 'r')
        {
            promo = Parser::parse_piece(c) & (0xFEU);
        }
    }

    MoveList list;
    MoveGenerator::add_all_moves(list, board, board.side_to_move());
    int n = list.length();

    for (int i = 0; i < n; i++)
    {
        Move_t move = list[i];
        if (move_to(move) != to)
            continue;
        if (move_from(move) != from)
            continue;
        if (promo && ((move_promote_to(move) & (0xFEU)) != promo))
            continue;
        return move;
    }
    Log::warning("move: no matching move for '" + str + "'");
    return std::nullopt;
}
