/*
 * File:   Zobrist.cpp
 *
 */

#include "Zobrist.h"

#include "Board.h"

Zobrist::Zobrist()
{
    // Initialize the random generator
    gen = std::mt19937_64(0);

    for (int i = 0; i < NUM_PIECES; i++)
    {
        for (int j = 0; j < NUM_SQUARES; j++)
        {
            pieces[i][j] = rand64();
        }
    }

    for (int i = 0; i < FULL_CASTLING_RIGHTS + 1; i++)
    {
        castling_rights[i] = rand64();
    }

    for (int i = 0; i < NUM_PIECES; i++)
    {
        ep_square[i] = rand64();
    }
    side = rand64();
}

U64 Zobrist::rand64()
{
    std::uniform_int_distribution<uint64_t> dist(0, UINT64_MAX);
    return dist(gen);
}

U64 Zobrist::get_zobrist_key(const Board& board)
{
    U64 zobrist_key = 0;

    for (int i = 0; i < NUM_SQUARES; i++)
    {
        U8 piece = board[i];
        zobrist_key ^= pieces[piece][i];
    }

    zobrist_key ^= castling_rights[board.castling_rights()];
    zobrist_key ^= ep_square[board.ep_square()];
    if (board.side_to_move())
    {
        zobrist_key ^= side;
    }

    return zobrist_key;
}
