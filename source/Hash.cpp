/*
 * File:   Hash.cpp
 *
 * Board::probe_hash and Board::record_hash now delegate to TranspositionTable.
 * The global hash_table array and reset_hash_table() have been removed.
 */

#include "Board.h"

int Board::probe_hash(int depth, int alpha, int beta, Move_t& best_move)
{
    return tt_->probe(get_hash(), depth, alpha, beta, best_move);
}

void Board::record_hash(int depth, int val, int flags, Move_t best_move)
{
    tt_->record(get_hash(), depth, val, flags, best_move);
}
