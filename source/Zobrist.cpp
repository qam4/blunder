/*
 * File:   Zobrist.cpp
 *
 */

#include "Zobrist.h"

#include "Board.h"

Zobrist::Zobrist()
{
    // Initialize the random generator
    gen_ = std::mt19937_64(0);

    for (int i = 0; i < NUM_PIECES; i++)
    {
        for (int j = 0; j < NUM_SQUARES; j++)
        {
            pieces_[i][j] = rand64();
        }
    }

    for (int i = 0; i < FULL_CASTLING_RIGHTS + 1; i++)
    {
        castling_rights_[i] = rand64();
    }

    for (int i = 0; i < NUM_SQUARES; i++)
    {
        ep_square_[i] = rand64();
    }
    side_ = rand64();
}

U64 Zobrist::rand64()
{
    std::uniform_int_distribution<uint64_t> dist(0, UINT64_MAX);
    return dist(gen_);
}

U64 Zobrist::get_zobrist_key(const Board& board) const
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
