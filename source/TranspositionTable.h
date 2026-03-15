/*
 * File:   TranspositionTable.h
 *
 * Transposition table class that owns the hash table storage.
 */

#ifndef TRANSPOSITION_TABLE_H
#define TRANSPOSITION_TABLE_H

#include <vector>

#include "Types.h"
#include "Move.h"
#include "Constants.h"

constexpr int HASH_TABLE_SIZE = 1024 * 1024;  // legacy, used only as fallback
constexpr int DEFAULT_HASH_SIZE_MB = 64;

constexpr int HASH_EXACT = 0;
constexpr int HASH_ALPHA = 1;
constexpr int HASH_BETA  = 2;

struct HASHE {
    U64 key;
    int depth;
    int flags;
    int value;
    Move_t best_move;
    uint8_t generation;
};

class TranspositionTable {
public:
    explicit TranspositionTable(int size = HASH_TABLE_SIZE);

    void clear();
    void resize(int size_mb);
    void new_generation() { generation_ = static_cast<uint8_t>((generation_ + 1) & 0xFF); }
    uint8_t generation() const { return generation_; }
    int probe(U64 hash, int depth, int alpha, int beta, Move_t& best_move,
             int* tt_depth_out = nullptr, int* tt_flags_out = nullptr,
             int* tt_value_out = nullptr);
    void record(U64 hash, int depth, int val, int flags, Move_t best_move);

private:
    std::vector<HASHE> table_;
    size_t mask_;  // size_ - 1, for bitmask indexing
    uint8_t generation_ = 0;

    /// Round down to nearest power of two
    static size_t to_power_of_two(size_t n);
};

#endif /* TRANSPOSITION_TABLE_H */
