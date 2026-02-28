/*
 * File:   Board.h
 *
 */

#ifndef BOARD_H
#define BOARD_H

#include <map>
#include <memory>

#include "Common.h"
#include "Move.h"
#include "Zobrist.h"
#include "TranspositionTable.h"
#include "Evaluator.h"

int pop_count(U64 x);
bool inline is_valid_piece(U8 piece) { return (piece >= WHITE_PAWN) && (piece <= BLACK_KING); }
bool inline is_valid_square(int square) { return (square >= 0) && (square <= 64); }
bool inline is_piece_slider(U8 piece) { return (piece >= WHITE_BISHOP) && (piece <= BLACK_QUEEN); }

class Board
{
    friend class MoveGenerator;
    friend class Tests;

private:
    U64 bitboards_[14];
    U8 board_array_[64];

    struct IrreversibleData
    {
        int full_move_count;
        int half_move_count;
        U8 castling_rights;
        U8 ep_square;
        U8 side_to_move;
        U64 board_hash;
    } irrev_;

    // Extended Position Description
    std::map<std::string, std::string> epd_;

    Zobrist zobrist_;
    std::shared_ptr<TranspositionTable> tt_;
    HandCraftedEvaluator evaluator_;
    U64 hash_history_[MAX_GAME_PLY];

    int game_ply_;
    int search_ply_;
    int max_search_ply_;

    struct IrreversibleData move_stack_[MAX_SEARCH_PLY];

public:
    Board();
    bool is_blank();
    void add_piece(U8 piece, int square);
    void remove_piece(int square);
    void reset();
    void do_move(Move_t move);
    void undo_move(Move_t move);
    void do_null_move();
    void undo_null_move();
    bool is_game_over();
    bool is_draw(bool in_search = false);

    U8 operator[](const int square) const; // return piece on that square
    U64 bitboard(const int type) const;
    int half_move_count() const { return irrev_.half_move_count; };
    int full_move_count() const { return irrev_.full_move_count; };
    U8 castling_rights() const { return irrev_.castling_rights; };
    U8 ep_square()       const { return irrev_.ep_square; };
    U8 side_to_move()    const { return irrev_.side_to_move; };
    U64 get_hash()       const { return irrev_.board_hash; };
    void set_side_to_move(U8 side) { irrev_.side_to_move = side; };
    void set_castling_rights(U8 rights) { irrev_.castling_rights = rights; };
    void set_ep_square(U8 square) { irrev_.ep_square = square; };
    void set_half_move_count(int count) { irrev_.half_move_count = count; };
    void set_full_move_count(int count) { irrev_.full_move_count = count; };
    void set_hash(U64 hash) { irrev_.board_hash = hash; };
    int get_game_ply() const { return game_ply_; };
    int get_search_ply() const { return search_ply_; }
    void set_search_ply(int ply) { search_ply_ = ply; }
    U64 get_hash_history(const int i) const { return hash_history_[i]; }

    void update_hash();
    TranspositionTable& get_tt() { return *tt_; }
    Evaluator& get_evaluator() { return evaluator_; }

    // EPD
    void set_epd_op(const std::string& opcode, const std::string& operand) { epd_[opcode] = operand; }
    std::string& epd_op(const std::string& opcode) {return epd_[opcode]; }
};

#endif /* BOARD_H */