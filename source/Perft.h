/*
 * File:   Perft.h
 * Author: fm
 *
 */

#ifndef PERFT_H
#define PERFT_H

#include "Common.h"
#include "Board.h"

int perft(class Board board, int depth);
int perft_fen(string fen, int depth);
void perft_benchmark(string fen, int depth);

#endif /* PERFT_H */
