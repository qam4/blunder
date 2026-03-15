/*
 * File:   Evaluator.cpp
 *
 * HandCraftedEvaluator implementation.
 * Stockfish-style tapered evaluation with dual-phase PSQT.
 */

#include <algorithm>

#include "Evaluator.h"

#include "Board.h"
#include "MoveGenerator.h"
#include "Zobrist.h"

// Phase constants
static constexpr int MIDGAME_LIMIT = 15258;
static constexpr int ENDGAME_LIMIT = 3915;
static constexpr int PHASE_MAX = 128;
static constexpr int MG = 0;
static constexpr int EG = 1;
static constexpr int NUM_PHASES = 2;

// File bitboard masks
static constexpr U64 FILE_BB[8] = {
    0x0101010101010101ULL << 0, 0x0101010101010101ULL << 1, 0x0101010101010101ULL << 2,
    0x0101010101010101ULL << 3, 0x0101010101010101ULL << 4, 0x0101010101010101ULL << 5,
    0x0101010101010101ULL << 6, 0x0101010101010101ULL << 7,
};

// Adjacent file masks (files neighboring file f)
static constexpr U64 adjacent_files(int f)
{
    U64 adj = 0;
    if (f > 0)
        adj |= 0x0101010101010101ULL << (f - 1);
    if (f < 7)
        adj |= 0x0101010101010101ULL << (f + 1);
    return adj;
}

// Pawn structure evaluation weights (centipawns)
// Passed pawn bonus by rank distance from start (index 0 = rank 2 for white, rank 7 for black)
// Ranks 0-5 map to distances 1-6 from start rank
static constexpr int PASSED_PAWN_MG[6] = { 10, 22, 34, 46, 58, 70 };
static constexpr int PASSED_PAWN_EG[6] = { 15, 36, 57, 78, 99, 120 };

static constexpr int ISOLATED_PAWN_MG = -10;
static constexpr int ISOLATED_PAWN_EG = -20;

static constexpr int DOUBLED_PAWN_MG = -10;
static constexpr int DOUBLED_PAWN_EG = -10;

static constexpr int BACKWARD_PAWN_MG = -8;
static constexpr int BACKWARD_PAWN_EG = -10;

// Connected pawn bonus by rank distance from start (index 0-5)
static constexpr int CONNECTED_PAWN_MG[6] = { 5, 7, 9, 11, 13, 15 };
static constexpr int CONNECTED_PAWN_EG[6] = { 5, 6, 7, 8, 9, 10 };

// King safety weights (MG-only)
static constexpr int KING_PAWN_SHIELD_MG = -15;
static constexpr int KING_OPEN_FILE_MG = -20;
static constexpr int KING_ZONE_ATTACK_MG = -8;
static constexpr int KING_SAFETY_PHASE_THRESHOLD = 40;

// Material values: PIECE_VALUE_BONUS[phase][piece_type >> 1]
// Index: 0=empty, 1=Pawn, 2=Knight, 3=Bishop, 4=Rook, 5=Queen, 6=King
static const int PIECE_VALUE_BONUS[NUM_PHASES][NUM_PIECES / 2] = {
    // MG: empty, pawn, knight, bishop, rook, queen, king
    { 0, 124, 781, 825, 1276, 2538, 0 },
    // EG
    { 0, 206, 854, 915, 1380, 2682, 0 }
};

// clang-format off

// Pawn PSQT: full 64-square layout per phase
static const int PIECE_SQUARE_BONUS_PAWN[NUM_PHASES][NUM_SQUARES] =
{
    {
        0, 0, 0, 0, 0, 0, 0, 0,
        3, 3, 10, 19, 16, 19, 7, -5,
        -9, -15, 11, 15, 32, 22, 5, -22,
        -4, -23, 6, 20, 40, 17, 4, -8,
        13, 0, -13, 1, 11, -2, -13, 5,
        5, -12, -7, 22, -8, -5, -15, -8,
        -7, 7, -3, -13, 5, -16, 10, -8,
        0, 0, 0, 0, 0, 0, 0, 0
    },
    {
        0, 0, 0, 0, 0, 0, 0, 0,
        -10, -6, 10, 0, 14, 7, -5, -19,
        -10, -10, -10, 4, 4, 3, -6, -4,
        6, -2, -8, -4, -13, -12, -10, -9,
        10, 5, 4, -5, -5, -5, 14, 9,
        28, 20, 21, 28, 30, 7, 6, 13,
        0, -11, 12, 21, 25, 19, 4, 7,
        0, 0, 0, 0, 0, 0, 0, 0
    }
};

// Compressed file-symmetric PSQT: PIECE_SQUARE_BONUS[phase][piece_index][rank][file_half]
// piece_index: 0=Knight, 1=Bishop, 2=Rook, 3=Queen, 4=King
static const int PIECE_SQUARE_BONUS[NUM_PHASES][5][8][4] = {
{
    {{-175, -92, -74, -73}, {-77, -41, -27, -15}, {-61, -17, 6, 12}, {-35, 8, 40, 49}, {-34, 13, 44, 51}, {-9, 22, 58, 53}, {-67, -27, 4, 37}, {-201, -83, -56, -26}},
    {{-53, -5, -8, -23}, {-15, 8, 19, 4}, {-7, 21, -5, 17}, {-5, 11, 25, 39}, {-12, 29, 22, 31}, {-16, 6, 1, 11}, {-17, -14, 5, 0}, {-48, 1, -14, -23}},
    {{-31, -20, -14, -5}, {-21, -13, -8, 6}, {-25, -11, -1, 3}, {-13, -5, -4, -6}, {-27, -15, -4, 3}, {-22, -2, 6, 12}, {-2, 12, 16, 18}, {-17, -19, -1, 9}},
    {{3, -5, -5, 4}, {-3, 5, 8, 12}, {-3, 6, 13, 7}, {4, 5, 9, 8}, {0, 14, 12, 5}, {-4, 10, 6, 8}, {-5, 6, 10, 8}, {-2, -2, 1, -2}},
    {{271, 327, 271, 198}, {278, 303, 234, 179}, {195, 258, 169, 120}, {164, 190, 138, 98}, {154, 179, 105, 70}, {123, 145, 81, 31}, {88, 120, 65, 33}, {59, 89, 45, -1}}
},
{
    {{-96, -65, -49, -21}, {-67, -54, -18, 8}, {-40, -27, -8, 29}, {-35, -2, 13, 28}, {-45, -16, 9, 39}, {-51, -44, -16, 17}, {-69, -50, -51, 12}, {-100, -88, -56, -17}},
    {{-57, -30, -37, -12}, {-37, -13, -17, 1}, {-16, -1, -2, 10}, {-20, -6, 0, 17}, {-17, -1, -14, 15}, {-30, 6, 4, 6}, {-31, -20, -1, 1}, {-46, -42, -37, -24}},
    {{-9, -13, -10, -9}, {-12, -9, -1, -2}, {6, -8, -2, -6}, {-6, 1, -9, 7}, {-5, 8, 7, -6}, {6, 1, -7, 10}, {4, 5, 20, -5}, {18, 0, 19, 13}},
    {{-69, -57, -47, -26}, {-55, -31, -22, -4}, {-39, -18, -9, 3}, {-23, -3, 13, 24}, {-29, -6, 9, 21}, {-38, -18, -12, 1}, {-50, -27, -24, -8}, {-75, -52, -43, -36}},
    {{1, 45, 85, 76}, {53, 100, 133, 135}, {88, 130, 169, 175}, {103, 156, 172, 172}, {96, 166, 199, 199}, {92, 172, 184, 191}, {47, 121, 116, 131}, {11, 59, 73, 78}}
}
};

// clang-format on

HandCraftedEvaluator::HandCraftedEvaluator()
    : piece_square_table_ {}
    , config_ {}
{
    psqt_init();
}

void HandCraftedEvaluator::psqt_init()
{
    for (int phase = 0; phase < NUM_PHASES; phase++)
    {
        for (int piece = 0; piece < NUM_PIECES; piece += 2)
        {
            for (int square = 0; square < NUM_SQUARES; square++)
            {
                int bonus = 0;
                if (piece == 0)
                {
                    // Empty square — leave as 0
                }
                else if (piece == PAWN)
                {
                    bonus = PIECE_SQUARE_BONUS_PAWN[phase][square];
                }
                else
                {
                    int file = std::min(square % 8, 7 - (square % 8));  // file symmetric
                    bonus = PIECE_SQUARE_BONUS[phase][(piece >> 1) - 2][square / 8][file];
                }

                int material = PIECE_VALUE_BONUS[phase][piece >> 1];
                piece_square_table_[phase][piece][square] = material + bonus;

                // Black piece: flip rank
                piece_square_table_[phase][piece | BLACK][square ^ 56] = material + bonus;
            }
        }
    }
}

int HandCraftedEvaluator::phase(const Board& board) const
{
    int npm = PIECE_VALUE_BONUS[MG][QUEEN >> 1] * pop_count(board.bitboard(WHITE_QUEEN))
        + PIECE_VALUE_BONUS[MG][ROOK >> 1] * pop_count(board.bitboard(WHITE_ROOK))
        + PIECE_VALUE_BONUS[MG][BISHOP >> 1] * pop_count(board.bitboard(WHITE_BISHOP))
        + PIECE_VALUE_BONUS[MG][KNIGHT >> 1] * pop_count(board.bitboard(WHITE_KNIGHT))
        + PIECE_VALUE_BONUS[MG][QUEEN >> 1] * pop_count(board.bitboard(BLACK_QUEEN))
        + PIECE_VALUE_BONUS[MG][ROOK >> 1] * pop_count(board.bitboard(BLACK_ROOK))
        + PIECE_VALUE_BONUS[MG][BISHOP >> 1] * pop_count(board.bitboard(BLACK_BISHOP))
        + PIECE_VALUE_BONUS[MG][KNIGHT >> 1] * pop_count(board.bitboard(BLACK_KNIGHT));

    npm = std::max(ENDGAME_LIMIT, std::min(npm, MIDGAME_LIMIT));
    return ((npm - ENDGAME_LIMIT) * PHASE_MAX) / (MIDGAME_LIMIT - ENDGAME_LIMIT);
}

int HandCraftedEvaluator::eval_pawn_structure(const Board& board, int p)
{
    // Compute pawn-only Zobrist hash
    U64 pawn_hash = 0;
    U64 wp = board.bitboard(WHITE_PAWN);
    U64 wp_iter = wp;
    while (wp_iter)
    {
        int sq = bit_scan_forward(wp_iter);
        pawn_hash ^= Zobrist::get_pieces(WHITE_PAWN, sq);
        wp_iter &= wp_iter - 1;
    }
    U64 bp = board.bitboard(BLACK_PAWN);
    U64 bp_iter = bp;
    while (bp_iter)
    {
        int sq = bit_scan_forward(bp_iter);
        pawn_hash ^= Zobrist::get_pieces(BLACK_PAWN, sq);
        bp_iter &= bp_iter - 1;
    }

    // Probe pawn hash table
    int index = static_cast<int>(pawn_hash % PAWN_HASH_SIZE);
    PawnHashEntry& entry = pawn_hash_[index];
    int mg_score;
    int eg_score;

    if (entry.key == pawn_hash && pawn_hash != 0)
    {
        mg_score = entry.mg_score;
        eg_score = entry.eg_score;
    }
    else
    {
        mg_score = 0;
        eg_score = 0;

        // Evaluate for both sides
        for (int side = 0; side <= 1; side++)
        {
            int sign = (side == WHITE) ? 1 : -1;
            U64 friendly_pawns = (side == WHITE) ? wp : bp;
            U64 enemy_pawns = (side == WHITE) ? bp : wp;
            U64 iter = friendly_pawns;

            while (iter)
            {
                int sq = bit_scan_forward(iter);
                iter &= iter - 1;

                int rank = sq / 8;
                int file = sq % 8;

                // Rank distance from start: white starts rank 1, black starts rank 6
                int rank_dist = (side == WHITE) ? (rank - 1) : (6 - rank);
                // Clamp to valid index [0, 5]
                rank_dist = std::max(0, std::min(5, rank_dist));

                U64 adj = adjacent_files(file);
                U64 file_bb = FILE_BB[file];

                // --- Passed pawn ---
                // No enemy pawns on same file or adjacent files ahead
                U64 front_mask;
                if (side == WHITE)
                {
                    // Squares ahead of this pawn (higher ranks)
                    front_mask = 0;
                    for (int r = rank + 1; r <= 7; r++)
                        front_mask |= (0xFFULL << (r * 8));
                }
                else
                {
                    // Squares ahead of this pawn (lower ranks)
                    front_mask = 0;
                    for (int r = 0; r < rank; r++)
                        front_mask |= (0xFFULL << (r * 8));
                }
                U64 blocking_files = file_bb | adj;
                if ((enemy_pawns & blocking_files & front_mask) == 0)
                {
                    mg_score += sign * PASSED_PAWN_MG[rank_dist];
                    eg_score += sign * PASSED_PAWN_EG[rank_dist];
                }

                // --- Isolated pawn ---
                if ((friendly_pawns & adj) == 0)
                {
                    mg_score += sign * ISOLATED_PAWN_MG;
                    eg_score += sign * ISOLATED_PAWN_EG;
                }

                // --- Backward pawn ---
                // A pawn is backward if:
                // 1. No friendly pawn behind on adjacent files can support it
                // 2. The stop square is controlled by an enemy pawn
                if ((friendly_pawns & adj) != 0)
                {
                    // Only check backward if not isolated (isolated already penalized)
                    U64 behind_mask;
                    if (side == WHITE)
                    {
                        behind_mask = 0;
                        for (int r = 0; r < rank; r++)
                            behind_mask |= (0xFFULL << (r * 8));
                    }
                    else
                    {
                        behind_mask = 0;
                        for (int r = rank + 1; r <= 7; r++)
                            behind_mask |= (0xFFULL << (r * 8));
                    }
                    bool no_support_behind = ((friendly_pawns & adj & behind_mask) == 0);

                    if (no_support_behind)
                    {
                        // Check if stop square is controlled by enemy pawn
                        int stop_sq = (side == WHITE) ? sq + 8 : sq - 8;
                        if (stop_sq >= 0 && stop_sq < 64)
                        {
                            // Enemy pawn attacks on the stop square
                            U64 stop_bb = 1ULL << stop_sq;
                            U64 enemy_pawn_attacks;
                            if (side == WHITE)
                            {
                                // Black pawns attack diagonally down
                                enemy_pawn_attacks = ((enemy_pawns >> 7) & ~FILE_BB[0])
                                    | ((enemy_pawns >> 9) & ~FILE_BB[7]);
                            }
                            else
                            {
                                // White pawns attack diagonally up
                                enemy_pawn_attacks = ((enemy_pawns << 7) & ~FILE_BB[7])
                                    | ((enemy_pawns << 9) & ~FILE_BB[0]);
                            }
                            if (stop_bb & enemy_pawn_attacks)
                            {
                                mg_score += sign * BACKWARD_PAWN_MG;
                                eg_score += sign * BACKWARD_PAWN_EG;
                            }
                        }
                    }
                }

                // --- Connected pawn ---
                // Defended by or adjacent to a friendly pawn
                U64 pawn_bb = 1ULL << sq;
                U64 friendly_pawn_attacks;
                if (side == WHITE)
                {
                    friendly_pawn_attacks = ((friendly_pawns << 7) & ~FILE_BB[7])
                        | ((friendly_pawns << 9) & ~FILE_BB[0]);
                }
                else
                {
                    friendly_pawn_attacks = ((friendly_pawns >> 7) & ~FILE_BB[0])
                        | ((friendly_pawns >> 9) & ~FILE_BB[7]);
                }
                // Adjacent: friendly pawn on same rank, adjacent file
                U64 same_rank = 0xFFULL << (rank * 8);
                bool is_defended = (pawn_bb & friendly_pawn_attacks) != 0;
                bool is_adjacent = (friendly_pawns & adj & same_rank) != 0;
                if (is_defended || is_adjacent)
                {
                    mg_score += sign * CONNECTED_PAWN_MG[rank_dist];
                    eg_score += sign * CONNECTED_PAWN_EG[rank_dist];
                }
            }

            // --- Doubled pawns ---
            for (int f = 0; f < 8; f++)
            {
                int count = pop_count(friendly_pawns & FILE_BB[f]);
                if (count > 1)
                {
                    // Penalty for each extra pawn on the file
                    mg_score += sign * DOUBLED_PAWN_MG * (count - 1);
                    eg_score += sign * DOUBLED_PAWN_EG * (count - 1);
                }
            }
        }

        // Store in pawn hash table
        entry.key = pawn_hash;
        entry.mg_score = mg_score;
        entry.eg_score = eg_score;
    }

    // Taper the score
    int tapered = (mg_score * p + eg_score * (PHASE_MAX - p)) / PHASE_MAX;
    last_pawn_structure_ = tapered;
    return tapered;
}

int HandCraftedEvaluator::eval_king_safety(const Board& board, int p)
{
    // King safety is MG-only; skip in endgame positions
    if (p <= KING_SAFETY_PHASE_THRESHOLD)
    {
        last_king_safety_ = 0;
        return 0;
    }

    int mg_score = 0;
    U64 all_pawns_w = board.bitboard(WHITE_PAWN);
    U64 all_pawns_b = board.bitboard(BLACK_PAWN);
    U64 occupied = 0;
    for (int i = 0; i < NUM_PIECES; i++)
        occupied |= board.bitboard(i);

    for (int side = 0; side <= 1; side++)
    {
        int sign = (side == WHITE) ? 1 : -1;
        int enemy = 1 - side;

        // Find king square
        U64 king_bb = board.bitboard(KING | side);
        if (king_bb == 0)
            continue;
        int king_sq = bit_scan_forward(king_bb);
        int king_file = king_sq % 8;
        int king_rank = king_sq / 8;

        U64 friendly_pawns = (side == WHITE) ? all_pawns_w : all_pawns_b;
        U64 enemy_pawns = (side == WHITE) ? all_pawns_b : all_pawns_w;

        // --- Pawn shield ---
        // Check king's file and adjacent files, 1-2 ranks ahead of king
        for (int f = std::max(0, king_file - 1); f <= std::min(7, king_file + 1); f++)
        {
            U64 file_bb = FILE_BB[f];
            U64 shield_mask = 0;
            if (side == WHITE)
            {
                // Ranks ahead = king_rank + 1 and king_rank + 2
                for (int r = king_rank + 1; r <= std::min(7, king_rank + 2); r++)
                    shield_mask |= (1ULL << (r * 8 + f));
            }
            else
            {
                // Ranks ahead = king_rank - 1 and king_rank - 2
                for (int r = king_rank - 1; r >= std::max(0, king_rank - 2); r--)
                    shield_mask |= (1ULL << (r * 8 + f));
            }

            // If no friendly pawn in the shield zone on this file, penalize
            if ((friendly_pawns & shield_mask) == 0)
            {
                mg_score += sign * KING_PAWN_SHIELD_MG;
            }
        }

        // --- Open/semi-open files near king ---
        for (int f = std::max(0, king_file - 1); f <= std::min(7, king_file + 1); f++)
        {
            U64 file_bb = FILE_BB[f];
            bool has_friendly_pawn = (friendly_pawns & file_bb) != 0;
            bool has_enemy_pawn = (enemy_pawns & file_bb) != 0;

            // Open file: no pawns at all; semi-open: no friendly pawns
            if (!has_friendly_pawn)
            {
                mg_score += sign * KING_OPEN_FILE_MG;
            }
        }

        // --- King zone attacks ---
        // King zone = king square + 8 neighbors
        U64 king_zone = KING_LOOKUP_TABLE[king_sq] | (1ULL << king_sq);

        // Count enemy piece attacks on king zone
        // Enemy knights
        U64 enemy_knights = board.bitboard(KNIGHT | enemy);
        while (enemy_knights)
        {
            int sq = bit_scan_forward(enemy_knights);
            enemy_knights &= enemy_knights - 1;
            U64 attacks = MoveGenerator::knight_targets(1ULL << sq);
            if (attacks & king_zone)
                mg_score += sign * KING_ZONE_ATTACK_MG;
        }

        // Enemy bishops
        U64 enemy_bishops = board.bitboard(BISHOP | enemy);
        while (enemy_bishops)
        {
            int sq = bit_scan_forward(enemy_bishops);
            enemy_bishops &= enemy_bishops - 1;
            U64 attacks = MoveGenerator::bishop_attacks(occupied, sq);
            if (attacks & king_zone)
                mg_score += sign * KING_ZONE_ATTACK_MG;
        }

        // Enemy rooks
        U64 enemy_rooks = board.bitboard(ROOK | enemy);
        while (enemy_rooks)
        {
            int sq = bit_scan_forward(enemy_rooks);
            enemy_rooks &= enemy_rooks - 1;
            U64 attacks = MoveGenerator::rook_attacks(occupied, sq);
            if (attacks & king_zone)
                mg_score += sign * KING_ZONE_ATTACK_MG;
        }

        // Enemy queens (rook + bishop attacks)
        U64 enemy_queens = board.bitboard(QUEEN | enemy);
        while (enemy_queens)
        {
            int sq = bit_scan_forward(enemy_queens);
            enemy_queens &= enemy_queens - 1;
            U64 attacks = MoveGenerator::rook_attacks(occupied, sq)
                | MoveGenerator::bishop_attacks(occupied, sq);
            if (attacks & king_zone)
                mg_score += sign * KING_ZONE_ATTACK_MG;
        }
    }

    // King safety is MG-only (EG weight = 0), tapered by phase
    int tapered = (mg_score * p) / PHASE_MAX;
    last_king_safety_ = tapered;
    return tapered;
}

// Mobility weights per piece type [MG, EG]
static constexpr int MOBILITY_KNIGHT_MG = 4;
static constexpr int MOBILITY_KNIGHT_EG = 4;
static constexpr int MOBILITY_BISHOP_MG = 4;
static constexpr int MOBILITY_BISHOP_EG = 4;
static constexpr int MOBILITY_ROOK_MG = 2;
static constexpr int MOBILITY_ROOK_EG = 3;
static constexpr int MOBILITY_QUEEN_MG = 1;
static constexpr int MOBILITY_QUEEN_EG = 2;

int HandCraftedEvaluator::eval_mobility(const Board& board, int p)
{
    int mg_score = 0;
    int eg_score = 0;

    // Compute occupied bitboard
    U64 occupied = 0;
    for (int i = 0; i < NUM_PIECES; i++)
        occupied |= board.bitboard(i);

    // Compute enemy pawn attack masks
    U64 wp = board.bitboard(WHITE_PAWN);
    U64 bp = board.bitboard(BLACK_PAWN);
    // White pawn attacks (used to penalize black mobility)
    U64 w_pawn_attacks = ((wp << 7) & ~FILE_BB[7]) | ((wp << 9) & ~FILE_BB[0]);
    // Black pawn attacks (used to penalize white mobility)
    U64 b_pawn_attacks = ((bp >> 7) & ~FILE_BB[0]) | ((bp >> 9) & ~FILE_BB[7]);

    for (int side = 0; side <= 1; side++)
    {
        int sign = (side == WHITE) ? 1 : -1;

        // Friendly pieces mask — exclude from mobility squares
        U64 friendly = 0;
        for (int pc = side; pc < NUM_PIECES; pc += 2)
            friendly |= board.bitboard(pc);

        // Enemy pawn control — exclude from mobility squares
        U64 enemy_pawn_ctrl = (side == WHITE) ? b_pawn_attacks : w_pawn_attacks;
        U64 safe = ~(friendly | enemy_pawn_ctrl);

        // Knights
        U64 knights = board.bitboard(KNIGHT | side);
        while (knights)
        {
            int sq = bit_scan_forward(knights);
            knights &= knights - 1;
            U64 attacks = MoveGenerator::knight_targets(1ULL << sq) & safe;
            int count = pop_count(attacks);
            mg_score += sign * count * MOBILITY_KNIGHT_MG;
            eg_score += sign * count * MOBILITY_KNIGHT_EG;
        }

        // Bishops
        U64 bishops = board.bitboard(BISHOP | side);
        while (bishops)
        {
            int sq = bit_scan_forward(bishops);
            bishops &= bishops - 1;
            U64 attacks = MoveGenerator::bishop_attacks(occupied, sq) & safe;
            int count = pop_count(attacks);
            mg_score += sign * count * MOBILITY_BISHOP_MG;
            eg_score += sign * count * MOBILITY_BISHOP_EG;
        }

        // Rooks
        U64 rooks = board.bitboard(ROOK | side);
        while (rooks)
        {
            int sq = bit_scan_forward(rooks);
            rooks &= rooks - 1;
            U64 attacks = MoveGenerator::rook_attacks(occupied, sq) & safe;
            int count = pop_count(attacks);
            mg_score += sign * count * MOBILITY_ROOK_MG;
            eg_score += sign * count * MOBILITY_ROOK_EG;
        }

        // Queens
        U64 queens = board.bitboard(QUEEN | side);
        while (queens)
        {
            int sq = bit_scan_forward(queens);
            queens &= queens - 1;
            U64 attacks = (MoveGenerator::rook_attacks(occupied, sq)
                           | MoveGenerator::bishop_attacks(occupied, sq))
                & safe;
            int count = pop_count(attacks);
            mg_score += sign * count * MOBILITY_QUEEN_MG;
            eg_score += sign * count * MOBILITY_QUEEN_EG;
        }
    }

    int tapered = (mg_score * p + eg_score * (PHASE_MAX - p)) / PHASE_MAX;
    last_mobility_ = tapered;
    return tapered;
}

// Piece bonus weights (centipawns)
static constexpr int BISHOP_PAIR_MG = 30;
static constexpr int BISHOP_PAIR_EG = 50;

static constexpr int ROOK_OPEN_FILE_MG = 20;
static constexpr int ROOK_OPEN_FILE_EG = 25;
static constexpr int ROOK_SEMI_OPEN_FILE_MG = 10;
static constexpr int ROOK_SEMI_OPEN_FILE_EG = 15;

static constexpr int SEVENTH_RANK_MG = 20;
static constexpr int SEVENTH_RANK_EG = 30;

// Rank masks for 7th rank detection
static constexpr U64 RANK_BB[8] = {
    0x00000000000000FFULL,  // rank 0
    0x000000000000FF00ULL,  // rank 1
    0x0000000000FF0000ULL,  // rank 2
    0x00000000FF000000ULL,  // rank 3
    0x000000FF00000000ULL,  // rank 4
    0x0000FF0000000000ULL,  // rank 5
    0x00FF000000000000ULL,  // rank 6
    0xFF00000000000000ULL,  // rank 7
};

int HandCraftedEvaluator::eval_piece_bonuses(const Board& board, int p)
{
    int mg_score = 0;
    int eg_score = 0;

    U64 wp = board.bitboard(WHITE_PAWN);
    U64 bp = board.bitboard(BLACK_PAWN);
    U64 all_pawns = wp | bp;

    for (int side = 0; side <= 1; side++)
    {
        int sign = (side == WHITE) ? 1 : -1;
        U64 friendly_pawns = (side == WHITE) ? wp : bp;
        U64 enemy_pawns = (side == WHITE) ? bp : wp;

        // --- Bishop pair ---
        if (pop_count(board.bitboard(BISHOP | side)) >= 2)
        {
            mg_score += sign * BISHOP_PAIR_MG;
            eg_score += sign * BISHOP_PAIR_EG;
        }

        // --- Rook file bonuses ---
        U64 rooks = board.bitboard(ROOK | side);
        while (rooks)
        {
            int sq = bit_scan_forward(rooks);
            rooks &= rooks - 1;
            int file = sq % 8;
            U64 file_bb = FILE_BB[file];

            bool has_friendly_pawn = (friendly_pawns & file_bb) != 0;
            bool has_enemy_pawn = (enemy_pawns & file_bb) != 0;

            if (!has_friendly_pawn && !has_enemy_pawn)
            {
                // Open file
                mg_score += sign * ROOK_OPEN_FILE_MG;
                eg_score += sign * ROOK_OPEN_FILE_EG;
            }
            else if (!has_friendly_pawn && has_enemy_pawn)
            {
                // Semi-open file
                mg_score += sign * ROOK_SEMI_OPEN_FILE_MG;
                eg_score += sign * ROOK_SEMI_OPEN_FILE_EG;
            }
        }

        // --- Rook/Queen on 7th rank ---
        // White: 7th rank = rank index 6, enemy king on rank 7
        // Black: 7th rank = rank index 1, enemy king on rank 0
        int seventh_rank = (side == WHITE) ? 6 : 1;
        int eighth_rank = (side == WHITE) ? 7 : 0;

        U64 enemy_king_bb = board.bitboard(KING | (1 - side));
        bool enemy_king_on_eighth = (enemy_king_bb & RANK_BB[eighth_rank]) != 0;

        if (enemy_king_on_eighth)
        {
            U64 seventh_rank_bb = RANK_BB[seventh_rank];

            // Rooks on 7th
            U64 rooks_on_7th = board.bitboard(ROOK | side) & seventh_rank_bb;
            int rook_count = pop_count(rooks_on_7th);
            mg_score += sign * rook_count * SEVENTH_RANK_MG;
            eg_score += sign * rook_count * SEVENTH_RANK_EG;

            // Queens on 7th
            U64 queens_on_7th = board.bitboard(QUEEN | side) & seventh_rank_bb;
            int queen_count = pop_count(queens_on_7th);
            mg_score += sign * queen_count * SEVENTH_RANK_MG;
            eg_score += sign * queen_count * SEVENTH_RANK_EG;
        }
    }

    int tapered = (mg_score * p + eg_score * (PHASE_MAX - p)) / PHASE_MAX;
    last_piece_bonuses_ = tapered;
    return tapered;
}

int HandCraftedEvaluator::evaluate(const Board& board)
{
    int mg = 0;
    int eg = 0;

    for (int square = 0; square < NUM_SQUARES; square++)
    {
        U8 piece = board[square];
        if (piece == EMPTY)
            continue;

        if (piece & BLACK)
        {
            mg -= piece_square_table_[MG][piece][square];
            eg -= piece_square_table_[EG][piece][square];
        }
        else
        {
            mg += piece_square_table_[MG][piece][square];
            eg += piece_square_table_[EG][piece][square];
        }
    }

    int p = phase(board);
    int score = (mg * p + eg * (PHASE_MAX - p)) / PHASE_MAX;

    if (config_.pawn_structure_enabled)
    {
        score += eval_pawn_structure(board, p);
    }
    else
    {
        last_pawn_structure_ = 0;
    }

    if (config_.king_safety_enabled)
    {
        score += eval_king_safety(board, p);
    }
    else
    {
        last_king_safety_ = 0;
    }

    if (config_.mobility_enabled)
    {
        score += eval_mobility(board, p);
    }
    else
    {
        last_mobility_ = 0;
    }

    if (config_.piece_bonuses_enabled)
    {
        score += eval_piece_bonuses(board, p);
    }
    else
    {
        last_piece_bonuses_ = 0;
    }

    if (config_.tempo_enabled)
    {
        score += (board.side_to_move() == WHITE) ? 28 : -28;
    }

    return score;
}

int HandCraftedEvaluator::side_relative_eval(const Board& board)
{
    int who2move = (board.side_to_move() == WHITE) ? 1 : -1;
    return who2move * evaluate(board);
}
