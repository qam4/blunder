#include "Perft.h"

#include "Move.h"
#include "MoveGenerator.h"
#include "MoveList.h"
#include "Parser.h"

// Performance testing
// https://www.chessprogramming.org/Perft

long perft(class Board& board, int depth)
{
    MoveList list;
    MoveGenerator::add_all_moves(list, board, board.side_to_move());
    int n = list.length();

    if (depth == 1)
    {
        return n;
    }

    long move_count = 0;
    for (int i = 0; i < n; i++)
    {
        Move_t move = list[i];
        board.do_move(move);
        move_count += perft(board, depth - 1);
        board.undo_move(move);
    }

    return move_count;
}

long perft_fen(string fen, int depth)
{
    Board board = Parser::parse_fen(fen);

    long move_count = perft(board, depth);

    return move_count;
}

void perft_benchmark(string fen, int depth)
{
    Board board = Parser::parse_fen(fen);

    clock_t tic = clock();
    long move_count = perft(board, depth);
    clock_t toc = clock();

    long double elapsed_secs = static_cast<long double>(toc - tic) / CLOCKS_PER_SEC;
    cout << "time: " << elapsed_secs << "s" << endl;
    cout << "moves: " << move_count << endl;
    cout << "moves per second: " << move_count / elapsed_secs << endl;
}
