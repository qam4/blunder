/*
 * File:   ValidateMove.h
 *
 */

#ifndef VALIDATE_MOVE_H
#define VALIDATE_MOVE_H

#include "Move.h"
#include "Board.h"

bool is_valid_move(Move_t move, const class Board &board, bool check_legal);

#endif /* VALIDATE_MOVE_H */
