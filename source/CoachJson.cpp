/*
 * File:   CoachJson.cpp
 *
 * Lightweight JSON serialization utilities for the Coaching Protocol.
 */

#include <sstream>

#include "CoachJson.h"

#include "Constants.h"
#include "Move.h"
#include "MoveComparator.h"
#include "Output.h"
#include "PositionAnalyzer.h"

namespace CoachJson
{
std::string to_json(int value)
{
    return std::to_string(value);
}

std::string to_json(bool value)
{
    return value ? "true" : "false";
}

std::string to_json(const std::string& value)
{
    std::string result;
    result.reserve(value.size() + 2);
    result += '"';
    for (char c : value)
    {
        switch (c)
        {
            case '"':
                result += "\\\"";
                break;
            case '\\':
                result += "\\\\";
                break;
            case '\b':
                result += "\\b";
                break;
            case '\f':
                result += "\\f";
                break;
            case '\n':
                result += "\\n";
                break;
            case '\r':
                result += "\\r";
                break;
            case '\t':
                result += "\\t";
                break;
            default:
                if (static_cast<unsigned char>(c) < 0x20)
                {
                    // Control characters: encode as \u00XX
                    char buf[7];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                    result += buf;
                }
                else
                {
                    result += c;
                }
                break;
        }
    }
    result += '"';
    return result;
}

std::string to_json_null()
{
    return "null";
}

std::string to_json(const char* value)
{
    return to_json(std::string(value));
}

std::string array(const std::vector<std::string>& elements)
{
    std::string result = "[";
    for (size_t i = 0; i < elements.size(); ++i)
    {
        if (i > 0)
            result += ',';
        result += elements[i];
    }
    result += ']';
    return result;
}

std::string object(const std::vector<std::pair<std::string, std::string>>& fields)
{
    std::string result = "{";
    for (size_t i = 0; i < fields.size(); ++i)
    {
        if (i > 0)
            result += ',';
        result += '"';
        result += fields[i].first;
        result += "\":";
        result += fields[i].second;
    }
    result += '}';
    return result;
}

// ---------------------------------------------------------------------------
// Internal helpers (not exposed in header)
// ---------------------------------------------------------------------------

static std::string piece_type_to_json(U8 piece_type)
{
    // piece_type is without color bit (PAWN=2, KNIGHT=4, etc.)
    switch (piece_type)
    {
        case PAWN:
            return CoachJson::to_json(std::string("pawn"));
        case KNIGHT:
            return CoachJson::to_json(std::string("knight"));
        case BISHOP:
            return CoachJson::to_json(std::string("bishop"));
        case ROOK:
            return CoachJson::to_json(std::string("rook"));
        case QUEEN:
            return CoachJson::to_json(std::string("queen"));
        case KING:
            return CoachJson::to_json(std::string("king"));
        default:
            return CoachJson::to_json_null();
    }
}

static std::string square_to_json(U8 sq)
{
    return CoachJson::to_json(Output::square(sq));
}

static std::string file_index_to_json(int file_idx)
{
    std::string s(1, static_cast<char>('a' + file_idx));
    return CoachJson::to_json(s);
}

/// Convert a Move_t to UCI notation string.
/// `side` is the side to move when this move is played (needed for castling).
static std::string move_to_uci(Move_t move, U8 side)
{
    if (move == Move(0U))
        return "0000";

    if (move.is_castle())
    {
        if (move == build_castle(KING_CASTLE))
        {
            return (side == WHITE) ? "e1g1" : "e8g8";
        }
        else
        {
            return (side == WHITE) ? "e1c1" : "e8c8";
        }
    }

    std::string result;
    U8 from = move.from();
    U8 to = move.to();
    result += static_cast<char>('a' + (from & 7));
    result += static_cast<char>('1' + (from >> 3));
    result += static_cast<char>('a' + (to & 7));
    result += static_cast<char>('1' + (to >> 3));

    if (move.is_promotion())
    {
        U8 promo = move.promote_to();
        switch (promo)
        {
            case QUEEN:
                result += 'q';
                break;
            case ROOK:
                result += 'r';
                break;
            case BISHOP:
                result += 'b';
                break;
            case KNIGHT:
                result += 'n';
                break;
            default:
                result += 'q';
                break;
        }
    }
    return result;
}

/// Serialize a vector of PVLine to a JSON array of top_line objects.
/// `side` is the side to move at the root position.
static std::string serialize_top_lines(const std::vector<PVLine>& lines, U8 side)
{
    std::vector<std::string> elems;
    elems.reserve(lines.size());
    for (const auto& pv : lines)
    {
        // Convert moves to UCI strings
        std::vector<std::string> move_strs;
        move_strs.reserve(pv.moves.size());
        U8 current_side = side;
        for (const auto& m : pv.moves)
        {
            move_strs.push_back(CoachJson::to_json(move_to_uci(m, current_side)));
            current_side ^= 1;  // alternate sides
        }

        std::string line_obj =
            CoachJson::object({ { "depth", CoachJson::to_json(static_cast<int>(pv.moves.size())) },
                                { "eval_cp", CoachJson::to_json(pv.score) },
                                { "moves", CoachJson::array(move_strs) },
                                { "theme", CoachJson::to_json(std::string("")) } });
        elems.push_back(line_obj);
    }
    return CoachJson::array(elems);
}

/// Serialize a vector of Tactic to a JSON array.
static std::string serialize_tactics(const std::vector<Tactic>& tactics)
{
    std::vector<std::string> elems;
    elems.reserve(tactics.size());
    for (const auto& t : tactics)
    {
        std::vector<std::string> sq_strs;
        sq_strs.reserve(t.squares.size());
        for (U8 sq : t.squares)
        {
            sq_strs.push_back(square_to_json(sq));
        }

        std::vector<std::string> piece_strs;
        piece_strs.reserve(t.pieces.size());
        for (const auto& p : t.pieces)
        {
            piece_strs.push_back(CoachJson::to_json(p));
        }

        std::string obj =
            CoachJson::object({ { "type", CoachJson::to_json(t.type) },
                                { "squares", CoachJson::array(sq_strs) },
                                { "pieces", CoachJson::array(piece_strs) },
                                { "in_pv", CoachJson::to_json(t.in_pv) },
                                { "description", CoachJson::to_json(t.description) } });
        elems.push_back(obj);
    }
    return CoachJson::array(elems);
}

// ---------------------------------------------------------------------------
// High-level serializers
// ---------------------------------------------------------------------------

std::string serialize_pong(const std::string& engine_name, const std::string& engine_version)
{
    return object({ { "status", to_json(std::string("ok")) },
                    { "engine_name", to_json(engine_name) },
                    { "engine_version", to_json(engine_version) } });
}

std::string serialize_error(const std::string& code, const std::string& message)
{
    return object({ { "code", to_json(code) }, { "message", to_json(message) } });
}

std::string serialize_position_report(const PositionReport& r)
{
    // eval_breakdown
    std::string breakdown = object({ { "material", to_json(r.breakdown.material) },
                                     { "mobility", to_json(r.breakdown.mobility) },
                                     { "king_safety", to_json(r.breakdown.king_safety) },
                                     { "pawn_structure", to_json(r.breakdown.pawn_structure) } });

    // hanging_pieces helper lambda
    auto serialize_hanging = [](const std::vector<HangingPiece>& pieces)
    {
        std::vector<std::string> elems;
        elems.reserve(pieces.size());
        for (const auto& hp : pieces)
        {
            elems.push_back(object({ { "square", square_to_json(hp.square) },
                                     { "piece", piece_type_to_json(hp.piece) } }));
        }
        return array(elems);
    };

    std::string hanging = object({ { "white", serialize_hanging(r.hanging_white) },
                                   { "black", serialize_hanging(r.hanging_black) } });

    // threats helper
    auto serialize_threats = [](const std::vector<Threat>& threats)
    {
        std::vector<std::string> elems;
        elems.reserve(threats.size());
        for (const auto& t : threats)
        {
            std::vector<std::string> tgt_strs;
            tgt_strs.reserve(t.target_squares.size());
            for (U8 sq : t.target_squares)
            {
                tgt_strs.push_back(square_to_json(sq));
            }
            elems.push_back(object({ { "type", to_json(t.type) },
                                     { "source_square", square_to_json(t.source_square) },
                                     { "target_squares", array(tgt_strs) },
                                     { "description", to_json(t.description) } }));
        }
        return array(elems);
    };

    std::string threats = object({ { "white", serialize_threats(r.threats_white) },
                                   { "black", serialize_threats(r.threats_black) } });

    // pawn_structure helper
    auto serialize_pawn_features = [](const PawnFeatures& pf)
    {
        auto file_array = [](const std::vector<int>& files)
        {
            std::vector<std::string> elems;
            elems.reserve(files.size());
            for (int f : files)
            {
                elems.push_back(file_index_to_json(f));
            }
            return array(elems);
        };
        return object({ { "isolated", file_array(pf.isolated) },
                        { "doubled", file_array(pf.doubled) },
                        { "passed", file_array(pf.passed) } });
    };

    std::string pawn_structure = object({ { "white", serialize_pawn_features(r.pawns_white) },
                                          { "black", serialize_pawn_features(r.pawns_black) } });

    // king_safety
    auto serialize_ks = [](const KingSafety& ks) {
        return object(
            { { "score", to_json(ks.score) }, { "description", to_json(ks.description) } });
    };

    std::string king_safety = object({ { "white", serialize_ks(r.king_safety_white) },
                                       { "black", serialize_ks(r.king_safety_black) } });

    // Determine side to move from FEN (character after first space)
    U8 side = WHITE;
    {
        auto sp = r.fen.find(' ');
        if (sp != std::string::npos && sp + 1 < r.fen.size())
        {
            side = (r.fen[sp + 1] == 'b') ? BLACK : WHITE;
        }
    }

    // top_lines
    std::string top_lines = serialize_top_lines(r.top_lines, side);

    // tactics
    std::string tactics = serialize_tactics(r.tactics);

    // threat_map
    std::vector<std::string> tm_elems;
    tm_elems.reserve(r.threat_map.size());
    for (const auto& entry : r.threat_map)
    {
        tm_elems.push_back(object(
            { { "square", square_to_json(entry.square) },
              { "piece",
                (entry.piece == EMPTY) ? to_json_null() : piece_type_to_json(entry.piece & 0xFE) },
              { "white_attackers", to_json(entry.white_attackers) },
              { "black_attackers", to_json(entry.black_attackers) },
              { "white_defenders", to_json(entry.white_defenders) },
              { "black_defenders", to_json(entry.black_defenders) },
              { "net_attacked", to_json(entry.net_attacked) } }));
    }
    std::string threat_map = array(tm_elems);

    // critical_reason: null when not critical
    std::string critical_reason_json =
        r.critical_moment ? to_json(r.critical_reason) : to_json_null();

    return object({ { "fen", to_json(r.fen) },
                    { "eval_cp", to_json(r.eval_cp) },
                    { "eval_breakdown", breakdown },
                    { "hanging_pieces", hanging },
                    { "threats", threats },
                    { "pawn_structure", pawn_structure },
                    { "king_safety", king_safety },
                    { "top_lines", top_lines },
                    { "tactics", tactics },
                    { "threat_map", threat_map },
                    { "critical_moment", to_json(r.critical_moment) },
                    { "critical_reason", critical_reason_json } });
}

std::string serialize_comparison_report(const ComparisonReport& r)
{
    // Determine side to move from FEN
    U8 side = WHITE;
    {
        auto sp = r.fen.find(' ');
        if (sp != std::string::npos && sp + 1 < r.fen.size())
        {
            side = (r.fen[sp + 1] == 'b') ? BLACK : WHITE;
        }
    }

    // refutation_line: null when not a blunder, otherwise array of UCI strings
    std::string refutation_json;
    if (r.refutation_line.empty())
    {
        refutation_json = to_json_null();
    }
    else
    {
        std::vector<std::string> ref_strs;
        ref_strs.reserve(r.refutation_line.size());
        for (const auto& m : r.refutation_line)
        {
            ref_strs.push_back(to_json(m));
        }
        refutation_json = array(ref_strs);
    }

    // missed_tactics
    std::string missed = serialize_tactics(r.missed_tactics);

    // top_lines
    std::string top_lines = serialize_top_lines(r.top_lines, side);

    // critical_reason
    std::string critical_reason_json =
        r.critical_moment ? to_json(r.critical_reason) : to_json_null();

    return object({ { "fen", to_json(r.fen) },
                    { "user_move", to_json(r.user_move) },
                    { "user_eval_cp", to_json(r.user_eval_cp) },
                    { "best_move", to_json(r.best_move) },
                    { "best_eval_cp", to_json(r.best_eval_cp) },
                    { "eval_drop_cp", to_json(r.eval_drop_cp) },
                    { "classification", to_json(r.classification) },
                    { "nag", to_json(r.nag) },
                    { "best_move_idea", to_json(r.best_move_idea) },
                    { "refutation_line", refutation_json },
                    { "missed_tactics", missed },
                    { "top_lines", top_lines },
                    { "critical_moment", to_json(r.critical_moment) },
                    { "critical_reason", critical_reason_json } });
}

}  // namespace CoachJson
