/*
 * File:   PositionAnalyzer.h
 *
 * Stateless utility that extracts structured analysis data from a board
 * position: evaluation breakdown, hanging pieces, threats, pawn structure,
 * king safety, threat map, tactics, and critical moment detection.
 */

#ifndef POSITIONANALYZER_H
#define POSITIONANALYZER_H

#include <string>
#include <vector>

#include "Common.h"
#include "Board.h"
#include "Search.h"
#include "Move.h"

struct EvalBreakdown {
    int material;
    int mobility;
    int king_safety;
    int pawn_structure;
    int tempo;
    int piece_bonuses;
};

struct HangingPiece {
    U8 square;
    U8 piece;  // piece type without color (PAWN, KNIGHT, etc.)
};

struct Threat {
    std::string type;        // "check", "capture", "fork", "pin", "skewer", "discovered_attack"
    U8 source_square;
    std::vector<U8> target_squares;
    std::string uci_move;       // e.g. "c4f7", "f3d6"
    std::string description;
};

struct PawnFeatures {
    std::vector<int> isolated;  // file indices (0-7)
    std::vector<int> doubled;
    std::vector<int> passed;
};

struct KingSafety {
    int score;
    std::string description;
};

struct ThreatMapEntry {
    U8 square;
    U8 piece;           // EMPTY if no piece
    int white_attackers;
    int black_attackers;
    int white_defenders;
    int black_defenders;
    bool net_attacked;
};

struct Tactic {
    std::string type;    // "fork", "pin", "skewer", etc.
    std::vector<U8> squares;
    std::vector<std::string> pieces;  // e.g. "Nc7", "Ra8"
    bool in_pv;
    std::string description;
};

struct PositionReport {
    std::string fen;
    int eval_cp;
    EvalBreakdown breakdown;
    std::vector<HangingPiece> hanging_white;
    std::vector<HangingPiece> hanging_black;
    std::vector<Threat> threats_white;
    std::vector<Threat> threats_black;
    PawnFeatures pawns_white;
    PawnFeatures pawns_black;
    KingSafety king_safety_white;
    KingSafety king_safety_black;
    std::vector<PVLine> top_lines;  // reuses PVLine from Search.h
    std::vector<Tactic> tactics;
    std::vector<ThreatMapEntry> threat_map;
    std::string threat_map_summary;
    bool critical_moment;
    std::string critical_reason;  // empty when not critical
};

class PositionAnalyzer {
public:
    // Full position analysis (used by coach eval)
    static PositionReport analyze(const Board& board,
                                  const std::vector<PVLine>& pv_lines);

    // Individual analysis components (testable in isolation)
    static EvalBreakdown compute_eval_breakdown(const Board& board);
    static std::vector<HangingPiece> find_hanging_pieces(const Board& board, U8 side);
    static std::vector<Threat> find_threats(const Board& board, U8 side);
    static PawnFeatures analyze_pawns(const Board& board, U8 side);
    static KingSafety assess_king_safety(const Board& board, U8 side);
    static std::vector<ThreatMapEntry> build_threat_map(const Board& board);
    static std::vector<Tactic> detect_tactics(const Board& board,
                                               const std::vector<PVLine>& pv_lines);
    static bool is_critical_moment(const std::vector<PVLine>& pv_lines,
                                    std::string& reason);
    static std::string label_line_theme(const Board& board, const std::vector<Move_t>& moves);
};

#endif /* POSITIONANALYZER_H */
