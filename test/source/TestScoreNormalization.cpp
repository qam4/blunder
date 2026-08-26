/*
 * File:   TestScoreNormalization.cpp
 *
 * Unit tests for the UCI/coaching-protocol score normalization at the output
 * boundary (normalize_score_cp in Constants.h) and for the eval-drop bands that
 * consume normalized units (MoveComparator::classify / ::compute_nag).
 *
 * Why these are worth pinning: normalize_score_cp rescales EVERY score the engine
 * reports, on both the UCI info line and every *_cp field of the coaching JSON.
 * A consumer (chess-coach) reads those as conventional centipawns and derives
 * student-facing decisions from them, so a regression here is silent and
 * downstream. The mate pass-through is the sharpest edge: the engine signals mate
 * by printing a near-MATE_SCORE magnitude on the "score cp" line rather than
 * "score mate", so if normalization ever leaked onto those values a mate would
 * arrive at the consumer as an ordinary large evaluation.
 */

#include <catch2/catch_test_macros.hpp>

#include "Constants.h"
#include "MoveComparator.h"

TEST_CASE("normalize_pawn_maps_to_one_hundred", "[normalization]")
{
    // The contract: NORMALIZE_TO_PAWN internal units == one pawn == 100 cp.
    REQUIRE(normalize_score_cp(NORMALIZE_TO_PAWN) == 100);
    REQUIRE(normalize_score_cp(2 * NORMALIZE_TO_PAWN) == 200);
    REQUIRE(normalize_score_cp(NORMALIZE_TO_PAWN / 2) == 50);
}

TEST_CASE("normalize_documents_the_residual_taper", "[normalization]")
{
    // A single fixed divisor cannot be exact for a phase-tapered evaluation: the
    // pawn in PIECE_VALUE_BONUS is 124 (middlegame) and 206 (endgame), and
    // NORMALIZE_TO_PAWN sits at the endgame end. So the endgame is very nearly
    // right and the middlegame under-reports by about a third.
    //
    // Asserted rather than merely commented so the trade-off is visible: if
    // NORMALIZE_TO_PAWN is ever changed, this test says out loud which phase the
    // new value favors instead of letting it pass silently.
    constexpr int MG_PAWN = 124;
    constexpr int EG_PAWN = 206;
    REQUIRE(normalize_score_cp(EG_PAWN) == 103);  // endgame: within 3% of a pawn
    REQUIRE(normalize_score_cp(MG_PAWN) == 62);   // middlegame: ~38% under
}

TEST_CASE("normalize_keeps_a_draw_a_draw", "[normalization]")
{
    // DRAW_SCORE must survive untouched. A rounding scheme that nudged 0 off zero
    // would turn every drawn line into a small advantage for one side.
    REQUIRE(normalize_score_cp(0) == 0);
    REQUIRE(normalize_score_cp(DRAW_SCORE) == DRAW_SCORE);
}

TEST_CASE("normalize_is_sign_symmetric", "[normalization]")
{
    // normalize(-x) == -normalize(x) for every x. Asymmetric rounding would bias
    // the engine toward one color, which is the kind of defect that hides for
    // months because each individual score still looks plausible.
    const int values[] = { 1, 3, 7, 25, 99, 100, 101, 199, 200, 201, 1234, 50000 };
    for (int v : values)
    {
        REQUIRE(normalize_score_cp(-v) == -normalize_score_cp(v));
    }
}

TEST_CASE("normalize_preserves_move_ordering", "[normalization]")
{
    // Monotonicity. The consumer no longer trusts the MAGNITUDE of these scores
    // but does rely on their ORDER (which candidate move is better), so a rounding
    // bug that reordered two nearly-equal lines would matter more than one that
    // shifted both. Steps of 1 across a rounding boundary are the interesting part.
    int previous = normalize_score_cp(-300);
    for (int raw = -299; raw <= 300; ++raw)
    {
        const int current = normalize_score_cp(raw);
        REQUIRE(current >= previous);
        previous = current;
    }
}

TEST_CASE("normalize_passes_mate_scores_through_untouched", "[normalization]")
{
    // Mate is signaled as a near-MATE_SCORE magnitude on the "score cp" line, so
    // these must NOT be divided. Halving a mate score would leave a value that no
    // consumer can recognize as mate at all.
    const int mate_band_floor = MATE_SCORE - MAX_SEARCH_PLY;
    const int mates[] = { MATE_SCORE, MATE_SCORE - 1, MATE_SCORE - 10, mate_band_floor };
    for (int m : mates)
    {
        REQUIRE(normalize_score_cp(m) == m);
        REQUIRE(normalize_score_cp(-m) == -m);
    }
}

TEST_CASE("normalize_rescales_just_below_the_mate_band", "[normalization]")
{
    // The seam. One unit below the band is an ordinary evaluation and IS rescaled;
    // the first value in the band is not. Pinning both sides means a change to
    // MAX_SEARCH_PLY or to the comparison operator cannot quietly move the
    // boundary. The band is exactly wide enough for a mate found at any reachable
    // ply, which is why it is expressed as MATE_SCORE - MAX_SEARCH_PLY.
    const int just_below = MATE_SCORE - MAX_SEARCH_PLY - 1;
    REQUIRE(normalize_score_cp(just_below)
            == (just_below * 100 + NORMALIZE_TO_PAWN / 2) / NORMALIZE_TO_PAWN);
    REQUIRE(normalize_score_cp(just_below) != just_below);

    const int in_band = MATE_SCORE - MAX_SEARCH_PLY;
    REQUIRE(normalize_score_cp(in_band) == in_band);
}

TEST_CASE("normalize_passes_out_of_band_sentinels_through", "[normalization]")
{
    // UNKNOWN_SCORE and MAX_SCORE are flags, not evaluations. Dividing them would
    // produce a number that looks like a real (huge) score.
    REQUIRE(normalize_score_cp(UNKNOWN_SCORE) == UNKNOWN_SCORE);
    REQUIRE(normalize_score_cp(-UNKNOWN_SCORE) == -UNKNOWN_SCORE);
    REQUIRE(normalize_score_cp(MAX_SCORE) == MAX_SCORE);
    REQUIRE(normalize_score_cp(-MAX_SCORE) == -MAX_SCORE);
}

TEST_CASE("classify_bands_read_conventional_centipawns", "[normalization]")
{
    // classify() takes a NORMALIZED drop, so its boundaries are in conventional
    // centipawns. Pinned at the boundaries because that is where an off-by-one in a
    // future retune shows up, and because these labels reach a student.
    REQUIRE(MoveComparator::classify(0) == "good");
    REQUIRE(MoveComparator::classify(15) == "good");
    REQUIRE(MoveComparator::classify(16) == "inaccuracy");
    REQUIRE(MoveComparator::classify(50) == "inaccuracy");
    REQUIRE(MoveComparator::classify(51) == "mistake");
    REQUIRE(MoveComparator::classify(150) == "mistake");
    REQUIRE(MoveComparator::classify(151) == "blunder");

    // Sanity in chess terms rather than in numbers: giving away a whole pawn for
    // nothing is at least a mistake, and a whole rook is a blunder.
    REQUIRE(MoveComparator::classify(normalize_score_cp(NORMALIZE_TO_PAWN)) == "mistake");
    REQUIRE(MoveComparator::classify(normalize_score_cp(5 * NORMALIZE_TO_PAWN)) == "blunder");
}

TEST_CASE("compute_nag_bands_read_conventional_centipawns", "[normalization]")
{
    // As classify(). Note these boundaries were converted from the pre-
    // normalization internal-unit values to keep behavior identical, which leaves
    // them about twice as strict as the conventional NAG thresholds now that the
    // argument really is centipawns. Deliberate and behavior-preserving; recorded
    // here so a future decision to adopt the conventional bands is an explicit
    // change to this test rather than an accident.
    REQUIRE(MoveComparator::compute_nag(0, false) == "!");
    REQUIRE(MoveComparator::compute_nag(5, false) == "!");
    REQUIRE(MoveComparator::compute_nag(6, false) == "!?");
    REQUIRE(MoveComparator::compute_nag(15, false) == "!?");
    REQUIRE(MoveComparator::compute_nag(16, false) == "?!");
    REQUIRE(MoveComparator::compute_nag(50, false) == "?!");

    // Playing the engine's own top move is annotated on move identity, not on the
    // drop, so it stays positive whatever the evaluation says.
    REQUIRE(MoveComparator::compute_nag(0, true) == "!");
    REQUIRE(MoveComparator::compute_nag(500, true) == "!");
}
