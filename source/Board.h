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
#include "PrincipalVariation.h"
#include "TimeManager.h"
#include "Evaluator.h"

#define NO_PV 0  // Not a PV node
#define IS_PV 1
#define NO_NULL 0  // avoid doing null move twice in a row
#define DO_NULL 1


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
    map<string, string> epd_;

    Zobrist zobrist_;
    std::shared_ptr<TranspositionTable> tt_;
    PrincipalVariation pv_;
    TimeManager tm_;
    HandCraftedEvaluator evaluator_;
    U64 hash_history_[MAX_GAME_PLY];

    int searched_moves_;
    int nodes_visited_;
    int game_ply_;
    int search_ply_;
    int max_search_ply_;
    Move_t search_best_move_;
    int search_best_score_;

    int follow_pv_;

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
    // Algos
    Move_t search(int depth, int search_time=DEFAULT_SEARCH_TIME, int max_nodes_visited=-1, bool xboard=false);
    int minimax(int depth, bool maximizing_player);
    Move_t minimax_root(int depth, bool maximizing_player);
    int negamax(int depth);
    Move_t negamax_root(int depth);
    int alphabeta(int alpha, int beta, int depth, int is_pv, int can_null);
    int quiesce(int alpha, int beta);

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
    int get_searched_moves() const { return searched_moves_; };
    int get_game_ply() const { return game_ply_; };
    int get_search_best_score() const { return search_best_score_; }
    U64 get_hash_history(const int i) const { return hash_history_[i]; }

    void update_hash();
    int probe_hash(int depth, int alpha, int beta, Move_t &best_move);
    void record_hash(int depth, int val, int flags, Move_t best_move);
    TranspositionTable& get_tt() { return *tt_; }
    PrincipalVariation& get_pv() { return pv_; }
    TimeManager& get_tm() { return tm_; }
    Evaluator& get_evaluator() { return evaluator_; }

    // EPD
    void set_epd_op(const string& opcode, const string& operand) { epd_[opcode] = operand; }
    string& epd_op(const string& opcode) {return epd_[opcode]; }
};

#endif /* BOARD_H */