/*
 * File:   PositionAnalyzer.cpp
 *
 * Stateless utility that extracts structured analysis data from a board
 * position. This file implements compute_eval_breakdown() and analyze_pawns();
 * remaining methods are stubs to be filled in by later tasks.
 */

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "PositionAnalyzer.h"

#include "Evaluator.h"
#include "MoveGenerator.h"
#include "Output.h"

// ---------------------------------------------------------------------------
// Helper: file bitboard constant for file index 0..7
// ---------------------------------------------------------------------------
static constexpr U64 FILE_BB[8] = {
    0x0101010101010101ULL << 0, 0x0101010101010101ULL << 1, 0x0101010101010101ULL << 2,
    0x0101010101010101ULL << 3, 0x0101010101010101ULL << 4, 0x0101010101010101ULL << 5,
    0x0101010101010101ULL << 6, 0x0101010101010101ULL << 7,
};

// ---------------------------------------------------------------------------
// compute_eval_breakdown()
//
// Decomposes the HCE evaluation into six components:
//   material       — PSQT material (total minus all named sub-scores)
//   mobility       — from HCE eval_mobility()
//   king_safety    — from HCE eval_king_safety()
//   pawn_structure — from HCE eval_pawn_structure()
//   piece_bonuses  — from HCE eval_piece_bonuses()
//   tempo          — +28 for white-to-move, -28 for black-to-move
//
// Calls evaluate() on the board's HCE to populate cached sub-scores, then
// reads them via the public accessors.
// All values are from the side-to-move perspective (matching evaluate()).
// ---------------------------------------------------------------------------
EvalBreakdown PositionAnalyzer::compute_eval_breakdown(const Board& board)
{
    EvalBreakdown bd {};
    Board board_copy = board;
    HandCraftedEvaluator& hce = board_copy.get_hce();
    int total = hce.evaluate(board_copy);

    bd.pawn_structure = hce.get_pawn_structure_score();
    bd.king_safety = hce.get_king_safety_score();
    bd.mobility = hce.get_mobility_score();
    bd.piece_bonuses = hce.get_piece_bonuses_score();

    // Tempo: +28 for white-to-move, -28 for black-to-move (white-relative)
    bd.tempo = (board_copy.side_to_move() == WHITE) ? 28 : -28;

    // Material = total minus all named sub-scores
    bd.material =
        total - bd.pawn_structure - bd.king_safety - bd.mobility - bd.piece_bonuses - bd.tempo;

    // Convert white-relative to side-to-move perspective
    if (board.side_to_move() == BLACK)
    {
        bd.material = -bd.material;
        bd.mobility = -bd.mobility;
        bd.king_safety = -bd.king_safety;
        bd.pawn_structure = -bd.pawn_structure;
        bd.tempo = -bd.tempo;
        bd.piece_bonuses = -bd.piece_bonuses;
    }
    return bd;
}

// ---------------------------------------------------------------------------
// analyze_pawns()
//
// Detects isolated, doubled, and passed pawns for the given side using
// bitboard operations.
//
// - Isolated: a pawn whose adjacent files have no friendly pawns.
// - Doubled:  multiple friendly pawns on the same file.
// - Passed:   a pawn with no opposing pawns on its file or adjacent files
//             ahead of it.
// ---------------------------------------------------------------------------
PawnFeatures PositionAnalyzer::analyze_pawns(const Board& board, U8 side)
{
    PawnFeatures features;

    U64 friendly_pawns = board.bitboard(PAWN | side);
    U64 enemy_pawns = board.bitboard(PAWN | (side ^ 1));

    // Iterate over each file
    for (int f = 0; f < 8; f++)
    {
        U64 file_bb = FILE_BB[f];
        U64 pawns_on_file = friendly_pawns & file_bb;
        int count = pop_count(pawns_on_file);

        if (count == 0)
            continue;

        // --- Doubled: more than one pawn on this file ---
        if (count > 1)
        {
            features.doubled.push_back(f);
        }

        // --- Isolated: no friendly pawns on adjacent files ---
        U64 adjacent_files = BB_EMPTY;
        if (f > 0)
            adjacent_files |= FILE_BB[f - 1];
        if (f < 7)
            adjacent_files |= FILE_BB[f + 1];

        if ((friendly_pawns & adjacent_files) == 0)
        {
            features.isolated.push_back(f);
        }

        // --- Passed: check each pawn on this file ---
        U64 pawns_iter = pawns_on_file;
        while (pawns_iter)
        {
            U8 sq = bit_scan_forward(pawns_iter);
            int rank = sq / 8;

            // Build a mask of the file and adjacent files ahead of this pawn
            U64 front_mask = BB_EMPTY;
            U64 relevant_files = file_bb | adjacent_files;

            if (side == WHITE)
            {
                // "Ahead" for white means higher ranks (rank+1 .. 7)
                for (int r = rank + 1; r <= 7; r++)
                {
                    front_mask |= (0xFFULL << (r * 8));
                }
            }
            else
            {
                // "Ahead" for black means lower ranks (rank-1 .. 0)
                for (int r = rank - 1; r >= 0; r--)
                {
                    front_mask |= (0xFFULL << (r * 8));
                }
            }

            U64 blocking_zone = relevant_files & front_mask;

            if ((enemy_pawns & blocking_zone) == 0)
            {
                // This pawn is passed — add the file if not already added
                if (features.passed.empty() || features.passed.back() != f)
                {
                    features.passed.push_back(f);
                }
            }

            pawns_iter &= pawns_iter - 1;  // clear LSB
        }
    }

    return features;
}

// ===========================================================================
// STUB implementations — to be filled in by tasks 2.3, 2.4, 2.5
// ===========================================================================

// ---------------------------------------------------------------------------
// analyze() — full position analysis (task 2.5)
// ---------------------------------------------------------------------------

// Forward declarations for helpers defined later in this file
static std::string piece_char(U8 piece_type);
static std::string piece_on_square_str(U8 piece, U8 sq);

PositionReport PositionAnalyzer::analyze(const Board& board, const std::vector<PVLine>& pv_lines)
{
    PositionReport report {};

    // 1. FEN
    report.fen = Output::board_to_fen(board);

    // 2-3. Eval breakdown (always from HCE, even if NNUE is loaded)
    report.breakdown = compute_eval_breakdown(board);

    // eval_cp = sum of breakdown components (ensures consistency)
    report.eval_cp = report.breakdown.material + report.breakdown.pawn_structure
        + report.breakdown.king_safety + report.breakdown.mobility + report.breakdown.piece_bonuses
        + report.breakdown.tempo;

    // 5-6. Hanging pieces
    report.hanging_white = find_hanging_pieces(board, WHITE);
    report.hanging_black = find_hanging_pieces(board, BLACK);

    // 7-8. Threats
    report.threats_white = find_threats(board, WHITE);
    report.threats_black = find_threats(board, BLACK);

    // 9-10. Pawn structure
    report.pawns_white = analyze_pawns(board, WHITE);
    report.pawns_black = analyze_pawns(board, BLACK);

    // 11-12. King safety
    report.king_safety_white = assess_king_safety(board, WHITE);
    report.king_safety_black = assess_king_safety(board, BLACK);

    // 13. Top lines (copy PV lines directly)
    report.top_lines = pv_lines;

    // 14. Tactics
    report.tactics = detect_tactics(board, pv_lines);

    // 15. Threat map
    report.threat_map = build_threat_map(board);

    // 15b. Threat map summary
    if (report.threat_map.empty())
    {
        report.threat_map_summary = "no significant threats on the board";
    }
    else
    {
        std::string summary;
        int count = 0;
        for (const auto& entry : report.threat_map)
        {
            if (count >= 4)
                break;

            std::string clause;
            if (entry.piece != EMPTY)
            {
                std::string piece_str = piece_on_square_str(entry.piece, entry.square);
                if (entry.net_attacked)
                {
                    // Check if undefended (0 own defenders)
                    U8 piece_color = entry.piece & 1;
                    int own_def =
                        (piece_color == WHITE) ? entry.white_defenders : entry.black_defenders;
                    if (own_def == 0)
                        clause = piece_str + " is undefended and attacked";
                    else
                        clause = piece_str + " is under attack";
                }
                else
                {
                    clause = piece_str + " is contested";
                }
            }
            else
            {
                clause = Output::square(entry.square) + " is contested by both sides";
            }

            if (!summary.empty())
                summary += "; ";
            summary += clause;
            count++;
        }
        report.threat_map_summary = summary;
    }

    // 16. Critical moment
    std::string reason;
    report.critical_moment = is_critical_moment(pv_lines, reason);
    report.critical_reason = reason;

    return report;
}

// ---------------------------------------------------------------------------
// find_hanging_pieces()
//
// Uses MoveGenerator::attacks_to() to count attackers and defenders for each
// occupied square belonging to `side`. A piece is "hanging" when it is
// attacked by the opponent more times than defended by its own side.
// Kings are excluded.
// ---------------------------------------------------------------------------
std::vector<HangingPiece> PositionAnalyzer::find_hanging_pieces(const Board& board, U8 side)
{
    std::vector<HangingPiece> result;

    // Build occupied bitboard
    U64 occupied = BB_EMPTY;
    for (int p = WHITE_PAWN; p <= BLACK_KING; p++)
        occupied |= board.bitboard(p);

    // Build color masks
    U64 own_pieces = board.bitboard(PAWN | side) | board.bitboard(KNIGHT | side)
        | board.bitboard(BISHOP | side) | board.bitboard(ROOK | side) | board.bitboard(QUEEN | side)
        | board.bitboard(KING | side);

    U8 opp = side ^ 1;
    U64 opp_pieces = board.bitboard(PAWN | opp) | board.bitboard(KNIGHT | opp)
        | board.bitboard(BISHOP | opp) | board.bitboard(ROOK | opp) | board.bitboard(QUEEN | opp)
        | board.bitboard(KING | opp);

    // Iterate over all own pieces (excluding kings)
    U64 candidates = own_pieces & ~board.bitboard(KING | side);
    while (candidates)
    {
        U8 sq = bit_scan_forward(candidates);
        U64 all_attackers = MoveGenerator::attacks_to(board, occupied, sq);

        int opponent_attackers = pop_count(all_attackers & opp_pieces);
        int own_defenders = pop_count(all_attackers & own_pieces);

        if (opponent_attackers > own_defenders)
        {
            U8 piece_on_sq = board[sq];
            HangingPiece hp;
            hp.square = sq;
            hp.piece = piece_on_sq & ~1;  // strip color bit to get type
            result.push_back(hp);
        }

        candidates &= candidates - 1;  // clear LSB
    }

    return result;
}

// ---------------------------------------------------------------------------
// Piece value for threat comparison (from SEE_PIECE_VALUE)
// Index: piece_type >> 1  (0=empty, 1=pawn, 2=knight, 3=bishop, 4=rook, 5=queen, 6=king)
// ---------------------------------------------------------------------------
static int piece_value(U8 piece_type)
{
    // piece_type is the raw piece without color (e.g. PAWN=2, KNIGHT=4, ...)
    int idx = piece_type >> 1;
    if (idx < 0 || idx > 6)
        return 0;
    return SEE_PIECE_VALUE[idx];
}

// Helper: get a piece character for descriptions (e.g. "N", "B", "R", "Q", "K", "")
static std::string piece_char(U8 piece_type)
{
    switch (piece_type & ~1)
    {
        case PAWN:
            return "";
        case KNIGHT:
            return "N";
        case BISHOP:
            return "B";
        case ROOK:
            return "R";
        case QUEEN:
            return "Q";
        case KING:
            return "K";
        default:
            return "";
    }
}

// Helper: build a piece+square string like "Nc3", "Qd1", "e4" (pawn)
static std::string piece_on_square_str(U8 piece, U8 sq)
{
    return piece_char(piece) + Output::square(sq);
}

// Helper: build a 4-char UCI move string from source and destination squares
static std::string uci_from_squares(U8 from, U8 to)
{
    std::string s;
    s += static_cast<char>('a' + (from & 7));
    s += static_cast<char>('1' + (from >> 3));
    s += static_cast<char>('a' + (to & 7));
    s += static_cast<char>('1' + (to >> 3));
    return s;
}

// ---------------------------------------------------------------------------
// find_threats()
//
// Scans for immediate tactical threats that `side` can make:
// - Check: pieces that can give check
// - Capture: pieces that can capture undefended or higher-value pieces
// - Fork: a piece attacking two or more higher-value pieces
// - Pin: sliding piece attacking through a less-valuable piece to a more-valuable one
// - Skewer: like pin but more-valuable piece is in front
// - Discovered attack: moving a piece reveals an attack from a slider behind
// ---------------------------------------------------------------------------
std::vector<Threat> PositionAnalyzer::find_threats(const Board& board, U8 side)
{
    std::vector<Threat> result;

    U8 opp = side ^ 1;

    // Build occupied bitboard
    U64 occupied = BB_EMPTY;
    for (int p = WHITE_PAWN; p <= BLACK_KING; p++)
        occupied |= board.bitboard(p);

    // Build color masks
    U64 own_pieces = board.bitboard(PAWN | side) | board.bitboard(KNIGHT | side)
        | board.bitboard(BISHOP | side) | board.bitboard(ROOK | side) | board.bitboard(QUEEN | side)
        | board.bitboard(KING | side);

    U64 opp_pieces = board.bitboard(PAWN | opp) | board.bitboard(KNIGHT | opp)
        | board.bitboard(BISHOP | opp) | board.bitboard(ROOK | opp) | board.bitboard(QUEEN | opp)
        | board.bitboard(KING | opp);

    // Opponent king square (for check detection)
    U64 opp_king_bb = board.bitboard(KING | opp);
    U8 opp_king_sq = (opp_king_bb != 0) ? bit_scan_forward(opp_king_bb) : 64;

    // -----------------------------------------------------------------------
    // 1. Check threats: pieces that can give check
    // -----------------------------------------------------------------------
    if (opp_king_sq < 64)
    {
        // Scan own pieces to find those that can move to give check

        // Knights that can reach squares attacking the opp king
        U64 knights = board.bitboard(KNIGHT | side);
        while (knights)
        {
            U8 sq = bit_scan_forward(knights);
            U64 targets = MoveGenerator::knight_targets(1ULL << sq);
            // Can this knight move to a square that attacks the opp king?
            U64 knight_check_sqs = MoveGenerator::knight_targets(1ULL << opp_king_sq);
            U64 check_moves = targets & knight_check_sqs & ~own_pieces;
            if (check_moves)
            {
                U8 check_sq = bit_scan_forward(check_moves);
                Threat t;
                t.type = "check";
                t.source_square = sq;
                t.target_squares.push_back(check_sq);
                t.uci_move = uci_from_squares(sq, check_sq);
                t.description =
                    piece_on_square_str(KNIGHT | side, sq) + " can give check via " + t.uci_move;
                result.push_back(t);
            }
            knights &= knights - 1;
        }

        // Rooks/Queens: can they move to a square on the king's rank/file?
        U64 rook_like = board.bitboard(ROOK | side) | board.bitboard(QUEEN | side);
        while (rook_like)
        {
            U8 sq = bit_scan_forward(rook_like);
            U64 targets = MoveGenerator::rook_targets(1ULL << sq, occupied);
            U64 king_rook_atk = MoveGenerator::rook_targets(1ULL << opp_king_sq, occupied);
            U64 check_moves = targets & king_rook_atk & ~own_pieces;
            if (check_moves)
            {
                U8 check_sq = bit_scan_forward(check_moves);
                Threat t;
                t.type = "check";
                t.source_square = sq;
                t.target_squares.push_back(check_sq);
                t.uci_move = uci_from_squares(sq, check_sq);
                t.description =
                    piece_on_square_str(board[sq], sq) + " can give check via " + t.uci_move;
                result.push_back(t);
            }
            rook_like &= rook_like - 1;
        }

        // Bishops/Queens: can they move to a square on the king's diagonals?
        U64 bishop_like = board.bitboard(BISHOP | side) | board.bitboard(QUEEN | side);
        while (bishop_like)
        {
            U8 sq = bit_scan_forward(bishop_like);
            U64 targets = MoveGenerator::bishop_targets(1ULL << sq, occupied);
            U64 king_bishop_atk = MoveGenerator::bishop_targets(1ULL << opp_king_sq, occupied);
            U64 check_moves = targets & king_bishop_atk & ~own_pieces;
            if (check_moves)
            {
                // Avoid duplicate if already reported as rook-like check (queen)
                U8 pt = board[sq] & ~1;
                if (pt != QUEEN)  // queens already handled above via rook_like
                {
                    U8 check_sq = bit_scan_forward(check_moves);
                    Threat t;
                    t.type = "check";
                    t.source_square = sq;
                    t.target_squares.push_back(check_sq);
                    t.uci_move = uci_from_squares(sq, check_sq);
                    t.description =
                        piece_on_square_str(board[sq], sq) + " can give check via " + t.uci_move;
                    result.push_back(t);
                }
            }
            bishop_like &= bishop_like - 1;
        }
    }

    // -----------------------------------------------------------------------
    // 2. Capture threats: pieces that can capture undefended or higher-value pieces
    // -----------------------------------------------------------------------
    U64 opp_iter = opp_pieces & ~opp_king_bb;  // exclude king from capture targets
    while (opp_iter)
    {
        U8 target_sq = bit_scan_forward(opp_iter);
        U8 target_piece = board[target_sq];
        int target_val = piece_value(target_piece & ~1);

        U64 attackers = MoveGenerator::attacks_to(board, occupied, target_sq) & own_pieces;
        // Check if target is undefended
        U64 defenders = MoveGenerator::attacks_to(board, occupied, target_sq) & opp_pieces;
        bool undefended = (pop_count(defenders) == 0);

        while (attackers)
        {
            U8 atk_sq = bit_scan_forward(attackers);
            U8 atk_piece = board[atk_sq];
            int atk_val = piece_value(atk_piece & ~1);

            // Report if target is undefended or attacker is less valuable
            if (undefended || atk_val < target_val)
            {
                Threat t;
                t.type = "capture";
                t.source_square = atk_sq;
                t.target_squares.push_back(target_sq);
                t.uci_move = uci_from_squares(atk_sq, target_sq);
                std::string desc = piece_on_square_str(atk_piece, atk_sq) + " can capture ";
                desc += (undefended ? "undefended " : "");
                desc += piece_on_square_str(target_piece, target_sq);
                desc += " via " + t.uci_move;
                t.description = desc;
                result.push_back(t);
            }
            attackers &= attackers - 1;
        }
        opp_iter &= opp_iter - 1;
    }

    // -----------------------------------------------------------------------
    // 3. Fork threats: a piece that attacks 2+ higher-value opponent pieces
    // -----------------------------------------------------------------------
    U64 own_iter = own_pieces & ~board.bitboard(KING | side);
    while (own_iter)
    {
        U8 sq = bit_scan_forward(own_iter);
        U8 piece = board[sq];
        U8 pt = piece & ~1;
        int own_val = piece_value(pt);

        // Get attack targets for this piece
        U64 attacks = BB_EMPTY;
        switch (pt)
        {
            case KNIGHT:
                attacks = MoveGenerator::knight_targets(1ULL << sq);
                break;
            case BISHOP:
                attacks = MoveGenerator::bishop_targets(1ULL << sq, occupied);
                break;
            case ROOK:
                attacks = MoveGenerator::rook_targets(1ULL << sq, occupied);
                break;
            case QUEEN:
                attacks = MoveGenerator::rook_targets(1ULL << sq, occupied)
                    | MoveGenerator::bishop_targets(1ULL << sq, occupied);
                break;
            case PAWN:
                attacks = MoveGenerator::pawn_targets(1ULL << sq, side);
                break;
            default:
                break;
        }

        // Find opponent pieces attacked that are higher value
        U64 attacked_opp = attacks & opp_pieces;
        std::vector<U8> higher_value_targets;
        U64 tmp = attacked_opp;
        while (tmp)
        {
            U8 tsq = bit_scan_forward(tmp);
            U8 tp = board[tsq];
            if (piece_value(tp & ~1) > own_val || (tp & ~1) == KING)
                higher_value_targets.push_back(tsq);
            tmp &= tmp - 1;
        }

        if (higher_value_targets.size() >= 2)
        {
            Threat t;
            t.type = "fork";
            t.source_square = sq;
            t.target_squares = higher_value_targets;
            std::string desc = piece_on_square_str(piece, sq) + " forks ";
            for (size_t i = 0; i < higher_value_targets.size(); i++)
            {
                if (i > 0)
                    desc += " and ";
                desc +=
                    piece_on_square_str(board[higher_value_targets[i]], higher_value_targets[i]);
            }
            t.description = desc;
            result.push_back(t);
        }

        own_iter &= own_iter - 1;
    }

    // -----------------------------------------------------------------------
    // 4. Pin and Skewer detection (sliding pieces)
    // -----------------------------------------------------------------------
    U64 sliders =
        board.bitboard(BISHOP | side) | board.bitboard(ROOK | side) | board.bitboard(QUEEN | side);

    while (sliders)
    {
        U8 sq = bit_scan_forward(sliders);
        U8 piece = board[sq];
        U8 pt = piece & ~1;

        // Get the ray directions this slider can attack along
        U64 slider_attacks = BB_EMPTY;
        if (pt == BISHOP || pt == QUEEN)
            slider_attacks |= MoveGenerator::bishop_targets(1ULL << sq, occupied);
        if (pt == ROOK || pt == QUEEN)
            slider_attacks |= MoveGenerator::rook_targets(1ULL << sq, occupied);

        // For each opponent piece on the attack ray, check if there's another
        // piece behind it on the same line
        U64 attacked_opp = slider_attacks & opp_pieces;
        while (attacked_opp)
        {
            U8 front_sq = bit_scan_forward(attacked_opp);
            U8 front_piece = board[front_sq];

            // Check if there's a line from slider through front_piece
            U64 line = lines_along(sq, front_sq);
            if (line == 0)
            {
                attacked_opp &= attacked_opp - 1;
                continue;
            }

            // Look for a piece behind front_sq on the same line
            U64 between_slider_front = squares_between(sq, front_sq);
            // Remove the front piece from occupied to see through it
            U64 occ_without_front = occupied & ~(1ULL << front_sq);

            // Re-compute slider attacks without the front piece
            U64 extended_attacks = BB_EMPTY;
            if (pt == BISHOP || pt == QUEEN)
                extended_attacks |= MoveGenerator::bishop_targets(1ULL << sq, occ_without_front);
            if (pt == ROOK || pt == QUEEN)
                extended_attacks |= MoveGenerator::rook_targets(1ULL << sq, occ_without_front);

            // Find pieces on the line behind front_sq
            U64 behind_mask = line & ~(1ULL << sq) & ~(1ULL << front_sq) & ~between_slider_front;
            U64 behind_pieces = extended_attacks & behind_mask & (opp_pieces | own_pieces);

            if (behind_pieces)
            {
                U8 back_sq = bit_scan_forward(behind_pieces);
                U8 back_piece = board[back_sq];

                int front_val = piece_value(front_piece & ~1);
                int back_val = piece_value(back_piece & ~1);

                // Only report if the back piece is an opponent piece or king
                bool back_is_opp = (back_piece & 1) == opp;

                if (back_is_opp && front_val < back_val)
                {
                    // Pin: less valuable piece in front, more valuable behind
                    Threat t;
                    t.type = "pin";
                    t.source_square = sq;
                    t.target_squares = { front_sq, back_sq };
                    t.description = piece_on_square_str(piece, sq) + " pins "
                        + piece_on_square_str(front_piece, front_sq) + " to "
                        + piece_on_square_str(back_piece, back_sq);
                    result.push_back(t);
                }
                else if (back_is_opp && front_val > back_val)
                {
                    // Skewer: more valuable piece in front, less valuable behind
                    Threat t;
                    t.type = "skewer";
                    t.source_square = sq;
                    t.target_squares = { front_sq, back_sq };
                    t.description = piece_on_square_str(piece, sq) + " skewers "
                        + piece_on_square_str(front_piece, front_sq) + " and "
                        + piece_on_square_str(back_piece, back_sq);
                    result.push_back(t);
                }
            }

            attacked_opp &= attacked_opp - 1;
        }

        sliders &= sliders - 1;
    }

    return result;
}

// ---------------------------------------------------------------------------
// assess_king_safety()
//
// Evaluates the pawn shield around the king of `side`:
// - Count pawns on the king's file and adjacent files within 1-2 ranks ahead
// - Check for open files near the king
// - Detect pawn storms (enemy pawns advanced near king)
// - Generate a score (negative = unsafe) and position-aware description
// ---------------------------------------------------------------------------
KingSafety PositionAnalyzer::assess_king_safety(const Board& board, U8 side)
{
    U64 king_bb = board.bitboard(KING | side);
    if (king_bb == 0)
        return KingSafety { 0, "no king found" };

    U8 king_sq = bit_scan_forward(king_bb);
    int king_file = king_sq % 8;
    int king_rank = king_sq / 8;

    U64 friendly_pawns = board.bitboard(PAWN | side);
    U64 enemy_pawns = board.bitboard(PAWN | (side ^ 1));

    int missing_shield = 0;
    int open_files_near_king = 0;
    int storm_pawn_count = 0;
    bool pawn_storm = false;
    std::string missing_files;

    const char file_chars[] = "abcdefgh";

    for (int f = std::max(0, king_file - 1); f <= std::min(7, king_file + 1); f++)
    {
        U64 file_bb = FILE_BB[f];
        U64 file_friendly = file_bb & friendly_pawns;
        U64 file_enemy = file_bb & enemy_pawns;

        // Check if this is an open file (no pawns of either side)
        if (file_friendly == 0 && file_enemy == 0)
            open_files_near_king++;

        // Detect pawn storm: count enemy pawns advanced past the 4th rank near king
        // For White's king: Black pawns storming down (rank 4 or below, erank <= 3)
        // For Black's king: White pawns storming up (rank 5 or above, erank >= 4)
        U64 enemy_on_file = file_enemy;
        while (enemy_on_file)
        {
            U8 esq = bit_scan_forward(enemy_on_file);
            int erank = esq / 8;
            if (side == WHITE && erank <= 3)  // Black pawns on rank 1-4 (storming White's king)
                storm_pawn_count++;
            if (side == BLACK && erank >= 4)  // White pawns on rank 5-8 (storming Black's king)
                storm_pawn_count++;
            enemy_on_file &= enemy_on_file - 1;
        }

        if (file_friendly == 0)
        {
            // No friendly pawn on this file at all
            missing_shield += 2;
            if (!missing_files.empty())
                missing_files += ",";
            missing_files += file_chars[f];
            continue;
        }

        // Check if there's a pawn within 1-2 ranks ahead of the king
        bool found_shield = false;
        if (side == WHITE)
        {
            for (int r = king_rank + 1; r <= std::min(7, king_rank + 2); r++)
            {
                if (file_friendly & (1ULL << (r * 8 + f)))
                {
                    found_shield = true;
                    break;
                }
            }
        }
        else
        {
            for (int r = king_rank - 1; r >= std::max(0, king_rank - 2); r--)
            {
                if (file_friendly & (1ULL << (r * 8 + f)))
                {
                    found_shield = true;
                    break;
                }
            }
        }

        if (!found_shield)
        {
            missing_shield += 1;
            if (!missing_files.empty())
                missing_files += ",";
            missing_files += file_chars[f];
        }
    }

    // Score: each missing shield unit costs ~15cp
    int score = -15 * missing_shield;
    score -= 20 * open_files_near_king;

    // Pawn storm requires 2+ advanced enemy pawns near the king
    pawn_storm = (storm_pawn_count >= 2);

    // --- Position-aware description ---

    // 1. Determine castling status
    std::string castling_status;
    if (side == WHITE)
    {
        if (king_sq == G1)
            castling_status = "kingside castled";
        else if (king_sq == C1)
            castling_status = "queenside castled";
        else if (king_sq == E1)
        {
            if (board.castling_rights() & (WHITE_KING_SIDE | WHITE_QUEEN_SIDE))
                castling_status = "king uncastled, still has castling rights";
            else
                castling_status = "king stuck in center, castling rights lost";
        }
        else
            castling_status = "king displaced to " + Output::square(king_sq);
    }
    else
    {
        if (king_sq == G8)
            castling_status = "kingside castled";
        else if (king_sq == C8)
            castling_status = "queenside castled";
        else if (king_sq == E8)
        {
            if (board.castling_rights() & (BLACK_KING_SIDE | BLACK_QUEEN_SIDE))
                castling_status = "king uncastled, still has castling rights";
            else
                castling_status = "king stuck in center, castling rights lost";
        }
        else
            castling_status = "king displaced to " + Output::square(king_sq);
    }

    // 2. Build description
    std::string desc = castling_status;

    if (missing_shield == 0 && open_files_near_king == 0 && !pawn_storm)
    {
        desc += ", solid pawn shield";
    }
    else
    {
        if (!missing_files.empty())
            desc += ", missing " + missing_files + "-pawn shield";
        if (pawn_storm)
            desc += ", pawn storm detected";
        if (open_files_near_king > 0)
            desc += ", open file near king";
    }

    return KingSafety { score, desc };
}

// ---------------------------------------------------------------------------
// build_threat_map()
//
// Iterates over all 64 squares. For each square that is attacked or contains
// a piece, computes white/black attacker counts and defender counts.
// net_attacked is true when the piece on the square is attacked more by the
// opponent than defended by its own side.
//
// Filtering: only include tactically interesting squares:
//   (a) net_attacked is true (occupied piece attacked more than defended)
//   (b) occupied and opponent attackers > own defenders
//   (c) key central square (d4/d5/e4/e5) with ≥1 attacker from either side
// Sorted by priority, capped at 16 entries.
// ---------------------------------------------------------------------------
std::vector<ThreatMapEntry> PositionAnalyzer::build_threat_map(const Board& board)
{
    std::vector<ThreatMapEntry> candidates;

    // Build occupied bitboard
    U64 occupied = BB_EMPTY;
    for (int p = WHITE_PAWN; p <= BLACK_KING; p++)
        occupied |= board.bitboard(p);

    // Build color masks
    U64 white_pieces = board.bitboard(WHITE_PAWN) | board.bitboard(WHITE_KNIGHT)
        | board.bitboard(WHITE_BISHOP) | board.bitboard(WHITE_ROOK) | board.bitboard(WHITE_QUEEN)
        | board.bitboard(WHITE_KING);

    U64 black_pieces = board.bitboard(BLACK_PAWN) | board.bitboard(BLACK_KNIGHT)
        | board.bitboard(BLACK_BISHOP) | board.bitboard(BLACK_ROOK) | board.bitboard(BLACK_QUEEN)
        | board.bitboard(BLACK_KING);

    // Key central squares
    auto is_key_central = [](int sq) { return sq == D4 || sq == D5 || sq == E4 || sq == E5; };

    for (int sq = 0; sq < NUM_SQUARES; sq++)
    {
        U8 piece_on_sq = board[sq];
        U64 all_attackers = MoveGenerator::attacks_to(board, occupied, static_cast<U8>(sq));

        int white_atk = pop_count(all_attackers & white_pieces);
        int black_atk = pop_count(all_attackers & black_pieces);

        // Skip empty squares with no attackers
        if (piece_on_sq == EMPTY && white_atk == 0 && black_atk == 0)
            continue;

        ThreatMapEntry entry;
        entry.square = static_cast<U8>(sq);
        entry.piece = piece_on_sq;
        entry.white_attackers = white_atk;
        entry.black_attackers = black_atk;

        // Compute defenders and attackers relative to the piece's color
        if (piece_on_sq != EMPTY)
        {
            U8 piece_color = piece_on_sq & 1;
            if (piece_color == WHITE)
            {
                entry.white_defenders = white_atk;
                entry.black_defenders = 0;
                entry.net_attacked = (black_atk > white_atk);
            }
            else
            {
                entry.white_defenders = 0;
                entry.black_defenders = black_atk;
                entry.net_attacked = (white_atk > black_atk);
            }
        }
        else
        {
            entry.white_defenders = 0;
            entry.black_defenders = 0;
            entry.net_attacked = false;
        }

        // --- Filtering: only include tactically interesting squares ---
        bool include = false;

        // (a) net_attacked is true
        if (entry.net_attacked)
            include = true;

        // (b) occupied and opponent attackers > own defenders
        if (piece_on_sq != EMPTY)
        {
            U8 piece_color = piece_on_sq & 1;
            int opp_atk = (piece_color == WHITE) ? black_atk : white_atk;
            int own_def = (piece_color == WHITE) ? white_atk : black_atk;
            if (opp_atk > own_def)
                include = true;
        }

        // (c) key central square with ≥1 attacker
        if (is_key_central(sq) && (white_atk > 0 || black_atk > 0))
            include = true;

        if (include)
            candidates.push_back(entry);
    }

    // Sort by priority: occupied net-attacked pieces (by piece value desc) first,
    // then contested central squares
    std::sort(candidates.begin(),
              candidates.end(),
              [&](const ThreatMapEntry& a, const ThreatMapEntry& b)
              {
                  // Priority 1: occupied net-attacked pieces
                  bool a_occ_attacked = (a.piece != EMPTY && a.net_attacked);
                  bool b_occ_attacked = (b.piece != EMPTY && b.net_attacked);
                  if (a_occ_attacked != b_occ_attacked)
                      return a_occ_attacked > b_occ_attacked;

                  // Within occupied net-attacked: sort by piece value descending
                  if (a_occ_attacked && b_occ_attacked)
                  {
                      int a_val = piece_value(a.piece & ~1);
                      int b_val = piece_value(b.piece & ~1);
                      if (a_val != b_val)
                          return a_val > b_val;
                  }

                  // Priority 2: key central squares
                  bool a_central = is_key_central(a.square);
                  bool b_central = is_key_central(b.square);
                  if (a_central != b_central)
                      return a_central > b_central;

                  return a.square < b.square;  // stable tie-break
              });

    // Truncate to 16 entries max
    if (candidates.size() > 16)
        candidates.resize(16);

    return candidates;
}

// ---------------------------------------------------------------------------
// detect_tactics()
//
// Combines on-board pattern detection with PV analysis:
// - On-board: scan for forks, pins, skewers, discovered attacks, back-rank
//   threats, overloaded pieces
// - In-PV: walk each PV line (first 3-4 moves), apply moves to a board copy,
//   and check for tactical motifs. Mark these with in_pv: true.
// ---------------------------------------------------------------------------
std::vector<Tactic> PositionAnalyzer::detect_tactics(const Board& board,
                                                     const std::vector<PVLine>& pv_lines)
{
    std::vector<Tactic> result;

    U8 stm = board.side_to_move();
    U8 opp = stm ^ 1;

    // Build occupied bitboard
    U64 occupied = BB_EMPTY;
    for (int p = WHITE_PAWN; p <= BLACK_KING; p++)
        occupied |= board.bitboard(p);

    U64 own_pieces = board.bitboard(PAWN | stm) | board.bitboard(KNIGHT | stm)
        | board.bitboard(BISHOP | stm) | board.bitboard(ROOK | stm) | board.bitboard(QUEEN | stm)
        | board.bitboard(KING | stm);

    U64 opp_pieces = board.bitboard(PAWN | opp) | board.bitboard(KNIGHT | opp)
        | board.bitboard(BISHOP | opp) | board.bitboard(ROOK | opp) | board.bitboard(QUEEN | opp)
        | board.bitboard(KING | opp);

    // -----------------------------------------------------------------------
    // On-board: Back-rank threat detection
    // Check if opponent's back rank is weak (king on back rank, no pawn shield)
    // -----------------------------------------------------------------------
    {
        U64 opp_king_bb = board.bitboard(KING | opp);
        if (opp_king_bb)
        {
            U8 opp_king_sq = bit_scan_forward(opp_king_bb);
            int king_rank = opp_king_sq / 8;
            int back_rank = (opp == WHITE) ? 0 : 7;

            if (king_rank == back_rank)
            {
                // Check if the rank in front of the king is blocked by own pawns
                int shield_rank = (opp == WHITE) ? 1 : 6;
                int king_file = opp_king_sq % 8;
                U64 opp_pawns = board.bitboard(PAWN | opp);
                bool blocked = true;

                for (int f = std::max(0, king_file - 1); f <= std::min(7, king_file + 1); f++)
                {
                    U8 shield_sq = static_cast<U8>(shield_rank * 8 + f);
                    if (!(opp_pawns & (1ULL << shield_sq)))
                    {
                        blocked = false;
                        break;
                    }
                }

                if (blocked)
                {
                    // Check if we have a rook or queen that can reach the back rank
                    U64 our_heavy = board.bitboard(ROOK | stm) | board.bitboard(QUEEN | stm);
                    while (our_heavy)
                    {
                        U8 sq = bit_scan_forward(our_heavy);
                        U64 rook_atk = MoveGenerator::rook_targets(1ULL << sq, occupied);
                        U64 back_rank_bb = (opp == WHITE) ? ROW_1 : ROW_8;
                        if (rook_atk & back_rank_bb)
                        {
                            Tactic t;
                            t.type = "back_rank_threat";
                            t.squares = { sq, opp_king_sq };
                            t.pieces = { piece_on_square_str(board[sq], sq),
                                         piece_on_square_str(KING | opp, opp_king_sq) };
                            t.in_pv = false;
                            t.description =
                                "Back rank threat: " + piece_on_square_str(board[sq], sq)
                                + " targets back rank with king trapped";
                            result.push_back(t);
                            break;  // one back-rank threat is enough
                        }
                        our_heavy &= our_heavy - 1;
                    }
                }
            }
        }
    }

    // -----------------------------------------------------------------------
    // On-board: Fork detection (same logic as find_threats but reported as Tactic)
    // -----------------------------------------------------------------------
    {
        U64 own_iter = own_pieces & ~board.bitboard(KING | stm);
        while (own_iter)
        {
            U8 sq = bit_scan_forward(own_iter);
            U8 piece = board[sq];
            U8 pt = piece & ~1;
            int own_val = piece_value(pt);

            U64 attacks = BB_EMPTY;
            switch (pt)
            {
                case KNIGHT:
                    attacks = MoveGenerator::knight_targets(1ULL << sq);
                    break;
                case BISHOP:
                    attacks = MoveGenerator::bishop_targets(1ULL << sq, occupied);
                    break;
                case ROOK:
                    attacks = MoveGenerator::rook_targets(1ULL << sq, occupied);
                    break;
                case QUEEN:
                    attacks = MoveGenerator::rook_targets(1ULL << sq, occupied)
                        | MoveGenerator::bishop_targets(1ULL << sq, occupied);
                    break;
                case PAWN:
                    attacks = MoveGenerator::pawn_targets(1ULL << sq, stm);
                    break;
                default:
                    break;
            }

            U64 attacked_opp = attacks & opp_pieces;
            std::vector<U8> fork_targets;
            U64 tmp = attacked_opp;
            while (tmp)
            {
                U8 tsq = bit_scan_forward(tmp);
                U8 tp = board[tsq];
                if (piece_value(tp & ~1) > own_val || (tp & ~1) == KING)
                    fork_targets.push_back(tsq);
                tmp &= tmp - 1;
            }

            if (fork_targets.size() >= 2)
            {
                Tactic t;
                t.type = "fork";
                t.squares.push_back(sq);
                for (U8 ts : fork_targets)
                    t.squares.push_back(ts);
                t.pieces.push_back(piece_on_square_str(piece, sq));
                for (U8 ts : fork_targets)
                    t.pieces.push_back(piece_on_square_str(board[ts], ts));
                t.in_pv = false;
                std::string desc = "Fork: " + piece_on_square_str(piece, sq) + " attacks ";
                for (size_t i = 0; i < fork_targets.size(); i++)
                {
                    if (i > 0)
                        desc += " and ";
                    desc += piece_on_square_str(board[fork_targets[i]], fork_targets[i]);
                }
                t.description = desc;
                result.push_back(t);
            }

            own_iter &= own_iter - 1;
        }
    }

    // -----------------------------------------------------------------------
    // On-board: Pin detection (sliding pieces pinning opponent pieces)
    // -----------------------------------------------------------------------
    {
        U64 sliders =
            board.bitboard(BISHOP | stm) | board.bitboard(ROOK | stm) | board.bitboard(QUEEN | stm);

        while (sliders)
        {
            U8 sq = bit_scan_forward(sliders);
            U8 piece = board[sq];
            U8 pt = piece & ~1;

            U64 slider_attacks = BB_EMPTY;
            if (pt == BISHOP || pt == QUEEN)
                slider_attacks |= MoveGenerator::bishop_targets(1ULL << sq, occupied);
            if (pt == ROOK || pt == QUEEN)
                slider_attacks |= MoveGenerator::rook_targets(1ULL << sq, occupied);

            U64 attacked_opp = slider_attacks & opp_pieces;
            while (attacked_opp)
            {
                U8 front_sq = bit_scan_forward(attacked_opp);
                U8 front_piece = board[front_sq];

                U64 line = lines_along(sq, front_sq);
                if (line != 0)
                {
                    U64 between_slider_front = squares_between(sq, front_sq);
                    U64 occ_without_front = occupied & ~(1ULL << front_sq);

                    U64 extended = BB_EMPTY;
                    if (pt == BISHOP || pt == QUEEN)
                        extended |= MoveGenerator::bishop_targets(1ULL << sq, occ_without_front);
                    if (pt == ROOK || pt == QUEEN)
                        extended |= MoveGenerator::rook_targets(1ULL << sq, occ_without_front);

                    U64 behind_mask =
                        line & ~(1ULL << sq) & ~(1ULL << front_sq) & ~between_slider_front;
                    U64 behind_pieces = extended & behind_mask & opp_pieces;

                    if (behind_pieces)
                    {
                        U8 back_sq = bit_scan_forward(behind_pieces);
                        U8 back_piece = board[back_sq];
                        int front_val = piece_value(front_piece & ~1);
                        int back_val = piece_value(back_piece & ~1);

                        if (front_val < back_val)
                        {
                            Tactic t;
                            t.type = "pin";
                            t.squares = { sq, front_sq, back_sq };
                            t.pieces = { piece_on_square_str(piece, sq),
                                         piece_on_square_str(front_piece, front_sq),
                                         piece_on_square_str(back_piece, back_sq) };
                            t.in_pv = false;
                            t.description = "Pin: " + piece_on_square_str(piece, sq) + " pins "
                                + piece_on_square_str(front_piece, front_sq) + " to "
                                + piece_on_square_str(back_piece, back_sq);
                            result.push_back(t);
                        }
                        else if (front_val > back_val)
                        {
                            Tactic t;
                            t.type = "skewer";
                            t.squares = { sq, front_sq, back_sq };
                            t.pieces = { piece_on_square_str(piece, sq),
                                         piece_on_square_str(front_piece, front_sq),
                                         piece_on_square_str(back_piece, back_sq) };
                            t.in_pv = false;
                            t.description = "Skewer: " + piece_on_square_str(piece, sq)
                                + " skewers " + piece_on_square_str(front_piece, front_sq) + " and "
                                + piece_on_square_str(back_piece, back_sq);
                            result.push_back(t);
                        }
                    }
                }
                attacked_opp &= attacked_opp - 1;
            }

            sliders &= sliders - 1;
        }
    }

    // -----------------------------------------------------------------------
    // On-board: Discovered attack detection
    // -----------------------------------------------------------------------
    {
        U64 sliders_da =
            board.bitboard(BISHOP | stm) | board.bitboard(ROOK | stm) | board.bitboard(QUEEN | stm);

        while (sliders_da)
        {
            U8 slider_sq = bit_scan_forward(sliders_da);
            U8 slider_piece = board[slider_sq];
            U8 slider_pt = slider_piece & ~1;

            // Get slider attack targets
            U64 slider_atk = BB_EMPTY;
            if (slider_pt == BISHOP || slider_pt == QUEEN)
                slider_atk |= MoveGenerator::bishop_targets(1ULL << slider_sq, occupied);
            if (slider_pt == ROOK || slider_pt == QUEEN)
                slider_atk |= MoveGenerator::rook_targets(1ULL << slider_sq, occupied);

            // Find own non-king pieces on the attack ray (potential blockers)
            U64 own_on_ray = slider_atk & (own_pieces & ~board.bitboard(KING | stm));
            while (own_on_ray)
            {
                U8 blocker_sq = bit_scan_forward(own_on_ray);
                U8 blocker_piece = board[blocker_sq];

                // Check the line from slider through blocker
                U64 line = lines_along(slider_sq, blocker_sq);
                if (line == 0)
                {
                    own_on_ray &= own_on_ray - 1;
                    continue;
                }

                // Remove blocker from occupied and recompute slider attacks
                U64 occ_without_blocker = occupied & ~(1ULL << blocker_sq);
                U64 extended_atk = BB_EMPTY;
                if (slider_pt == BISHOP || slider_pt == QUEEN)
                    extended_atk |=
                        MoveGenerator::bishop_targets(1ULL << slider_sq, occ_without_blocker);
                if (slider_pt == ROOK || slider_pt == QUEEN)
                    extended_atk |=
                        MoveGenerator::rook_targets(1ULL << slider_sq, occ_without_blocker);

                // Look for opponent higher-value pieces now attacked on the same line
                U64 behind_blocker = line & ~(1ULL << slider_sq) & ~(1ULL << blocker_sq)
                    & ~squares_between(slider_sq, blocker_sq);
                U64 targets_behind = extended_atk & behind_blocker & opp_pieces;

                while (targets_behind)
                {
                    U8 target_sq = bit_scan_forward(targets_behind);
                    U8 target_piece = board[target_sq];
                    int blocker_val = piece_value(blocker_piece & ~1);
                    int target_val = piece_value(target_piece & ~1);

                    if (target_val > blocker_val)
                    {
                        // Check blocker has at least one move off the line
                        U8 blocker_pt = blocker_piece & ~1;
                        U64 blocker_moves = BB_EMPTY;
                        switch (blocker_pt)
                        {
                            case KNIGHT:
                                blocker_moves = MoveGenerator::knight_targets(1ULL << blocker_sq);
                                break;
                            case BISHOP:
                                blocker_moves =
                                    MoveGenerator::bishop_targets(1ULL << blocker_sq, occupied);
                                break;
                            case ROOK:
                                blocker_moves =
                                    MoveGenerator::rook_targets(1ULL << blocker_sq, occupied);
                                break;
                            case QUEEN:
                                blocker_moves =
                                    MoveGenerator::rook_targets(1ULL << blocker_sq, occupied)
                                    | MoveGenerator::bishop_targets(1ULL << blocker_sq, occupied);
                                break;
                            case PAWN:
                                blocker_moves =
                                    MoveGenerator::pawn_targets(1ULL << blocker_sq, stm);
                                // Also include pawn pushes
                                if (stm == WHITE)
                                    blocker_moves |= ((1ULL << blocker_sq) << 8) & ~occupied;
                                else
                                    blocker_moves |= ((1ULL << blocker_sq) >> 8) & ~occupied;
                                break;
                            default:
                                break;
                        }

                        // Filter out moves that stay on the same line
                        U64 off_line_moves = blocker_moves & ~line & ~own_pieces;
                        if (off_line_moves)
                        {
                            Tactic t;
                            t.type = "discovered_attack";
                            t.squares = { slider_sq, blocker_sq, target_sq };
                            t.pieces = { piece_on_square_str(slider_piece, slider_sq),
                                         piece_on_square_str(blocker_piece, blocker_sq),
                                         piece_on_square_str(target_piece, target_sq) };
                            t.in_pv = false;
                            t.description = "Discovered attack: "
                                + piece_on_square_str(blocker_piece, blocker_sq)
                                + " moves to reveal " + piece_on_square_str(slider_piece, slider_sq)
                                + " attacking " + piece_on_square_str(target_piece, target_sq);
                            result.push_back(t);
                        }
                    }
                    targets_behind &= targets_behind - 1;
                }

                own_on_ray &= own_on_ray - 1;
            }

            sliders_da &= sliders_da - 1;
        }
    }

    // -----------------------------------------------------------------------
    // In-PV: Walk each PV line (first 3-4 moves) and detect tactics
    // -----------------------------------------------------------------------
    for (const auto& pv : pv_lines)
    {
        if (pv.moves.empty())
            continue;

        Board pv_board = board;
        int moves_to_walk = std::min(static_cast<int>(pv.moves.size()), 4);

        for (int i = 0; i < moves_to_walk; i++)
        {
            Move_t m = pv.moves[i];
            U8 from_sq = m.from();
            U8 piece_moved = pv_board[from_sq];
            if (piece_moved == EMPTY)
                break;

            pv_board.do_move(m);

            // After this move, check for forks by the moved piece
            U8 to_sq = m.to();
            U8 moved_pt = piece_moved & ~1;
            U8 moved_side = piece_moved & 1;
            U8 moved_opp = moved_side ^ 1;
            int moved_val = piece_value(moved_pt);

            U64 pv_occupied = BB_EMPTY;
            for (int p = WHITE_PAWN; p <= BLACK_KING; p++)
                pv_occupied |= pv_board.bitboard(p);

            U64 pv_opp_pieces = pv_board.bitboard(PAWN | moved_opp)
                | pv_board.bitboard(KNIGHT | moved_opp) | pv_board.bitboard(BISHOP | moved_opp)
                | pv_board.bitboard(ROOK | moved_opp) | pv_board.bitboard(QUEEN | moved_opp)
                | pv_board.bitboard(KING | moved_opp);

            U64 atk = BB_EMPTY;
            switch (moved_pt)
            {
                case KNIGHT:
                    atk = MoveGenerator::knight_targets(1ULL << to_sq);
                    break;
                case BISHOP:
                    atk = MoveGenerator::bishop_targets(1ULL << to_sq, pv_occupied);
                    break;
                case ROOK:
                    atk = MoveGenerator::rook_targets(1ULL << to_sq, pv_occupied);
                    break;
                case QUEEN:
                    atk = MoveGenerator::rook_targets(1ULL << to_sq, pv_occupied)
                        | MoveGenerator::bishop_targets(1ULL << to_sq, pv_occupied);
                    break;
                case PAWN:
                    atk = MoveGenerator::pawn_targets(1ULL << to_sq, moved_side);
                    break;
                default:
                    break;
            }

            U64 attacked_opp_pv = atk & pv_opp_pieces;
            std::vector<U8> fork_targets_pv;
            U64 tmp2 = attacked_opp_pv;
            while (tmp2)
            {
                U8 tsq = bit_scan_forward(tmp2);
                U8 tp = pv_board[tsq];
                if (piece_value(tp & ~1) > moved_val || (tp & ~1) == KING)
                    fork_targets_pv.push_back(tsq);
                tmp2 &= tmp2 - 1;
            }

            if (fork_targets_pv.size() >= 2)
            {
                Tactic t;
                t.type = "fork";
                t.squares.push_back(to_sq);
                for (U8 ts : fork_targets_pv)
                    t.squares.push_back(ts);
                t.pieces.push_back(piece_on_square_str(piece_moved, to_sq));
                for (U8 ts : fork_targets_pv)
                    t.pieces.push_back(piece_on_square_str(pv_board[ts], ts));
                t.in_pv = true;
                std::string desc =
                    "Fork in PV: " + piece_on_square_str(piece_moved, to_sq) + " attacks ";
                for (size_t j = 0; j < fork_targets_pv.size(); j++)
                {
                    if (j > 0)
                        desc += " and ";
                    desc += piece_on_square_str(pv_board[fork_targets_pv[j]], fork_targets_pv[j]);
                }
                t.description = desc;
                result.push_back(t);
            }

            // Check for discovered attacks in PV
            // The piece that just moved from from_sq — did it unblock a slider?
            {
                // Check own sliders that might have been blocked by the moved piece
                U64 pv_sliders = pv_board.bitboard(BISHOP | moved_side)
                    | pv_board.bitboard(ROOK | moved_side) | pv_board.bitboard(QUEEN | moved_side);

                U64 pv_opp_da = pv_board.bitboard(PAWN | moved_opp)
                    | pv_board.bitboard(KNIGHT | moved_opp) | pv_board.bitboard(BISHOP | moved_opp)
                    | pv_board.bitboard(ROOK | moved_opp) | pv_board.bitboard(QUEEN | moved_opp)
                    | pv_board.bitboard(KING | moved_opp);

                while (pv_sliders)
                {
                    U8 s_sq = bit_scan_forward(pv_sliders);
                    // Was from_sq on a line between this slider and some target?
                    U64 line = lines_along(s_sq, from_sq);
                    if (line != 0)
                    {
                        U8 s_piece = pv_board[s_sq];
                        U8 s_pt = s_piece & ~1;

                        // Compute slider attacks now (piece has moved away from from_sq)
                        U64 s_atk = BB_EMPTY;
                        if (s_pt == BISHOP || s_pt == QUEEN)
                            s_atk |= MoveGenerator::bishop_targets(1ULL << s_sq, pv_occupied);
                        if (s_pt == ROOK || s_pt == QUEEN)
                            s_atk |= MoveGenerator::rook_targets(1ULL << s_sq, pv_occupied);

                        // Check if slider now attacks opponent pieces on the line beyond from_sq
                        U64 beyond_from = line & ~(1ULL << s_sq) & ~squares_between(s_sq, from_sq)
                            & ~(1ULL << from_sq);
                        U64 revealed_targets = s_atk & beyond_from & pv_opp_da;

                        while (revealed_targets)
                        {
                            U8 rt_sq = bit_scan_forward(revealed_targets);
                            U8 rt_piece = pv_board[rt_sq];
                            int rt_val = piece_value(rt_piece & ~1);
                            int moved_val_da = piece_value(moved_pt);

                            if (rt_val > moved_val_da)
                            {
                                Tactic t;
                                t.type = "discovered_attack";
                                t.squares = { s_sq, to_sq, rt_sq };
                                t.pieces = { piece_on_square_str(s_piece, s_sq),
                                             piece_on_square_str(piece_moved, to_sq),
                                             piece_on_square_str(rt_piece, rt_sq) };
                                t.in_pv = true;
                                t.description = "Discovered attack in PV: "
                                    + piece_on_square_str(piece_moved, to_sq) + " moves to reveal "
                                    + piece_on_square_str(s_piece, s_sq) + " attacking "
                                    + piece_on_square_str(rt_piece, rt_sq);
                                result.push_back(t);
                            }
                            revealed_targets &= revealed_targets - 1;
                        }
                    }
                    pv_sliders &= pv_sliders - 1;
                }
            }

            // Check for back-rank mate threat in PV
            if (MoveGenerator::in_check(pv_board, pv_board.side_to_move()))
            {
                U64 checked_king = pv_board.bitboard(KING | pv_board.side_to_move());
                if (checked_king)
                {
                    U8 ksq = bit_scan_forward(checked_king);
                    int krank = ksq / 8;
                    int back = (pv_board.side_to_move() == WHITE) ? 0 : 7;
                    if (krank == back)
                    {
                        Tactic t;
                        t.type = "back_rank_threat";
                        t.squares = { to_sq, ksq };
                        t.pieces = { piece_on_square_str(piece_moved, to_sq),
                                     piece_on_square_str(KING | pv_board.side_to_move(), ksq) };
                        t.in_pv = true;
                        t.description =
                            "Back rank check in PV by " + piece_on_square_str(piece_moved, to_sq);
                        result.push_back(t);
                    }
                }
            }
        }
    }

    return result;
}

// ---------------------------------------------------------------------------
// is_critical_moment()
//
// Compares the eval of the best PV line against the 3rd-best (or last
// available if fewer than 3). If the spread exceeds 100 centipawns, the
// position is critical.
// ---------------------------------------------------------------------------
bool PositionAnalyzer::is_critical_moment(const std::vector<PVLine>& pv_lines, std::string& reason)
{
    if (pv_lines.size() < 2)
        return false;

    int best_eval = pv_lines[0].score;

    // Use 3rd-best line (index 2) if available, otherwise last available
    size_t compare_idx = (pv_lines.size() >= 3) ? 2 : pv_lines.size() - 1;
    int compare_eval = pv_lines[compare_idx].score;

    int spread = std::abs(best_eval - compare_eval);

    if (spread > 100)
    {
        reason = "eval spread between best and ";
        reason += (compare_idx == 2) ? "3rd-best" : "2nd-best";
        reason += " line is ";
        reason += std::to_string(spread);
        reason += "cp";
        return true;
    }

    return false;
}

// ---------------------------------------------------------------------------
// label_line_theme()
//
// Heuristic labeling based on the first 2-3 moves of a PV line.
// ---------------------------------------------------------------------------
std::string PositionAnalyzer::label_line_theme(const Board& board, const std::vector<Move_t>& moves)
{
    if (moves.empty())
        return "general play";

    int moves_to_check = std::min(static_cast<int>(moves.size()), 3);

    bool has_castle = false;
    bool has_check = false;
    bool has_high_value_capture = false;
    bool has_center_pawn_move = false;
    bool has_development = false;

    Board board_copy = board;

    for (int i = 0; i < moves_to_check; i++)
    {
        Move_t m = moves[i];
        U8 from = m.from();
        U8 to = m.to();
        U8 piece_on_from = board_copy[from];

        if (piece_on_from == EMPTY)
            break;

        U8 piece_type = piece_on_from & ~1;  // strip color

        // Check for castling
        if (m.is_castle())
        {
            has_castle = true;
            board_copy.do_move(m);
            continue;
        }

        // Check for captures of high-value pieces (rook or queen)
        if (m.is_capture())
        {
            U8 captured = m.captured();
            U8 cap_type = captured & ~1;
            if (cap_type == ROOK || cap_type == QUEEN)
                has_high_value_capture = true;
        }

        // Check for center pawn moves (pawns moving to d4/d5/e4/e5)
        if (piece_type == PAWN)
        {
            if (to == D4 || to == D5 || to == E4 || to == E5)
                has_center_pawn_move = true;
        }

        // Check for piece development (knight/bishop moving from back rank)
        if (piece_type == KNIGHT || piece_type == BISHOP)
        {
            int from_rank = from / 8;
            U8 color = piece_on_from & 1;
            if ((color == WHITE && from_rank == 0) || (color == BLACK && from_rank == 7))
                has_development = true;
        }

        // Apply the move and check if it gives check
        board_copy.do_move(m);
        U8 opp = board_copy.side_to_move();
        if (MoveGenerator::in_check(board_copy, opp))
            has_check = true;
    }

    // Priority-based labeling
    if (has_check)
        return "king attack";
    if (has_high_value_capture)
        return "material win";
    if (has_castle)
        return "king safety, castling";
    if (has_center_pawn_move)
        return "central pawn break";
    if (has_development)
        return "piece development";

    return "general play";
}
