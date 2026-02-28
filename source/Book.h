/*
 * File:   Book.h
 *
 * Polyglot opening book reader.
 * Parses .bin files in the standard Polyglot format (16-byte entries,
 * big-endian, sorted by key) and returns weighted-random book moves.
 */

#ifndef BOOK_H
#define BOOK_H

#include <random>
#include <string>
#include <vector>

#include "Common.h"
#include "Move.h"

class Board;

class Book
{
public:
    /// Open a Polyglot .bin book file. Returns true on success.
    bool open(const std::string& path);

    /// Returns true if the book is loaded and has entries.
    bool is_open() const { return !entries_.empty(); }

    /// Returns true if the book contains at least one move for the given board.
    bool has_move(const Board& board) const;

    /// Returns a weighted-random book move for the given board, or Move(0) if none found.
    Move_t get_move(const Board& board) const;

    /// Close the book and free memory.
    void close();

    /// Set the maximum ply depth for book usage. 0 = unlimited.
    void set_max_depth(int depth) { max_depth_ = depth; }
    int max_depth() const { return max_depth_; }

    /// Check if the book should be consulted at the given ply (full move count).
    /// ply here is the game ply (number of half-moves played from start).
    bool within_depth(int game_ply) const
    {
        return max_depth_ == 0 || game_ply < max_depth_;
    }

    /// Compute the Polyglot-compatible Zobrist hash for a board position.
    /// This uses the published Polyglot random numbers, NOT the engine's internal keys.
    static U64 polyglot_hash(const Board& board);

private:
    struct PolyglotEntry
    {
        U64 key;
        U16 move;
        U16 weight;
        U32 learn;
    };

    /// Find all book entries matching the given key via binary search.
    std::vector<PolyglotEntry> find_entries(U64 key) const;

    /// Convert a Polyglot move encoding to the engine's Move_t format.
    static Move_t polyglot_move_to_engine_move(U16 pg_move, const Board& board);

    std::vector<PolyglotEntry> entries_;
    int max_depth_ = 0;  ///< Max game ply for book usage. 0 = unlimited.
    mutable std::mt19937 rng_{std::random_device{}()};
};

#endif /* BOOK_H */
