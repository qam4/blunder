/*
 * File:   Output.h
 * Author: pj
 *
 * Created on January 7, 2013, 11:38 PM
 */

#ifndef OUTPUT_H
#define OUTPUT_H

#include "Common.h"
#include "Board.h"
#include "Move.h"

class Output
{

public:
    static std::string board(const class Board &board);
    static std::string bitboard(U64 bb);
    static std::string move(Move_t move, const class Board &board);
    static std::string move_san(Move_t move, class Board &board);
    static std::string movelist(const class MoveList &list, const class Board &board);
    static std::string square(U8 square);
    static std::string piece(U8 piece);
    static std::string castling_rights(U8 castling_rights);
    static std::string board_to_fen(const class Board &board);
    static void set_colors_enabled(bool enable) { colors_enabled_ = enable; }
    static bool is_colors_enabled() { return colors_enabled_ == true; }
    static void set_unicode_enabled(bool enable) { unicode_enabled_ = enable; }
    static bool is_unicode_enabled() { return unicode_enabled_ == true; }

private:
    static bool colors_enabled_;
    static bool unicode_enabled_;
};

#endif /* OUTPUT_H */
