/*
 * File:   Zobrist.cpp
 *
 */

#include <random>

#include "Zobrist.h"

#include "Board.h"

// Static member definitions
U64 Zobrist::pieces_[NUM_PIECES][NUM_SQUARES] = {};
U64 Zobrist::castling_rights_[FULL_CASTLING_RIGHTS + 1] = {};
U64 Zobrist::ep_square_[NUM_SQUARES] = {};
U64 Zobrist::side_ = 0;
bool Zobrist::initialized_ = false;

void Zobrist::init()
{
    if (initialized_)
    {
        return;
    }

    std::mt19937_64 gen(0);
    std::uniform_int_distribution<uint64_t> dist(0, UINT64_MAX);

    for (int i = 0; i < NUM_PIECES; i++)
    {
        for (int j = 0; j < NUM_SQUARES; j++)
        {
            pieces_[i][j] = dist(gen);
        }
    }

    for (int i = 0; i < FULL_CASTLING_RIGHTS + 1; i++)
    {
        castling_rights_[i] = dist(gen);
    }

    for (int i = 0; i < NUM_SQUARES; i++)
    {
        ep_square_[i] = dist(gen);
    }
    side_ = dist(gen);

    initialized_ = true;
}

U64 Zobrist::get_zobrist_key(const Board& board)
{
    U64 zobrist_key = 0;

    for (int i = 0; i < NUM_SQUARES; i++)
    {
        U8 piece = board[i];
        if (piece != EMPTY)
        {
            zobrist_key ^= pieces_[piece][i];
        }
    }

    zobrist_key ^= castling_rights_[board.castling_rights()];

    U8 square = board.ep_square();
    if (square != NULL_SQUARE)
    {
        zobrist_key ^= ep_square_[square];
    }

    if (board.side_to_move())
    {
        zobrist_key ^= side_;
    }

    return zobrist_key;
}
