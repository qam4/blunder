/*
 * File:   TestPositionAnalyzer.cpp
 *
 * Regression tests for the coaching position analyzer: threat legality,
 * discovered-attack self-reference, back-rank labelling, the discovered-attack
 * squares contract, and the pin/skewer quality filter.
 */

#include "Tests.h"

#include <catch2/catch_test_macros.hpp>

#include "PositionAnalyzer.h"

namespace
{
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
    REQUIRE(da->description == "Discovered attack: d2 moves to reveal Bc1 attacking Qg5");
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
    // A diagonal check must not be labelled a back-rank threat.
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
