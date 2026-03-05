/*
 * File:   TranspositionTable.cpp
 *
 * Implementation of the TranspositionTable class.
 */

#include "TranspositionTable.h"

size_t TranspositionTable::to_power_of_two(size_t n)
{
    if (n == 0)
    {
        return 1;
    }
    size_t p = 1;
    while (p < n)
    {
        p <<= 1;
    }
    // p is now >= n; if p overshot, go back one step
    // But we want the largest power of two <= n
    if (p > n)
    {
        p >>= 1;
    }
    return (p > 0) ? p : 1;
}

TranspositionTable::TranspositionTable(int size)
{
    size_t actual = to_power_of_two(static_cast<size_t>(size));
    table_.resize(actual);
    mask_ = actual - 1;
    clear();
}

void TranspositionTable::clear()
{
    for (auto& entry : table_)
    {
        entry.key = 0;
        entry.depth = 0;
        entry.flags = 0;
        entry.value = 0;
        entry.best_move = 0U;
    }
}

void TranspositionTable::resize(int size_mb)
{
    if (size_mb < 1)
    {
        size_mb = 1;
    }
    size_t bytes = static_cast<size_t>(size_mb) * 1024 * 1024;
    size_t entries = bytes / sizeof(HASHE);
    size_t actual = to_power_of_two(entries);
    if (actual == 0)
    {
        actual = 1;
    }
    table_.resize(actual);
    mask_ = actual - 1;
    clear();
}

int TranspositionTable::probe(U64 hash, int depth, int alpha, int beta, Move_t& best_move)
{
    HASHE* phashe = &table_[hash & mask_];

    if (phashe->key == hash)
    {
        best_move = phashe->best_move;

        if (phashe->depth >= depth)
        {
            if (phashe->flags == HASH_EXACT)
            {
                return phashe->value;
            }
            if ((phashe->flags == HASH_ALPHA) && (phashe->value <= alpha))
            {
                return alpha;
            }
            if ((phashe->flags == HASH_BETA) && (phashe->value >= beta))
            {
                return beta;
            }
        }
    }

    return UNKNOWN_SCORE;
}

void TranspositionTable::record(U64 hash, int depth, int val, int flags, Move_t best_move)
{
    HASHE* phashe = &table_[hash & mask_];

    phashe->key = hash;
    phashe->best_move = best_move;
    phashe->value = val;
    phashe->flags = flags;
    phashe->depth = depth;
}
