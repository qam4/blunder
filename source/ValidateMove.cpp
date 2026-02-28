/*
 * File:   ValidateMove.cpp
 *
 */
#include "ValidateMove.h"

#include "MoveGenerator.h"
#include "Output.h"

using std::cerr;
using std::endl;
using std::string;

string static is_legal_move_err(Move_t move, const class Board& board);
string static is_valid_move_err(Move_t move, const class Board& board);

bool is_valid_move(Move_t move, const class Board& board, bool check_legal)
{
    // cout << "Validating " << Output::move(move, board) << "..." << endl;
    string error = is_valid_move_err(move, board);
    if (error == "")
    {
        if (check_legal)
        {
            error = is_legal_move_err(move, board);
            if (error == "")
            {
                return true;
            }
        }
        else
        {
            return true;
        }
    }
    cerr << "INVALID_MOVE MOVE " << Output::move(move, board) << ": " << error << endl;
    cerr << Output::board(board);
    cerr << "fen=" << Output::board_to_fen(board) << endl;
    return false;
}

string is_legal_move_err(Move_t move, const class Board& board)
{
    U8 side = board.side_to_move();
    Board board_copy = board;
    board_copy.do_move(move);
    bool is_in_check = MoveGenerator::in_check(board_copy, side);
    board_copy.undo_move(move);
    if (is_in_check)
    {
        return "king is in check";
    }
    return "";
}

string is_valid_move_err(Move_t move, const class Board& board)
{
    U8 side = board.side_to_move();
    U8 from = move_from(move);
    U8 to = move_to(move);
    U8 mover = board[from];
    U8 captured = move_captured(move);
    U8 promote_to = move_promote_to(move);

    // check castle first
    if (is_castle(move))
    {
        if ((move & 0xFFFF) != 0)
            return "Castle move shouldn't have data in lower 2 bytes";
        if (move != build_castle(KING_CASTLE) && move != build_castle(QUEEN_CASTLE))
            return "Invalid castle move";
        if (move == build_castle(KING_CASTLE))
        {
            if (side == WHITE)
            {
                if (!(board.castling_rights() & WHITE_KING_SIDE))
                    return "White doesn't have king-side castling rights";
                if (board[H1] != WHITE_ROOK)
                    return "White cannot castle king-side no rook in h1";
                if (board[E1] != WHITE_KING)
                    return "White cannot castle king-side no king in e1";
                if (board[G1] != EMPTY)
                    return "White cannot castle king-side g1 not empty";
                if (board[F1] != EMPTY)
                    return "White cannot castle king-side f1 not empty";
            }
            else
            {
                if (!(board.castling_rights() & BLACK_KING_SIDE))
                    return "Black doesn't have king-side castling rights";
                if (board[H8] != BLACK_ROOK)
                    return "Black cannot castle king-side no rook in h1";
                if (board[E8] != BLACK_KING)
                    return "Black cannot castle king-side no king in e1";
                if (board[G8] != EMPTY)
                    return "Black cannot castle king-side g1 not empty";
                if (board[F8] != EMPTY)
                    return "Black cannot castle king-side f1 not empty";
            }
        }
        if (move == build_castle(QUEEN_CASTLE))
        {
            if (side == WHITE)
            {
                if (!(board.castling_rights() & WHITE_QUEEN_SIDE))
                    return "White doesn't have queen-side castling rights";
                if (board[A1] != WHITE_ROOK)
                    return "White cannot castle queen-side no rook in a1";
                if (board[E1] != WHITE_KING)
                    return "White cannot castle queen-side no king in e1";
                if (board[B1] != EMPTY)
                    return "White cannot castle queen-side b1 not empty";
                if (board[C1] != EMPTY)
                    return "White cannot castle queen-side c1 not empty";
                if (board[D1] != EMPTY)
                    return "White cannot castle queen-side d1 not empty";
            }
            else
            {
                if (!(board.castling_rights() & BLACK_QUEEN_SIDE))
                    return "Black doesn't have queen-side castling rights";
                if (board[A8] != BLACK_ROOK)
                    return "Black cannot castle queen-side no rook in a8";
                if (board[E8] != BLACK_KING)
                    return "Black cannot castle queen-side no king in e8";
                if (board[B8] != EMPTY)
                    return "Black cannot castle queen-side b8 not empty";
                if (board[C8] != EMPTY)
                    return "Black cannot castle queen-side c8 not empty";
                if (board[D8] != EMPTY)
                    return "Black cannot castle queen-side d8 not empty";
            }
        }
        // still need to add checks
        // assert(false);  // TODO: implement stuff
        return "";
    }
    if (mover == EMPTY)
        return "mover doesn't exist in board";
    if ((mover & 1) != side)
        return "mover belongs to wrong side";
    // if capture make sure something to capture
    if (is_capture(move) && !is_ep_capture(move))
    {
        if (captured == EMPTY)
            return "can't find captured piece";
        if ((captured & 1) == side)
            return "can't capture friendly piece";
    }
    // if EP capture make sure correct square
    if (is_ep_capture(move))
    {
        if (side == BLACK)
        {
            if (board.ep_square() != to)
                return "no ep square set";
            if (mover != BLACK_PAWN)
                return "can't ep move non-pawn";
            if (captured != WHITE_PAWN)
                return "can't ep capture non-pawn";
            if ((from & 56) != (3 * 8))
                return "ep capture from wrong row";
            if ((to & 56) != (2 * 8))
                return "ep capture to wrong row";
        }
        else
        {
            if (board.ep_square() != to)
                return "no ep square set";
            if (mover != WHITE_PAWN)
                return "can't ep move non-pawn";
            if (captured != BLACK_PAWN)
                return "can't ep capture non-pawn";
            if ((from & 56) != (4 * 8))
                return "ep capture from wrong row";
            if ((to & 56) != (5 * 8))
                return "ep capture to wrong row";
        }
    }
    // if promotion make sure on right square
    if (is_promotion(move))
    {
        if (side == BLACK)
        {
            if ((from & 56) != (1 * 8))
                return "promotion from wrong row";
            if ((to & 56) != (0 * 8))
                return "promotion to wrong row";
            if (promote_to != BLACK_QUEEN && promote_to != BLACK_ROOK && promote_to != BLACK_BISHOP
                && promote_to != BLACK_KNIGHT)
                return "invalid promote_to piece";
        }
        else
        {
            if ((from & 56) != (6 * 8))
                return "promotion from wrong row";
            if ((to & 56) != (7 * 8))
                return "promotion to wrong row";
            if (promote_to != WHITE_QUEEN && promote_to != WHITE_ROOK && promote_to != WHITE_BISHOP
                && promote_to != WHITE_KNIGHT)
                return "invalid promote_to piece";
        }
    }
    if (is_pawn_double_push(move))
    {
        if (side == BLACK)
        {
            if ((from & 56) != (6 * 8))
                return "pawn double push from wrong row";
            if ((to & 56) != (4 * 8))
                return "pawn double push to wrong row";
            if (from - to != 16)
                return "pawn double push should move two rows";
            if (mover != BLACK_PAWN)
                return "only pawn can pawn double push";
        }
        else
        {
            if ((from & 56) != (1 * 8))
                return "pawn double push from wrong row";
            if ((to & 56) != (3 * 8))
                return "pawn double push to wrong row";
            if (to - from != 16)
                return "pawn double push should move two rows";
            if (mover != WHITE_PAWN)
                return "only pawn can pawn double push";
        }
    }
    int rows = abs((from & 56) - (to & 56)) >> 3;
    int files = abs((from & 7) - (to & 7));
    // if knight make sure rows moved ^ files moves = 3
    if ((mover & (~1)) == KNIGHT)
    {
        if ((rows ^ files) != 3)
            return "knight must move 2 rows and 1 file (or vice versa)";
    }
    // if pawn make sure rows moved and files moved make sense
    if ((mover & (~1)) == PAWN)
    {
        if (rows > 2)
            return "pawn can't move more than 2 rows";
        if (rows == 0)
            return "pawn must move 1 row";
        if (files > 1)
            return "pawn can't move more than 1 file";
    }
    // if king check rows and files 0 or 1
    if ((mover & (~1)) == KING)
    {
        if (rows > 1)
            return "king can't move more than 1 row";
        if (files > 1)
            return "king can't move more than 1 file";
    }

    return "";
}