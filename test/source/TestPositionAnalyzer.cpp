/*
 * File:   TestPositionAnalyzer.cpp
 *
 * Regression tests for the coaching position analyzer: threat legality,
 * discovered-attack self-reference, back-rank labeling, the discovered-attack
 * squares contract, and the pin/skewer quality filter.
 */

#include <catch2/catch_test_macros.hpp>

#include "Evaluator.h"
#include "PositionAnalyzer.h"
#include "Tests.h"

namespace
{
// --- Endgame probes (all four are phase 0: no queens, no more than one rook
// or two bishops of material on the board) ---

// Drawn K+P vs K. A pawn push gives "check" to the black king, which used to
// come back as the theme "king attack".
const char* KPK_DRAW_FEN = "8/8/4k3/8/4P3/4K3/8/8 w - - 0 1";
// Rook endgame, White a whole rook up.
const char* ROOK_EG_FEN = "8/5p1k/6p1/7p/8/6PK/5P1P/1R6 w - - 0 1";
// Blocked queenside pawns, kings racing.
const char* PAWN_EG_FEN = "8/2p5/8/1P6/8/5k2/8/5K2 w - - 0 1";
// Opposite-colored bishops, level.
const char* OPP_BISHOPS_FEN = "8/4k3/3b4/8/8/3B4/4K3/8 w - - 0 1";

// --- Non-endgame probes ---
const char* START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
// Middlegame, both sides fully developed, White castled.
const char* MIDDLEGAME_FEN = "r1bq1r1k/pp2nppp/2n5/2bpp3/4P3/2PP1N2/PP1NBPPP/R1BQ1RK1 w - - 0 11";
// Italian-ish opening position: White's king still on e1 with the e-pawn on e4,
// so the e-file shield in front of the king is missing.
const char* NO_SHIELD_MG_FEN =
    "r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/2N2N2/PPPP1PPP/R1BQK2R w KQkq - 6 5";

// White to move and IN CHECK after 1.e4 e6 2.Nc3 Qg5 3.d4 Qg6 4.Nf3 Nf6
// 5.e5 Bb4 6.exf6 Qe4+. Nc3 is pinned to Ke1 by Bb4; White's only legal moves
// are Kd2, Be2, Qe2, Be3.
const char* QE4_CHECK_FEN = "rnb1k2r/pppp1ppp/4pP2/8/1b1Pq3/2N2N2/PPP2PPP/R1BQK2R w KQkq - 1 7";

// After 1.e4 e6 2.Nc3 Qg5, White to move. The d2 pawn blocks Bc1's diagonal to
// the black queen on g5; pushing it is a discovered attack. Qg5 also lies on
// lines through the d2 and g2 pawns (the spurious "pins" we now filter out).
const char* QG5_FEN = "rnb1kbnr/pppp1ppp/4p3/6q1/4P3/2N5/PPPP1PPP/R1BQKB1R w KQkq - 2 3";

bool has_uci(const std::vector<Threat>& threats, const std::string& uci)
{
    for (const auto& t : threats)
        if (t.uci_move == uci)
            return true;
    return false;
}

int count_type(const std::vector<Tactic>& tactics, const std::string& type)
{
    int n = 0;
    for (const auto& t : tactics)
        if (t.type == type)
            n++;
    return n;
}

// Label the line given as SAN from `fen`. Each move is applied so the next SAN
// is parsed in the position it belongs to.
std::string theme_of(const char* fen, const std::vector<std::string>& sans)
{
    Board board = Parser::parse_fen(fen);
    Board walker = board;
    std::vector<Move_t> moves;
    for (const auto& san : sans)
    {
        auto mv = Parser::parse_san(san, walker);
        INFO("unparsable SAN in test line: " << san);
        REQUIRE(mv.has_value());
        moves.push_back(*mv);
        walker.do_move(*mv);
    }
    return PositionAnalyzer::label_line_theme(board, moves);
}

bool mentions(const std::string& haystack, const std::string& needle)
{
    return haystack.find(needle) != std::string::npos;
}
}  // namespace

TEST_CASE("threats are legal moves for the side", "[position analyzer]")
{
    Board board = Parser::parse_fen(QE4_CHECK_FEN);
    auto threats = PositionAnalyzer::find_threats(board, WHITE);

    // The two pseudo-legal captures that leaked before the legality filter:
    // a pinned knight "capturing" the checking queen, and a pawn capture that
    // ignores the check.
    REQUIRE_FALSE(has_uci(threats, "c3e4"));  // Nc3 is pinned
    REQUIRE_FALSE(has_uci(threats, "f6g7"));  // ignores the check

    // Invariant: every capture/check threat must be an actually legal move.
    MoveList legal;
    MoveGenerator::add_all_moves(legal, board, WHITE);
    U64 legal_dest[NUM_SQUARES] = { BB_EMPTY };
    for (int i = 0; i < legal.length(); i++)
        legal_dest[legal[i].from()] |= (1ULL << legal[i].to());

    for (const auto& t : threats)
    {
        if (t.type == "capture" || t.type == "check")
        {
            INFO("illegal threat leaked: " << t.description);
            REQUIRE((legal_dest[t.source_square] & (1ULL << t.target_squares[0])) != 0);
        }
    }
}

TEST_CASE("king safety exposes structured fields for coaching", "[position analyzer]")
{
    // Starting position: both kings home with castling rights and a solid shield.
    Board start = Parser::parse_fen(START_FEN);
    KingSafety ks = PositionAnalyzer::assess_king_safety(start, WHITE);
    REQUIRE(ks.king_square == "e1");
    REQUIRE(ks.castling_status == "uncastled_with_rights");
    REQUIRE(ks.missing_shield_files.empty());
    REQUIRE_FALSE(ks.open_file_near_king);

    // Middlegame with the e-pawn pushed: the shield in front of e1 is gone and
    // the shield fields must still report it.
    Board exposed = Parser::parse_fen(NO_SHIELD_MG_FEN);
    REQUIRE_FALSE(is_endgame_phase(exposed));
    KingSafety mks = PositionAnalyzer::assess_king_safety(exposed, WHITE);
    REQUIRE(mks.missing_shield_files == std::vector<std::string> { "e" });
    REQUIRE(mks.score < 0);
    REQUIRE(mentions(mks.description, "missing e-pawn shield"));

    // Bare K+R vs K: the king square and castling status are still reported,
    // but nothing may claim shield danger — see the endgame test below.
    Board endgame = Parser::parse_fen("8/8/8/4k3/8/8/4K3/4R3 w - - 0 1");
    KingSafety eks = PositionAnalyzer::assess_king_safety(endgame, WHITE);
    REQUIRE(eks.king_square == "e2");
    REQUIRE(eks.castling_status == "displaced");
    REQUIRE(eks.missing_shield_files.empty());
    REQUIRE_FALSE(eks.open_file_near_king);
}

TEST_CASE("endgame king safety prose agrees with the phase-gated score", "[position analyzer]")
{
    // The evaluator scores king safety as 0 below KING_SAFETY_PHASE_THRESHOLD.
    // The words and the structured flags must not contradict that number by
    // reporting a missing pawn shield, an open file or a pawn storm.
    for (const char* fen : { KPK_DRAW_FEN, ROOK_EG_FEN, PAWN_EG_FEN, OPP_BISHOPS_FEN })
    {
        INFO("fen: " << fen);
        Board board = Parser::parse_fen(fen);
        REQUIRE(is_endgame_phase(board));
        REQUIRE(PositionAnalyzer::compute_eval_breakdown(board).king_safety == 0);

        for (U8 side : { WHITE, BLACK })
        {
            KingSafety ks = PositionAnalyzer::assess_king_safety(board, side);
            REQUIRE(ks.score == 0);
            REQUIRE(ks.missing_shield_files.empty());
            REQUIRE_FALSE(ks.open_file_near_king);
            REQUIRE_FALSE(ks.pawn_storm);
            // Describes activity, not danger.
            REQUIRE(mentions(ks.description, "endgame king on "));
            REQUIRE_FALSE(mentions(ks.description, "shield"));
            REQUIRE_FALSE(mentions(ks.description, "open file"));
            REQUIRE_FALSE(mentions(ks.description, "pawn storm"));
        }
    }

    // The activity clauses come from the board: a king near the center reads as
    // centralized, a king on the rim does not.
    Board kpk = Parser::parse_fen(KPK_DRAW_FEN);
    REQUIRE(mentions(PositionAnalyzer::assess_king_safety(kpk, WHITE).description, "centralized"));

    Board rook_eg = Parser::parse_fen(ROOK_EG_FEN);
    KingSafety rks = PositionAnalyzer::assess_king_safety(rook_eg, WHITE);
    REQUIRE(mentions(rks.description, "far from the center"));
    // Kh3 is two king-steps from the h5 pawn.
    REQUIRE(mentions(rks.description, "2 squares from the nearest enemy pawn"));
}

TEST_CASE("endgame lines get endgame themes", "[position analyzer]")
{
    // Drawn KPK. The old classifier called this "king attack" because e4-e5
    // gives check to the king on d6.
    std::string kpk = theme_of(KPK_DRAW_FEN, { "Kf4", "Kd6", "e5" });
    REQUIRE(kpk != "king attack");
    REQUIRE(kpk == "passed pawn push");

    // Rook endgame: Rb6 confines the black king to the last two ranks.
    REQUIRE(theme_of(ROOK_EG_FEN, { "Kg2", "Kg7", "Rb6" }) == "rook cuts the king off");

    // Same position, a line of pure king play.
    REQUIRE(theme_of(ROOK_EG_FEN, { "Kh4", "Kg7", "Kg5" }) == "king activity");

    // Blocked pawns: the king walk toward the center is the whole idea.
    REQUIRE(theme_of(PAWN_EG_FEN, { "Ke1", "Ke3", "Kf1" }) == "king activity");

    // Opposite-colored bishops: Kd3 centralizes.
    REQUIRE(theme_of(OPP_BISHOPS_FEN, { "Be4", "Be5", "Kd3" }) == "king activity");

    // Both sides have a passer and one of them pushes: a race.
    REQUIRE(theme_of("8/6p1/8/8/8/8/1P6/K6k w - - 0 1", { "b4" }) == "pawn race");

    // Promotion outranks everything else.
    REQUIRE(theme_of("8/1P6/8/7k/8/8/8/K7 w - - 0 1", { "b8=Q" }) == "promotion");

    // Rook dropping in behind its own passed pawn (Tarrasch).
    REQUIRE(theme_of("8/8/8/8/P7/6k1/8/1R4K1 w - - 0 1", { "Ra1" }) == "rook behind the passer");

    // A rook up, so mopping up the last pawn is conversion, not a "material win"
    // (nothing bigger than a pawn is taken).
    REQUIRE(theme_of("8/5p2/8/8/8/5R2/8/K6k w - - 0 1", { "Rxf7" })
            == "conversion, simplification");

    // Winning a rook is still a material win in an endgame.
    REQUIRE(theme_of("8/8/8/4r3/8/4R3/8/K6k w - - 0 1", { "Rxe5" }) == "material win");
}

TEST_CASE("opening and middlegame themes are unaffected", "[position analyzer]")
{
    Board start = Parser::parse_fen(START_FEN);
    Board middlegame = Parser::parse_fen(MIDDLEGAME_FEN);
    REQUIRE_FALSE(is_endgame_phase(start));
    REQUIRE_FALSE(is_endgame_phase(middlegame));

    REQUIRE(theme_of(START_FEN, { "Nc3", "Nc6", "Nf3" }) == "piece development");
    REQUIRE(theme_of(START_FEN, { "e4", "e5", "Nf3" }) == "central pawn break");
    REQUIRE(theme_of(MIDDLEGAME_FEN, { "exd5", "Nxd5", "d4" }) == "central pawn break");
    REQUIRE(theme_of(MIDDLEGAME_FEN, { "b4", "Bd6", "b5" }) == "general play");

    // Castling and development are middlegame labels only.
    REQUIRE(theme_of("r1bqk2r/pppp1ppp/2n2n2/2b1p3/2B1P3/2N2N2/PPPP1PPP/R1BQK2R w KQkq - 6 5",
                     { "O-O" })
            == "king safety, castling");

    // The same knight sortie in an endgame is not "piece development".
    std::string eg_knight = theme_of("8/8/4k3/8/8/8/8/1N2K3 w - - 0 1", { "Nc3" });
    REQUIRE(eg_knight != "piece development");
    REQUIRE(eg_knight == "general play");
}
TEST_CASE("discovered attack uses [slider, target, mover] squares", "[position analyzer]")
{
    Board board = Parser::parse_fen(QG5_FEN);
    auto tactics = PositionAnalyzer::detect_tactics(board, {});

    const Tactic* da = nullptr;
    for (const auto& t : tactics)
        if (t.type == "discovered_attack")
            da = &t;

    REQUIRE(da != nullptr);
    REQUIRE(da->squares.size() == 3);
    REQUIRE(da->squares[0] == C1);  // revealed attacker (bishop)
    REQUIRE(da->squares[1] == G5);  // attacked target (queen)
    REQUIRE(da->squares[2] == D2);  // moving piece (pawn)
    // The mover and the revealed slider must be different pieces.
    REQUIRE(da->squares[0] != da->squares[2]);
    REQUIRE(da->description == "Discovered attack (White): d2 moves to reveal Bc1 attacking Qg5");
}

TEST_CASE("no self-referencing discovered attack in PV", "[position analyzer]")
{
    // Black to move; ...Bxc3 lands the bishop on c3 giving a *direct* diagonal
    // check to Ke1. The old PV scan reported "Bc3 moves to reveal Bc3".
    Board board = Parser::parse_fen("4k3/8/8/8/1b6/2N5/8/4K3 b - - 0 1");
    auto mv = Parser::parse_san("Bxc3", board);
    REQUIRE(mv.has_value());

    PVLine pv;
    pv.moves.push_back(*mv);
    auto tactics = PositionAnalyzer::detect_tactics(board, { pv });

    REQUIRE(count_type(tactics, "discovered_attack") == 0);
    // A diagonal check must not be labeled a back-rank threat.
    REQUIRE(count_type(tactics, "back_rank_threat") == 0);
}

TEST_CASE("genuine back-rank check is still detected in PV", "[position analyzer]")
{
    // Re8+ is a real rook back-rank check against a king walled in by pawns.
    Board board = Parser::parse_fen("6k1/5ppp/8/8/8/8/8/4R1K1 w - - 0 1");
    auto mv = Parser::parse_san("Re8+", board);
    REQUIRE(mv.has_value());

    PVLine pv;
    pv.moves.push_back(*mv);
    auto tactics = PositionAnalyzer::detect_tactics(board, { pv });

    bool found_pv_back_rank = false;
    for (const auto& t : tactics)
        if (t.type == "back_rank_threat" && t.in_pv)
            found_pv_back_rank = true;
    REQUIRE(found_pv_back_rank);
}

TEST_CASE("pawn pins to a non-king piece are filtered out", "[position analyzer]")
{
    Board board = Parser::parse_fen(QG5_FEN);
    auto threats = PositionAnalyzer::find_threats(board, BLACK);

    // "Qg5 pins d2 to Bc1" and "Qg5 pins g2 to Ng1" pinned a pawn to a piece —
    // low-value noise that should no longer be reported.
    for (const auto& t : threats)
    {
        if (t.type == "pin")
        {
            U8 front_sq = t.target_squares[0];
            U8 front_piece = board[front_sq];
            U8 back_sq = t.target_squares[1];
            U8 back_piece = board[back_sq];
            bool front_is_pawn = (front_piece & ~1) == PAWN;
            bool back_is_king = (back_piece & ~1) == KING;
            INFO("low-value pin leaked: " << t.description);
            REQUIRE_FALSE((front_is_pawn && !back_is_king));
        }
    }
}
