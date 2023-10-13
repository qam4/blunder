/*
 * File:   Parser.h
 *
 */

#ifndef PARSER_H
#define PARSER_H

#include "Common.h"
#include "Board.h"
#include "Move.h"

class Parser
{
public:
    static class Board parse_fen(string fen);
    static class Board parse_epd(string epd);
    static Move_t parse_san(string str, const Board &board);
    static U8 parse_piece(char piece);
    static U8 side(char c);
    static U8 castling_right(char c);
    static U8 square(char sq[]);
    static Move_t move(string str, const Board &board);

private:
};

#endif /* PARSER_H */