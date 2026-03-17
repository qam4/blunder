/*
 * File:   MoveComparator.cpp
 *
 * Implements move comparison: classification, NAG assignment, and
 * heuristic description of the best move.
 */

#include <algorithm>
#include <string>
#include <vector>

#include "MoveComparator.h"

#include "Output.h"
#include "PositionAnalyzer.h"

// ---------------------------------------------------------------------------
// Local helper: convert a Move_t to a UCI string (e.g. "e2e4", "e7e8q").
// Mirrors the static helper in CoachJson.cpp.
// ---------------------------------------------------------------------------
static std::string move_to_uci(Move_t move, U8 side)
{
    if (move == Move(0U))
        return "0000";

    if (move.is_castle())
    {
        if (move == build_castle(KING_CASTLE))
            return (side == WHITE) ? "e1g1" : "e8g8";
        else
            return (side == WHITE) ? "e1c1" : "e8c8";
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
        switch (move.promote_to())
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

// ---------------------------------------------------------------------------
// compare() — full comparison flow
//
// 1. Run MultiPV search on the position to get top lines and the best move.
// 2. If user_move == best_move, eval_drop_cp = 0.
// 3. Otherwise, apply user's move, search the resulting position, negate the
//    score (opponent's perspective → original side's perspective), compute
//    eval_drop_cp = max(0, best_eval_cp - user_eval_cp).
// 4. Classify and assign NAG.
// 5. If blunder, extract refutation_line from the post-user-move search PV.
// 6. Detect missed tactics (tactics in best line but not after user's move).
// 7. Populate top_lines, critical_moment, critical_reason from MultiPV results.
// ---------------------------------------------------------------------------
ComparisonReport MoveComparator::compare(
    Board& board, Search& search, Move_t user_move, int multipv_count, int depth, int movetime)
{
    ComparisonReport report;
    U8 side = board.side_to_move();

    report.fen = Output::board_to_fen(board);
    report.user_move = move_to_uci(user_move, side);

    // --- Step 1: Run MultiPV search on the current position ----------------
    search.set_verbose(false);
    search.set_output_mode(Search::OutputMode::NORMAL);
    search.set_abort(false);
    if (movetime > 0)
    {
        search.get_tm().start(movetime * 1000);  // ms → µs
    }
    else
    {
        search.get_tm().start(-1, -1);  // no time limit, depth-limited
    }
    search.search(depth, -1, -1, false, multipv_count);

    const auto& multipv_results = search.get_multipv_results();
    if (multipv_results.empty())
    {
        // No legal moves / search produced nothing — return a default report
        report.best_move = report.user_move;
        report.best_eval_cp = 0;
        report.user_eval_cp = 0;
        report.eval_drop_cp = 0;
        report.classification = "good";
        report.nag = "!";
        report.best_move_idea = "no moves available";
        return report;
    }

    // Best move and eval from the MultiPV results
    Move_t best_move = multipv_results[0].best_move();
    int best_eval = multipv_results[0].score;

    report.best_move = move_to_uci(best_move, side);
    report.best_eval_cp = best_eval;

    // --- Step 2 & 3: Compute user eval and eval drop -----------------------
    bool is_best = (user_move == best_move);

    if (is_best)
    {
        report.user_eval_cp = best_eval;
        report.eval_drop_cp = 0;
    }
    else
    {
        // Apply user's move, search the resulting position, negate the score
        board.do_move(user_move);

        search.set_verbose(false);
        search.set_output_mode(Search::OutputMode::NORMAL);
        search.set_abort(false);
        if (movetime > 0)
        {
            search.get_tm().start(movetime * 1000);
        }
        else
        {
            search.get_tm().start(-1, -1);
        }
        search.search(depth, -1, -1, false, 1);  // single-PV for user's move

        const auto& user_results = search.get_multipv_results();
        int user_eval_raw = user_results.empty() ? 0 : user_results[0].score;

        // Negate: the search returns score from the opponent's perspective
        int user_eval = -user_eval_raw;
        report.user_eval_cp = user_eval;
        report.eval_drop_cp = std::max(0, best_eval - user_eval);

        // --- Step 5: Extract refutation line if blunder --------------------
        // The PV from the post-user-move search IS the opponent's best
        // continuation (the refutation).
        std::string classification_preview = classify(report.eval_drop_cp);
        if (classification_preview == "blunder" && !user_results.empty())
        {
            const auto& refutation_pv = user_results[0].moves;
            // The opponent is now side_to_move after user's move
            U8 opp_side = board.side_to_move();
            for (size_t i = 0; i < refutation_pv.size(); ++i)
            {
                // Alternate sides for each move in the PV
                U8 move_side = (i % 2 == 0) ? opp_side : side;
                report.refutation_line.push_back(move_to_uci(refutation_pv[i], move_side));
            }
        }

        // --- Step 6: Detect missed tactics ---------------------------------
        // Tactics in the best line (from the original position search)
        std::vector<Tactic> best_tactics = PositionAnalyzer::detect_tactics(board, multipv_results);

        // Tactics after user's move (from the post-user-move search)
        std::vector<Tactic> user_tactics = PositionAnalyzer::detect_tactics(board, user_results);

        // Missed tactics = tactics in best line but not after user's move
        for (const auto& bt : best_tactics)
        {
            bool found = false;
            for (const auto& ut : user_tactics)
            {
                if (bt.type == ut.type && bt.description == ut.description)
                {
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                report.missed_tactics.push_back(bt);
            }
        }

        board.undo_move(user_move);
    }

    // --- Step 4: Classify and assign NAG -----------------------------------
    report.classification = classify(report.eval_drop_cp);
    report.nag = compute_nag(report.eval_drop_cp, is_best);

    // --- Best move idea ----------------------------------------------------
    std::vector<Move_t> best_pv_moves;
    if (!multipv_results.empty())
    {
        best_pv_moves = multipv_results[0].moves;
    }
    report.best_move_idea = describe_best_move(board, best_move, best_pv_moves);

    // --- Step 7: Populate top_lines, critical_moment, critical_reason ------
    report.top_lines = multipv_results;

    std::string reason;
    report.critical_moment = PositionAnalyzer::is_critical_moment(multipv_results, reason);
    report.critical_reason = reason;

    return report;
}

// ---------------------------------------------------------------------------
// classify() — map eval_drop_cp to a classification string
//   good:       eval_drop_cp <= 30
//   inaccuracy: 31 <= eval_drop_cp <= 100
//   mistake:    101 <= eval_drop_cp <= 300
//   blunder:    eval_drop_cp > 300
// ---------------------------------------------------------------------------
std::string MoveComparator::classify(int eval_drop_cp)
{
    if (eval_drop_cp <= 30)
        return "good";
    if (eval_drop_cp <= 100)
        return "inaccuracy";
    if (eval_drop_cp <= 300)
        return "mistake";
    return "blunder";
}

// ---------------------------------------------------------------------------
// compute_nag() — map eval_drop_cp to a NAG symbol
//   !   eval_drop_cp <= 10
//   !?  11 <= eval_drop_cp <= 30
//   ?!  31 <= eval_drop_cp <= 100
//   ?   101 <= eval_drop_cp <= 300
//   ??  eval_drop_cp > 300
// Special: is_best_move == true → always "!"
// ---------------------------------------------------------------------------
std::string MoveComparator::compute_nag(int eval_drop_cp, bool is_best_move)
{
    if (is_best_move)
        return "!";

    if (eval_drop_cp <= 10)
        return "!";
    if (eval_drop_cp <= 30)
        return "!?";
    if (eval_drop_cp <= 100)
        return "?!";
    if (eval_drop_cp <= 300)
        return "?";
    return "??";
}

// ---------------------------------------------------------------------------
// describe_best_move() — heuristic description based on move characteristics
//
// Examines the best move's properties (capture, check, castling, pawn push,
// piece development) and returns a short human-readable string.
// ---------------------------------------------------------------------------
std::string MoveComparator::describe_best_move(const Board& board,
                                               Move_t best_move,
                                               const std::vector<Move_t>& /*pv_moves*/)
{
    U8 from_sq = best_move.from();
    U8 to_sq = best_move.to();
    U8 piece = board[from_sq];
    U8 piece_type = piece & ~1U;  // strip color bit to get type (PAWN, KNIGHT, etc.)

    // Castling → king safety
    if (best_move.is_castle())
    {
        return "king safety — castling to a safer position";
    }

    // Captures of high-value pieces → material win
    if (best_move.is_capture())
    {
        U8 captured = best_move.captured();
        U8 captured_type = captured & ~1U;
        if (captured_type == QUEEN)
        {
            return "material win — captures the queen";
        }
        if (captured_type == ROOK)
        {
            return "material win — captures a rook";
        }
        if (captured_type >= KNIGHT && captured_type <= BISHOP)
        {
            return "material win — captures a minor piece";
        }
        return "material gain — winning capture";
    }

    // Promotion
    if (best_move.is_promotion())
    {
        return "pawn promotion — creating a new piece";
    }

    // Pawn moves to center (d4, e4, d5, e5)
    if (piece_type == PAWN)
    {
        if (to_sq == D4 || to_sq == E4 || to_sq == D5 || to_sq == E5)
        {
            return "central control — pawn occupies the center";
        }
        // Advanced pawn push
        U8 rank = to_sq >> 3;
        if (rank >= 5)
        {  // 6th rank or beyond for white (ranks 5,6,7 = 6th,7th,8th)
            return "pawn advance — pushing toward promotion";
        }
        return "pawn structure — improving pawn position";
    }

    // Knight/Bishop development (moving from back rank)
    if (piece_type == KNIGHT || piece_type == BISHOP)
    {
        U8 from_rank = from_sq >> 3;
        if (from_rank == 0 || from_rank == 7)
        {
            return "piece development — activating a minor piece";
        }
        // Knight/Bishop to center
        if (to_sq == D4 || to_sq == E4 || to_sq == D5 || to_sq == E5)
        {
            return "central control — placing piece in the center";
        }
        return "piece activity — improving piece placement";
    }

    // Rook moves
    if (piece_type == ROOK)
    {
        // Check if moving to an open file or 7th rank
        U8 to_rank = to_sq >> 3;
        if (to_rank == 6 || to_rank == 1)
        {  // 7th rank for either side
            return "rook activity — rook on the seventh rank";
        }
        return "rook activity — improving rook placement";
    }

    // Queen moves
    if (piece_type == QUEEN)
    {
        return "queen activity — improving queen placement";
    }

    // King moves (non-castling)
    if (piece_type == KING)
    {
        return "king safety — repositioning the king";
    }

    return "improving position";
}
