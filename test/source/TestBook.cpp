/*
 * File:   TestBook.cpp
 *
 * Unit tests for the Polyglot opening book reader.
 * Validates: Requirements 25.1, 25.3, 26.1, 26.2
 */

#include <catch2/catch_test_macros.hpp>

#include "Board.h"
#include "Book.h"
#include "Parser.h"
#include "Tests.h"

// Path to books directory relative to the CTest working directory (build/dev-mingw/test/)
static const std::string BOOK_PATH = "../../../books/i-gm1950.bin";

// ============================================================================
// polyglot_hash tests — verify against published test keys from the Polyglot
// book format specification.
// Reference: https://allstar.jhuapl.edu/repo/p4/amd64/polyglot/doc/book_format.html
// ============================================================================

TEST_CASE("polyglot_hash matches published key for starting position", "[book]")
{
    auto board = Parser::parse_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    REQUIRE(Book::polyglot_hash(board) == U64(0x463b96181691fc9c));
}

TEST_CASE("polyglot_hash matches published key after e2e4", "[book]")
{
    auto board = Parser::parse_fen("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1");
    REQUIRE(Book::polyglot_hash(board) == U64(0x823c9b50fd114196));
}

TEST_CASE("polyglot_hash matches published key after e2e4 d7d5", "[book]")
{
    auto board = Parser::parse_fen("rnbqkbnr/ppp1pppp/8/3p4/4P3/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 2");
    REQUIRE(Book::polyglot_hash(board) == U64(0x0756b94461c50fb0));
}

TEST_CASE("polyglot_hash matches published key after e2e4 d7d5 e4e5", "[book]")
{
    auto board = Parser::parse_fen("rnbqkbnr/ppp1pppp/8/3pP3/8/8/PPPP1PPP/RNBQKBNR b KQkq - 0 2");
    REQUIRE(Book::polyglot_hash(board) == U64(0x662fafb965db29d4));
}

TEST_CASE("polyglot_hash matches published key with en passant possible", "[book]")
{
    // After e2e4 d7d5 e4e5 f7f5 — en passant is possible
    auto board = Parser::parse_fen("rnbqkbnr/ppp1p1pp/8/3pPp2/8/8/PPPP1PPP/RNBQKBNR w KQkq f6 0 3");
    REQUIRE(Book::polyglot_hash(board) == U64(0x22a48b5a8e47ff78));
}

TEST_CASE("polyglot_hash matches published key after king moves", "[book]")
{
    // After e2e4 d7d5 e4e5 f7f5 e1e2 — white king moved
    auto board = Parser::parse_fen("rnbqkbnr/ppp1p1pp/8/3pPp2/8/8/PPPPKPPP/RNBQ1BNR b kq - 0 3");
    REQUIRE(Book::polyglot_hash(board) == U64(0x652a607ca3f242c1));
}

TEST_CASE("polyglot_hash matches published key with no castling rights", "[book]")
{
    // After e2e4 d7d5 e4e5 f7f5 e1e2 e8f7 — both kings moved
    auto board = Parser::parse_fen("rnbq1bnr/ppp1pkpp/8/3pPp2/8/8/PPPPKPPP/RNBQ1BNR w - - 0 4");
    REQUIRE(Book::polyglot_hash(board) == U64(0x00fdd303c946bdd9));
}

TEST_CASE("polyglot_hash matches published key for en passant edge case", "[book]")
{
    // After a2a4 b7b5 h2h4 b5b4 c2c4 — en passant possible on c3
    auto board = Parser::parse_fen("rnbqkbnr/p1pppppp/8/8/PpP4P/8/1P1PPPP1/RNBQKBNR b KQkq c3 0 3");
    REQUIRE(Book::polyglot_hash(board) == U64(0x3c8123ea7b067637));
}

TEST_CASE("polyglot_hash matches published key after en passant capture", "[book]")
{
    // After a2a4 b7b5 h2h4 b5b4 c2c4 b4c3 a1a3
    auto board = Parser::parse_fen("rnbqkbnr/p1pppppp/8/8/P6P/R1p5/1P1PPPP1/1NBQKBNR b Kkq - 0 4");
    REQUIRE(Book::polyglot_hash(board) == U64(0x5c3f9b829b279560));
}

// ============================================================================
// Book file loading and move lookup tests
// ============================================================================

TEST_CASE("book open returns false for nonexistent file", "[book]")
{
    Book book;
    REQUIRE_FALSE(book.open("nonexistent_file.bin"));
}

TEST_CASE("book open succeeds for existing book file", "[book]")
{
    Book book;
    REQUIRE(book.open(BOOK_PATH));
    book.close();
}

TEST_CASE("book has_move returns true for starting position", "[book]")
{
    Book book;
    REQUIRE(book.open(BOOK_PATH));

    auto board = Parser::parse_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    REQUIRE(book.has_move(board));

    book.close();
}

TEST_CASE("book get_move returns a valid move for starting position", "[book]")
{
    Book book;
    REQUIRE(book.open(BOOK_PATH));

    auto board = Parser::parse_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    Move_t move = book.get_move(board);
    REQUIRE(move != Move(0));

    // The move should be a legal move
    MoveList list;
    MoveGenerator::add_all_moves(list, board, board.side_to_move());
    REQUIRE(list.contains(move));

    book.close();
}

TEST_CASE("book has_move returns false for empty book", "[book]")
{
    Book book;
    auto board = Parser::parse_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    REQUIRE_FALSE(book.has_move(board));
}

TEST_CASE("book get_move returns zero for position not in book", "[book]")
{
    Book book;
    REQUIRE(book.open(BOOK_PATH));

    // A bare kings position — not in any opening book
    auto board = Parser::parse_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
    REQUIRE_FALSE(book.has_move(board));
    REQUIRE(book.get_move(board) == Move(0));

    book.close();
}
