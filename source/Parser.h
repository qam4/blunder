/*
 * File:   Parser.h
 */

#ifndef PARSER_H
#define PARSER_H

#include <optional>

#include "Board.h"
#include "Common.h"
#include "Move.h"

class Parser
{
  public:
    static Board parse_fen(const std::string& fen);
    static Board parse_epd(std::string epd);
    static std::optional<Move_t> parse_san(const std::string& str, const Board& board);
    static U8 parse_piece(char piece);
    static U8 side(char c);
    static U8 castling_right(char c);
    static U8 square(const char sq[]);
    static std::optional<Move_t> move(const std::string& str, const Board& board);
};

#endif /* PARSER_H */
