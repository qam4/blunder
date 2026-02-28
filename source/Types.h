/*
 * File:   Types.h
 *
 */

#ifndef TYPES_H
#define TYPES_H

#include <cstdint>

using U64 = uint64_t;  // used for bitboards
using U8 = uint8_t;    // used for pieces
using U16 = uint16_t;  // used for move scores
using U32 = uint32_t;  // used for moves
#define C64(constantU64) constantU64##ULL

#endif /* TYPES_H */
