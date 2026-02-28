/*
 * File:   Move.h
 */

#ifndef MOVES_H
#define MOVES_H

#include "Types.h"

// Bit layout for move encoding (lower 24 bits):
//   bits  0- 5: from square
//   bits  6-11: to square
//   bits 12-15: captured piece
//   bits 16-23: flags (promotion piece, double push, EP, castling)

// Shift/mask constants
constexpr int FROM_SHIFT = 0;
constexpr U32 FROM_MASK = 0x3FU << FROM_SHIFT;
constexpr int TO_SHIFT = 6;
constexpr U32 TO_MASK = 0x3FU << TO_SHIFT;
constexpr int CAPTURE_SHIFT = 12;
constexpr U32 CAPTURE_MASK = 0xFU << CAPTURE_SHIFT;
constexpr int FLAGS_SHIFT = 16;
constexpr U32 FLAGS_MASK = 0xFFU << FLAGS_SHIFT;
constexpr U32 NO_FLAGS = 0x0U;
constexpr U32 PROMOTE_TO_MASK = 0xFU << FLAGS_SHIFT;
constexpr U32 PAWN_DOUBLE_PUSH = 16U << FLAGS_SHIFT;
constexpr U32 EP_CAPTURE = 32U << FLAGS_SHIFT;
constexpr U32 KING_CASTLE = 64U << FLAGS_SHIFT;
constexpr U32 QUEEN_CASTLE = 128U << FLAGS_SHIFT;
constexpr U32 CASTLE_MASK = KING_CASTLE | QUEEN_CASTLE;

// Mask covering only the move identity bits (no score)
constexpr U32 MOVE_IDENTITY_MASK = 0x00FFFFFFU;

// Move class: thin wrapper around U32 (lower 24 bits only, no score)
struct Move
{
    U32 data = 0;

    Move() = default;
    constexpr Move(U32 raw)  // NOLINT: intentional implicit conversion for backward compat
        : data(raw & MOVE_IDENTITY_MASK)
    {
    }

    // Implicit conversion to U32 for backward compatibility
    operator U32() const { return data; }  // NOLINT

    // Accessors
    U8 from() const { return static_cast<U8>(data & FROM_MASK); }
    U8 to() const { return static_cast<U8>((data & TO_MASK) >> TO_SHIFT); }
    U8 captured() const { return static_cast<U8>((data & CAPTURE_MASK) >> CAPTURE_SHIFT); }
    U8 flags() const { return static_cast<U8>((data & FLAGS_MASK) >> FLAGS_SHIFT); }
    U8 promote_to() const { return static_cast<U8>((data & PROMOTE_TO_MASK) >> FLAGS_SHIFT); }

    bool is_capture() const { return (data & CAPTURE_MASK) != 0; }
    bool is_promotion() const { return (data & PROMOTE_TO_MASK) != 0; }
    bool is_ep_capture() const { return (data & EP_CAPTURE) != 0; }
    bool is_castle() const { return (data & CASTLE_MASK) != 0; }
    bool is_pawn_double_push() const { return (data & PAWN_DOUBLE_PUSH) != 0; }

    // Raw access (for TT storage, comparison, etc.)
    U32 raw() const { return data; }

    bool operator==(Move other) const { return data == other.data; }
    bool operator!=(Move other) const { return data != other.data; }
    bool operator==(U32 other) const { return data == (other & MOVE_IDENTITY_MASK); }
    bool operator!=(U32 other) const { return data != (other & MOVE_IDENTITY_MASK); }
    friend bool operator==(U32 lhs, Move rhs) { return rhs == lhs; }
    friend bool operator!=(U32 lhs, Move rhs) { return rhs != lhs; }
};

// Backward-compatible type alias — Move_t is now Move
using Move_t = Move;

// Builder functions (return Move)
inline Move build_move(U8 from, U8 to)
{
    return Move(static_cast<U32>(from | (to << TO_SHIFT)));
}
inline Move build_capture(U8 from, U8 to, U8 capture)
{
    return Move(static_cast<U32>(from | (to << TO_SHIFT) | (capture << CAPTURE_SHIFT)));
}
inline Move build_ep_capture(U8 from, U8 to, U8 capture)
{
    return Move(static_cast<U32>(from | (to << TO_SHIFT) | (capture << CAPTURE_SHIFT)) | EP_CAPTURE);
}
inline Move build_promotion(U8 from, U8 to, U8 promote_to)
{
    return Move(static_cast<U32>(from | (to << TO_SHIFT) | (promote_to << FLAGS_SHIFT)));
}
inline Move build_capture_promotion(U8 from, U8 to, U8 capture, U8 promote_to)
{
    return Move(static_cast<U32>(from | (to << TO_SHIFT) | (capture << CAPTURE_SHIFT)
                                 | (promote_to << FLAGS_SHIFT)));
}
inline Move build_pawn_double_push(U8 from, U8 to)
{
    return Move(static_cast<U32>(from | (to << TO_SHIFT)) | PAWN_DOUBLE_PUSH);
}
inline Move build_castle(U32 flags)
{
    return Move(flags);
}
inline Move build_move_all(U8 from, U8 to, U8 capture, U32 flags)
{
    return Move(static_cast<U32>(from | (to << TO_SHIFT) | (capture << CAPTURE_SHIFT)) | flags);
}

// Free-function accessors (backward compat, delegate to Move methods)
inline U8 move_from(Move m) { return m.from(); }
inline U8 move_to(Move m) { return m.to(); }
inline U8 move_captured(Move m) { return m.captured(); }
inline U8 move_flags(Move m) { return m.flags(); }
inline U8 move_promote_to(Move m) { return m.promote_to(); }
inline bool is_capture(Move m) { return m.is_capture(); }
inline bool is_promotion(Move m) { return m.is_promotion(); }
inline bool is_ep_capture(Move m) { return m.is_ep_capture(); }
inline bool is_castle(Move m) { return m.is_castle(); }
inline bool is_pawn_double_push(Move m) { return m.is_pawn_double_push(); }

// ScoredMove: pairs a Move with a sort score (used only in MoveList)
struct ScoredMove
{
    Move move;
    U16 score = 0;

    ScoredMove() = default;
    ScoredMove(Move m, U16 s)
        : move(m)
        , score(s)
    {
    }
    explicit ScoredMove(Move m)
        : move(m)
        , score(0)
    {
    }
};

#endif /* MOVES_H */
