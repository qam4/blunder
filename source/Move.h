/*
 * File:   Moves.h
 *
 */

#ifndef MOVES_H
#define MOVES_H

#include "Types.h"

typedef U32 Move_t;

static const int FROM_SHIFT = 0;
static const U32 FROM_MASK = 0x3FU << FROM_SHIFT;
static const int TO_SHIFT = 6;
static const U32 TO_MASK = 0x3FU << TO_SHIFT;
static const int CAPTURE_SHIFT = 12;
static const U32 CAPTURE_MASK = 0xFU << CAPTURE_SHIFT;

// FLAGS
static const int FLAGS_SHIFT = 16;
static const U32 FLAGS_MASK = 0xFFU << FLAGS_SHIFT;
static const U32 NO_FLAGS = 0x0U;
// Promotions
// to create promotion just put piece to promote to in flags bits 0-3
static const U32 PROMOTE_TO_MASK = 0xFU << FLAGS_SHIFT;
// fourth bit denotes double pawn push
static const U32 PAWN_DOUBLE_PUSH = 16U << FLAGS_SHIFT;
// fifth bit denote EP capture
static const U32 EP_CAPTURE = 32U << FLAGS_SHIFT;
// sixth and seventh bit denotes castle
static const U32 KING_CASTLE = 64U << FLAGS_SHIFT;
static const U32 QUEEN_CASTLE = 128U << FLAGS_SHIFT;
static const U32 CASTLE_MASK = KING_CASTLE | QUEEN_CASTLE;

static const int SCORE_SHIFT = 24;
static const U32 SCORE_MASK = 0xFFU << SCORE_SHIFT;

Move_t inline build_move(U8 from, U8 to) { return static_cast<U32>(from | to << TO_SHIFT); }
Move_t inline build_capture(U8 from, U8 to, U8 capture) { return static_cast<U32>(from | (to << TO_SHIFT) | (capture << CAPTURE_SHIFT)); }
Move_t inline build_ep_capture(U8 from, U8 to, U8 capture) { return static_cast<U32>(from | (to << TO_SHIFT) | (capture << CAPTURE_SHIFT)) | EP_CAPTURE; }
Move_t inline build_promotion(U8 from, U8 to, U8 promote_to) { return static_cast<U32>(from | (to << TO_SHIFT) | (promote_to << FLAGS_SHIFT)); }
Move_t inline build_capture_promotion(U8 from, U8 to, U8 capture, U8 promote_to) { return static_cast<U32>(from | (to << TO_SHIFT) | (capture << CAPTURE_SHIFT) | (promote_to << FLAGS_SHIFT)); }
Move_t inline build_pawn_double_push(U8 from, U8 to) { return static_cast<U32>(from | (to << TO_SHIFT)) | PAWN_DOUBLE_PUSH; }
Move_t inline build_castle(U32 flags) { return flags; }

Move_t inline build_move_all(U8 from, U8 to, U8 capture, U32 flags) { return static_cast<U32>(from | (to << TO_SHIFT) | (capture << CAPTURE_SHIFT)) | flags; }
void inline move_reset_score(Move_t *move) { *move = *move & ~SCORE_MASK; }
void inline move_set_score(Move_t *move, U8 score)
{
    move_reset_score(move);
    *move = *move | static_cast<U32>(score << SCORE_SHIFT);
}

bool inline is_capture(Move_t move) { return move & CAPTURE_MASK; }
bool inline is_promotion(Move_t move) { return move & PROMOTE_TO_MASK; }
bool inline is_ep_capture(Move_t move) { return move & EP_CAPTURE; }
bool inline is_castle(Move_t move) { return move & CASTLE_MASK; }
bool inline is_pawn_double_push(Move_t move) { return move & PAWN_DOUBLE_PUSH; }

U8 inline move_from(Move_t move) { return move & FROM_MASK; }
U8 inline move_to(Move_t move) { return (move & TO_MASK) >> TO_SHIFT; }
U8 inline move_captured(Move_t move) { return (move & CAPTURE_MASK) >> CAPTURE_SHIFT; }
U8 inline move_flags(Move_t move) { return (move & FLAGS_MASK) >> FLAGS_SHIFT; }
U8 inline move_promote_to(Move_t move){ return (move & PROMOTE_TO_MASK) >> FLAGS_SHIFT; }
U8 inline move_score(Move_t move) { return static_cast<U8>((move & SCORE_MASK) >> SCORE_SHIFT); }

#endif /* MOVES_H */
