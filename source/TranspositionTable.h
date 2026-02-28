/*
 * File:   TranspositionTable.h
 *
 * Transposition table class that owns the hash table storage.
 * Replaces the global hash_table array and #define constants from Hash.h.
 */

#ifndef TRANSPOSITION_TABLE_H
#define TRANSPOSITION_TABLE_H

#include <vector>

#include "Types.h"
#include "Move.h"
#include "Constants.h"

constexpr int HASH_TABLE_SIZE = 1024 * 1024;

constexpr int HASH_EXACT = 0;  // the value of the node is exactly the value stored
constexpr int HASH_ALPHA = 1;  // the value of the node is at most the value stored
constexpr int HASH_BETA  = 2;  // the value of the node is at least the value stored

struct HASHE {
    U64 key;
    int depth;
    int flags;
    int value;
    Move_t best_move;
};

class TranspositionTable {
public:
    TranspositionTable(int size = HASH_TABLE_SIZE);

    void clear();
    int probe(U64 hash, int depth, int alpha, int beta, Move_t& best_move);
    void record(U64 hash, int depth, int val, int flags, Move_t best_move);

private:
    std::vector<HASHE> table_;
};

#endif /* TRANSPOSITION_TABLE_H */
