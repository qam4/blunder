/*
 * File:   TranspositionTable.cpp
 *
 * Implementation of the TranspositionTable class.
 * Logic moved from Hash.cpp (probe_hash / record_hash / reset_hash_table).
 */

#include "TranspositionTable.h"

TranspositionTable::TranspositionTable(int size)
    : table_(static_cast<size_t>(size))
{
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

int TranspositionTable::probe(U64 hash, int depth, int alpha, int beta, Move_t& best_move)
{
    HASHE* phashe = &table_[static_cast<size_t>(hash % table_.size())];

    if (phashe->key == hash)
    {
        // best move is expected to be good even if the depth is not sufficient
        best_move = phashe->best_move;

        if (phashe->depth >= depth)
        {
            if (phashe->flags == HASH_EXACT)
            {
                // node_value = hash_value
                return phashe->value;
            }
            if ((phashe->flags == HASH_ALPHA) && (phashe->value <= alpha))
            {
                // node_value <= hash_value <= alpha
                return alpha;
            }
            if ((phashe->flags == HASH_BETA) && (phashe->value >= beta))
            {
                // node_value >= hash_value >= beta
                return beta;
            }
        }
    }

    return UNKNOWN_SCORE;
}

void TranspositionTable::record(U64 hash, int depth, int val, int flags, Move_t best_move)
{
    HASHE* phashe = &table_[static_cast<size_t>(hash % table_.size())];

    phashe->key = hash;
    phashe->best_move = best_move;
    phashe->value = val;
    phashe->flags = flags;
    phashe->depth = depth;
}
