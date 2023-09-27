/*
 * File:   MoveGeneratorMagicBitboards.cpp
 *
 */

#include "MoveGenerator.h"

#include <string.h>

#include "LookupTables.h"

// https://www.chessprogramming.org/Magic_Bitboards
// https://rhysre.net/fast-chess-move-generation-with-magic-bitboards.html

#define ROOK_MB_TABLE_SIZE 4096
#define BISHOP_MB_TABLE_SIZE 1024

U64 ROOK_MB_TABLE[NUM_SQUARES][ROOK_MB_TABLE_SIZE] = { { 0 } };
U64 BISHOP_MB_TABLE[NUM_SQUARES][BISHOP_MB_TABLE_SIZE] = { { 0 } };

static U64 get_blockers_from_index(int index, U64 mask)
{
    U64 blockers = 0ULL;
    int bits = pop_count(mask);
    for (int i = 0; i < bits; i++)
    {
        int bit_pos = bit_scan_forward(mask);
        mask &= mask - 1;
        if (index & (1 << i))
        {
            blockers |= (1ULL << bit_pos);
        }
    }
    return blockers;
}

void MoveGenerator::init_rook_magic_table()
{
    memset(ROOK_MB_TABLE, 0, sizeof(ROOK_MB_TABLE));
    // For all squares
    for (int square = 0; square < NUM_SQUARES; square++)
    {
        // For all possible blockers for this square
        for (int blocker_index = 0; blocker_index < (1 << ROOK_INDEX_BITS[square]); blocker_index++)
        {
            U64 blockers = get_blockers_from_index(blocker_index, ROOK_MB_MASK[square]);
            U64 index = (blockers * ROOK_MAGICS[square]) >> (64 - ROOK_INDEX_BITS[square]);
#ifndef NDEBUG
            assert(index < ROOK_MB_TABLE_SIZE);
            assert(ROOK_MB_TABLE[square][index] == 0);
#endif
            ROOK_MB_TABLE[square][index] = rook_attacks_slow(blockers, square);
        }
    }
}

void MoveGenerator::init_bishop_magic_table()
{
    memset(BISHOP_MB_TABLE, 0, sizeof(BISHOP_MB_TABLE));
    // For all squares
    for (int square = 0; square < 64; square++)
    {
        // For all possible blockers for this square
        for (int blocker_index = 0; blocker_index < (1 << BISHOP_INDEX_BITS[square]);
             blocker_index++)
        {
            U64 blockers = get_blockers_from_index(blocker_index, BISHOP_MB_MASK[square]);
            U64 index = (blockers * BISHOP_MAGICS[square]) >> (64 - BISHOP_INDEX_BITS[square]);
#ifndef NDEBUG
            assert(index < BISHOP_MB_TABLE_SIZE);
            assert(BISHOP_MB_TABLE[square][index] == 0);
#endif
            BISHOP_MB_TABLE[square][index] = bishop_attacks_slow(blockers, square);
        }
    }
}

void MoveGenerator::init_magic_tables()
{
    init_rook_magic_table();
    init_bishop_magic_table();
}

U64 MoveGenerator::rook_attacks_magic(U64 occ, int sq)
{
    occ &= ROOK_MB_MASK[sq];
    return ROOK_MB_TABLE[sq][(occ * ROOK_MAGICS[sq]) >> (64 - ROOK_INDEX_BITS[sq])];
}

U64 MoveGenerator::bishop_attacks_magic(U64 occ, int sq)
{
    occ &= BISHOP_MB_MASK[sq];
    return BISHOP_MB_TABLE[sq][(occ * BISHOP_MAGICS[sq]) >> (64 - BISHOP_INDEX_BITS[sq])];
}
