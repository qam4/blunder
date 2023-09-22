/*
 * File:   MoveGeneratorHyperbola.cpp
 *
 */

#include "MoveGenerator.h"

#include "LookupTables.h"

// https://www.chessprogramming.org/Efficient_Generation_of_Sliding_Piece_Attacks
// Hyperbola quiescence: https://www.chessprogramming.org/Hyperbola_Quintessence
U64 MoveGenerator::diag_attacks_hyperbola(U64 occ, int sq)
{
    // lineAttacks = (o-2s) ^ (o'-2s')'
    //     with m=mask
    // lineAttacks=(((o&m)-2s) ^ ((o&m)'-2s')')&m
    U64 forward, reverse, slider, lineMask;

    lineMask = diag_mask_ex(sq);  // excludes square of slider
    slider = C64(1) << sq;        // singleBitboard[sqOfSlider]; // single bit 1 << sq, 2^sq

    forward = occ & lineMask;     // also performs the first subtraction by clearing the s in o
    reverse = byteswap(forward);  // o'-s'
    forward -= (slider);          // o -2s
    reverse -= byteswap(slider);  // o'-2s'
    forward ^= byteswap(reverse);
    forward &= lineMask;  // mask the line again
    return forward;
}

U64 MoveGenerator::anti_diag_attacks_hyperbola(U64 occ, int sq)
{
    // lineAttacks= (o-2s) ^ (o'-2s')'
    //     with m=mask
    // lineAttacks=(((o&m)-2s) ^ ((o&m)'-2s')')&m
    U64 forward, reverse, slider, lineMask;

    lineMask = anti_diag_mask_ex(sq);  // excludes square of slider
    slider = C64(1) << sq;             // singleBitboard[sqOfSlider]; // single bit 1 << sq, 2^sq

    forward = occ & lineMask;     // also performs the first subtraction by clearing the s in o
    reverse = byteswap(forward);  // o'-s'
    forward -= (slider);          // o -2s
    reverse -= byteswap(slider);  // o'-2s'
    forward ^= byteswap(reverse);
    forward &= lineMask;  // mask the line again
    return forward;
}

U64 MoveGenerator::file_attacks_hyperbola(U64 occ, int sq)
{
    // lineAttacks= (o-2s) ^ (o'-2s')'
    //     with m=mask
    // lineAttacks=(((o&m)-2s) ^ ((o&m)'-2s')')&m
    U64 forward, reverse, slider, lineMask;

    lineMask = file_mask_ex(sq);  // excludes square of slider
    slider = C64(1) << sq;        // singleBitboard[sqOfSlider]; // single bit 1 << sq, 2^sq

    forward = occ & lineMask;     // also performs the first subtraction by clearing the s in o
    reverse = byteswap(forward);  // o'-s'
    forward -= (slider);          // o -2s
    reverse -= byteswap(slider);  // o'-2s'
    forward ^= byteswap(reverse);
    forward &= lineMask;  // mask the line again
    return forward;
}

U64 MoveGenerator::rank_attacks_hyperbola(U64 occ, int sq)
{
    // lineAttacks= (o-2s) ^ (o'-2s')'
    //     with m=mask
    // lineAttacks=(((o&m)-2s) ^ ((o&m)'-2s')')&m
    U64 forward, reverse, slider, lineMask;

    lineMask = rank_mask_ex(sq);  // excludes square of slider
    slider = C64(1) << sq;        // singleBitboard[sqOfSlider]; // single bit 1 << sq, 2^sq

    forward = occ & lineMask;  // also performs the first subtraction by clearing the s in o
    reverse = mirror_horizontal(forward);  // o'-s'
    forward -= (slider);                   // o -2s
    reverse -= mirror_horizontal(slider);  // o'-2s'
    forward ^= mirror_horizontal(reverse);
    forward &= lineMask;  // mask the line again
    return forward;
}

/*
U64 MoveGenerator::rook_attacks_hyperbola(U64 occ, int sq)
{
    return file_attacks(occ, sq) + rank_attacks(occ, sq);
}

U64 MoveGenerator::bishop_attacks_hyperbola(U64 occ, int sq)
{
    return diag_attacks(occ, sq) + anti_diag_attacks(occ, sq);
}
*/
