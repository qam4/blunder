/*
 * File:   Board.h
 *
 */

#ifndef BOARD_H
#define BOARD_H

#include "Common.h"
#include "Move.h"
#include "Zobrist.h"
#include "Hash.h"

int pop_count(U64 x);
bool inline is_valid_piece(U8 piece) { return (piece >= WHITE_PAWN) && (piece <= BLACK_KING); }
bool inline is_valid_square(int square) { return (square >= 0) && (square <= 64); }
bool inline is_piece_slider(U8 piece) { return (piece >= WHITE_BISHOP) && (piece <= BLACK_QUEEN); }

class Board
{
    friend class MoveGenerator;
    friend class Tests;

private:
    U64 bitboards[14];
    U8 board_array[64];

    struct IrreversibleData
    {
        U8 full_move_count;
        U8 half_move_count;
        U8 castling_rights;
        U8 ep_square;
        U8 side_to_move;
        U64 board_hash;
    } irrev;

    Zobrist zobrist;
    U64 hash_history[MAX_GAME_PLY];

    int searched_moves;
    int game_ply;
    int search_ply;
    int max_search_ply;
    clock_t search_start_time;
    Move_t search_best_move;
    int search_best_score;
    int search_time;       // search time in usec

    int follow_pv;

    struct IrreversibleData move_stack[MAX_SEARCH_PLY];

public:
    Board();
    bool is_blank();
    void add_piece(U8 piece, int square);
    void remove_piece(int square);
    void reset();
    void do_move(Move_t move);
    void undo_move(Move_t move);
    int evaluate();
    bool is_game_over();
    bool is_draw();
    // Algos
    Move_t search(int depth, int searchtime=MAX_SEARCH_TIME, bool xboard=false);
    int minimax(int depth, bool maximizing_player);
    Move_t minimax_root(int depth, bool maximizing_player);
    int negamax(int depth);
    Move_t negamax_root(int depth);
    int alphabeta(int alpha, int beta, int depth);
    int quiesce(int alpha, int beta);
    bool is_search_time_over();

    U8 operator[](const int square) const; // return piece on that square
    U64 bitboard(const int type) const;
    U8 half_move_count() const { return irrev.half_move_count; };
    U8 full_move_count() const { return irrev.full_move_count; };
    U8 castling_rights() const { return irrev.castling_rights; };
    U8 ep_square()       const { return irrev.ep_square; };
    U8 side_to_move()    const { return irrev.side_to_move; };
    U64 get_hash()       const { return irrev.board_hash; };
    void set_side_to_move(U8 side) { irrev.side_to_move = side; };
    void set_castling_rights(U8 rights) { irrev.castling_rights = rights; };
    void set_ep_square(U8 square) { irrev.ep_square = square; };
    void set_half_move_count(U8 count) { irrev.half_move_count = count; };
    void set_full_move_count(U8 count) { irrev.full_move_count = count; };
    void set_hash(U64 hash) { irrev.board_hash = hash; };
    int get_searched_moves() const { return searched_moves; };
    int get_game_ply() const { return game_ply; };
    int get_search_best_score() const { return search_best_score; }
    U64 get_hash_history(const int i) const { return hash_history[i]; }

    void update_hash();
    int probe_hash(int depth, int alpha, int beta, Move_t &best_move);
    void record_hash(int depth, int val, int flags, Move_t best_move);

    void store_pv_move(Move_t move);
    void print_pv();
    void sort_pv_move(class MoveList &list, Move_t best_move);
};

#endif /* BOARD_H */