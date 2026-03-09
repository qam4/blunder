/*
 * File:   TestMultiPV.cpp
 *
 * Unit tests for MultiPV (multiple principal variations) support.
 * Validates: Requirements 1.1, 2.3, 6.1, 7.1, 7.2
 */

#include <iostream>
#include <sstream>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "Board.h"
#include "Book.h"
#include "MoveGenerator.h"
#include "MoveList.h"
#include "Parser.h"
#include "Search.h"
#include "UCI.h"

// Path to books directory — use PROJECT_ROOT_DIR from CMake
static const std::string BOOK_PATH = std::string(PROJECT_ROOT_DIR) + "/books/i-gm1950.bin";

// Helper: capture stdout while executing a callable
template<typename Func>
static std::string capture_stdout(Func&& fn)
{
    std::ostringstream captured;
    std::streambuf* old_buf = std::cout.rdbuf(captured.rdbuf());
    fn();
    std::cout.rdbuf(old_buf);
    return captured.str();
}

// ============================================================================
// Req 1.1: UCI handshake advertises MultiPV option
// ============================================================================

TEST_CASE("uci handshake advertises MultiPV option", "[multipv][uci]")
{
    UCI uci;

    // Capture the output of sending "uci" command by calling cmd_uci indirectly.
    // We feed "uci\nquit\n" via stdin redirection.
    std::string output = capture_stdout(
        [&]()
        {
            std::istringstream input("uci\nquit\n");
            std::streambuf* old_cin = std::cin.rdbuf(input.rdbuf());
            uci.run();
            std::cin.rdbuf(old_cin);
        });

    // Verify the MultiPV option line is present
    std::string expected_option = "option name MultiPV type spin default 1 min 1 max 256";
    REQUIRE(output.find(expected_option) != std::string::npos);

    // Verify it appears before "uciok"
    auto option_pos = output.find(expected_option);
    auto uciok_pos = output.find("uciok");
    REQUIRE(uciok_pos != std::string::npos);
    REQUIRE(option_pos < uciok_pos);
}

// ============================================================================
// Req 2.3: Default MultiPV count is 1
// ============================================================================

TEST_CASE("default MultiPV count is 1", "[multipv][search]")
{
    // Search with default parameters (no setoption) should produce exactly 1 PV result
    std::string fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    Board board = Parser::parse_fen(fen);
    Search search(board);

    // Search at depth 1 with default multipv_count (1)
    search.search(1, -1);

    const auto& results = search.get_multipv_results();
    REQUIRE(results.size() == 1);
    REQUIRE(results[0].best_move() != Move_t(0U));
}

// ============================================================================
// Req 6.1: Book move takes precedence over MultiPV search
// ============================================================================

TEST_CASE("book move takes precedence over MultiPV search", "[multipv][book]")
{
    // Set up UCI with a book and MultiPV > 1, verify book move returned
    UCI uci;

    Book book;
    REQUIRE(book.open(BOOK_PATH));
    uci.set_book(std::move(book));

    // Send commands: set MultiPV to 4, set starting position, go depth 4
    // The starting position is in the book, so a book move should be returned
    // without performing a MultiPV search.
    std::string output = capture_stdout(
        [&]()
        {
            std::istringstream input(
                "uci\n"
                "setoption name MultiPV value 4\n"
                "position startpos\n"
                "go depth 4\n"
                "quit\n");
            std::streambuf* old_cin = std::cin.rdbuf(input.rdbuf());
            uci.run();
            std::cin.rdbuf(old_cin);
        });

    // The output should contain "bestmove" (book move returned)
    REQUIRE(output.find("bestmove") != std::string::npos);

    // Book moves are returned immediately — there should be no "info depth" lines
    // from a MultiPV search (the book bypasses search entirely)
    REQUIRE(output.find("info depth") == std::string::npos);
}

// ============================================================================
// Req 7.1, 7.2: Time limit stops MultiPV mid-depth gracefully
// ============================================================================

TEST_CASE("time limit stops MultiPV search gracefully", "[multipv][time]")
{
    // Use a position with many legal moves and a very short time limit
    // with high MultiPV count to force time expiry during search
    std::string fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    Board board = Parser::parse_fen(fen);
    Search search(board);

    // Very short time: 10000 usec = 0.01 seconds, with MultiPV = 10
    // The search should terminate gracefully without crashing
    Move_t move = search.search(64, 10000, -1, false, 10);

    // The search should return a valid move (not crash or hang)
    REQUIRE(move != Move_t(0U));

    // Results should be non-empty and contain valid data
    const auto& results = search.get_multipv_results();
    REQUIRE(!results.empty());

    // All returned PV lines should have valid root moves
    for (const auto& pv : results)
    {
        if (pv.best_move() != Move_t(0U))
        {
            // Score should be within reasonable bounds
            REQUIRE(pv.score >= -MAX_SCORE);
            REQUIRE(pv.score <= MAX_SCORE);
        }
    }
}

// ============================================================================
// Single legal move returns one PV line regardless of MultiPV count
// ============================================================================

TEST_CASE("single legal move returns one PV line regardless of MultiPV count", "[multipv][search]")
{
    // White Kh8, Black Kf6 + Rg2.
    // Kg8 is attacked by Rg2 (g-file), Kg7 is attacked by Rg2 and Kf6.
    // Only legal move is Kh7.
    std::string fen = "7K/8/5k2/8/8/8/6r1/8 w - - 0 1";
    Board board = Parser::parse_fen(fen);

    // Verify there's exactly one legal move
    MoveList legal;
    MoveGenerator::add_all_moves(legal, board, board.side_to_move());
    REQUIRE(legal.length() == 1);

    Search search(board);
    // Search with MultiPV = 5, but only 1 legal move exists
    search.search(4, -1, -1, false, 5);

    const auto& results = search.get_multipv_results();
    // Should return exactly 1 PV line (capped by legal moves)
    REQUIRE(results.size() == 1);
    REQUIRE(results[0].best_move() != Move_t(0U));
}
