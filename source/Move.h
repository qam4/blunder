/*
 * File:   Moves.h
 *
 */

#ifndef MOVES_H
#define MOVES_H

#include "Types.h"

typedef U32 Move_t;

// FLAGS
static const U8 NO_FLAGS = 0;
// Promotions
// to create promotion just put piece to promote to in flags bits 0-3
// fourth bit denotes double pawn push
static const U8 PAWN_DOUBLE_PUSH = 16;
// fifth bit denote EP capture
static const U8 EP_CAPTURE = 32;
// sixth and seventh bit denotes castle
static const U8 KING_CASTLE = 64;
static const U8 QUEEN_CASTLE = 128;
static const U8 CASTLE_MASK = KING_CASTLE | QUEEN_CASTLE;

Move_t inline build_move(U8 from, U8 to) { return (from & 0x3FU) | (to & 0x3FU) << 6; }
Move_t inline build_capture(U8 from, U8 to, U8 capture) { return (from & 0x3FU) | (to & 0x3FU) << 6 | (capture & 0xFU) << 12; }
Move_t inline build_ep_capture(U8 from, U8 to, U8 capture) { return (from & 0x3FU) | (to & 0x3FU) << 6 | (capture & 0xFU) << 12 | EP_CAPTURE << 16; }
Move_t inline build_promotion(U8 from, U8 to, U8 promote_to) { return (from & 0x3FU) | (to & 0x3FU) << 6 | ((promote_to & 0xFU) << 16); }
Move_t inline build_capture_promotion(U8 from, U8 to, U8 capture, U8 promote_to){ return (from & 0x3FU) | (to & 0x3FU) << 6 | (capture & 0xFU) << 12 | ((promote_to & 0xFU) << 16); }
Move_t inline build_pawn_double_push(U8 from, U8 to) { return (from & 0x3FU) | (to & 0x3FU) << 6 | PAWN_DOUBLE_PUSH << 16; }
Move_t inline build_castle(U8 flags) { return static_cast<U32>(flags & CASTLE_MASK) << 16; }


Move_t inline build_move_flags(U8 from, U8 to, U8 flags) { return (from & 0x3FU) | (to & 0x3FU) << 6 | (flags & 0xFFU) << 16; }
Move_t inline build_move_all(U8 from, U8 to, U8 flags, U8 capture) { return (from & 0x3FU) | (to & 0x3FU) << 6 | (capture & 0xFU) << 12 | (flags & 0xFFU) << 16; }
void inline move_add_score(Move_t *move, U8 score) { *move = *move | (score & 0xFFU) << 24; }

bool inline is_capture(Move_t move) { return (move >> 12) & 0xFU; }
bool inline is_promotion(Move_t move){ return (move >> 16) & (0xFU); } // mask all pieces
bool inline is_ep_capture(Move_t move){ return (move >> 16) & EP_CAPTURE; }
bool inline is_castle(Move_t move){ return (move >> 16) & CASTLE_MASK; }
bool inline is_pawn_double_push(Move_t move){ return (move >> 16) & PAWN_DOUBLE_PUSH; }

U8 inline move_from(Move_t move) { return move & 0x3FU; }
U8 inline move_to(Move_t move) { return (move >> 6) & 0x3FU; }
U8 inline move_captured(Move_t move) { return (move >> 12) & 0xFU; }
U8 inline move_flags(Move_t move) { return (move >> 16) & 0xFFU; }
U8 inline move_promote_to(Move_t move){ return (move >> 16) & 0xFU; }
U8 inline move_score(Move_t move) { return static_cast<U8>(move >> 24) & 0xFFU; }

#endif /* MOVES_H */
