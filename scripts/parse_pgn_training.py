#!/usr/bin/env python3
"""
Parse PGN game databases (e.g. Lichess) into NNUE training data.

Extracts positions from real games and labels them with the game result,
producing binary training data compatible with train_nnue.py.

Data sources:
  - Lichess: https://database.lichess.org/ (monthly PGN dumps)
  - FIDE/TWIC: https://theweekinchess.com/twic

Usage:
    python scripts/parse_pgn_training.py --input games.pgn --output training.bin
    python scripts/parse_pgn_training.py --input games.pgn --output training.bin --min-elo 2000
"""

import argparse
import struct
import sys
from pathlib import Path

try:
    import chess
    import chess.pgn
except ImportError:
    print("Error: python-chess is required. Install with: pip install python-chess")
    sys.exit(1)


# Piece type mapping: chess.PAWN=1..chess.KING=6 → 0..5
PIECE_TYPE_MAP = {
    chess.PAWN: 0,
    chess.KNIGHT: 1,
    chess.BISHOP: 2,
    chess.ROOK: 3,
    chess.QUEEN: 4,
    chess.KING: 5,
}

RESULT_MAP = {
    "1-0": 1.0,
    "0-1": 0.0,
    "1/2-1/2": 0.5,
}


def board_to_features(board: chess.Board) -> list[float]:
    """Convert a python-chess Board to 768 HalfKP features.

    Encoding: 12 planes of 64 squares each.
      plane = piece_type * 2 + color   (color: 0=white, 1=black)
      index = plane * 64 + square
    """
    features = [0.0] * 768
    for sq in chess.SQUARES:
        piece = board.piece_at(sq)
        if piece is None:
            continue
        pt = PIECE_TYPE_MAP[piece.piece_type]
        color = 0 if piece.color == chess.WHITE else 1
        plane = pt * 2 + color
        features[plane * 64 + sq] = 1.0
    return features


def extract_positions(
    pgn_path: str,
    max_games: int,
    min_elo: int,
    skip_first_n: int,
    sample_every: int,
) -> list[tuple[list[float], float, float]]:
    """Extract labeled positions from a PGN file.

    Returns list of (features_768, search_score_placeholder, game_result).
    search_score is set to 0.0 since we only have the game result.
    """
    entries: list[tuple[list[float], float, float]] = []
    games_parsed = 0
    games_skipped_elo = 0
    games_skipped_result = 0

    with open(pgn_path, encoding="utf-8", errors="replace") as f:
        while True:
            if max_games > 0 and games_parsed >= max_games:
                break

            game = chess.pgn.read_game(f)
            if game is None:
                break

            # Filter by result
            result_str = game.headers.get("Result", "*")
            if result_str not in RESULT_MAP:
                games_skipped_result += 1
                continue

            # Filter by Elo
            if min_elo > 0:
                try:
                    white_elo = int(game.headers.get("WhiteElo", "0"))
                    black_elo = int(game.headers.get("BlackElo", "0"))
                    if white_elo < min_elo or black_elo < min_elo:
                        games_skipped_elo += 1
                        continue
                except ValueError:
                    games_skipped_elo += 1
                    continue

            result = RESULT_MAP[result_str]
            board = game.board()
            move_num = 0

            for move in game.mainline_moves():
                board.push(move)
                move_num += 1

                # Skip opening moves
                if move_num <= skip_first_n:
                    continue

                # Sample every N moves to reduce correlation
                if (move_num - skip_first_n) % sample_every != 0:
                    continue

                # Skip positions in check (noisy)
                if board.is_check():
                    continue

                # Flip result to side-to-move perspective
                stm_result = result if board.turn == chess.WHITE else 1.0 - result

                features = board_to_features(board)
                entries.append((features, 0.0, stm_result))

            games_parsed += 1
            if games_parsed % 1000 == 0:
                print(
                    f"  Parsed {games_parsed} games, "
                    f"{len(entries)} positions extracted..."
                )

    print(f"\nParsing complete:")
    print(f"  Games parsed:       {games_parsed}")
    print(f"  Games skipped (Elo): {games_skipped_elo}")
    print(f"  Games skipped (result): {games_skipped_result}")
    print(f"  Positions extracted: {len(entries)}")
    return entries



def write_training_data(
    entries: list[tuple[list[float], float, float]], output_path: str
) -> None:
    """Write training data in the same binary format as the C++ self-play.

    Format per entry: [768 floats: features][1 float: score][1 float: result]
    Total: 3080 bytes per entry.
    """
    with open(output_path, "wb") as f:
        for features, score, result in entries:
            for val in features:
                f.write(struct.pack("f", val))
            f.write(struct.pack("f", score))
            f.write(struct.pack("f", result))

    size_mb = Path(output_path).stat().st_size / (1024 * 1024)
    print(f"Wrote {len(entries)} entries to {output_path} ({size_mb:.1f} MB)")


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Parse PGN databases into NNUE training data"
    )
    parser.add_argument(
        "--input", required=True, help="Input PGN file (can be large)"
    )
    parser.add_argument(
        "--output",
        default="training_pgn.bin",
        help="Output binary file (default: training_pgn.bin)",
    )
    parser.add_argument(
        "--max-games",
        type=int,
        default=0,
        help="Max games to parse (0 = all, default: 0)",
    )
    parser.add_argument(
        "--min-elo",
        type=int,
        default=0,
        help="Minimum Elo for both players (0 = no filter, default: 0)",
    )
    parser.add_argument(
        "--skip-first",
        type=int,
        default=8,
        help="Skip first N moves per game (default: 8)",
    )
    parser.add_argument(
        "--sample-every",
        type=int,
        default=4,
        help="Sample every Nth move after skip (default: 4)",
    )

    args = parser.parse_args()

    if not Path(args.input).exists():
        print(f"Error: Input file '{args.input}' not found!")
        print("\nDownload Lichess games from: https://database.lichess.org/")
        print("Example: lichess_db_standard_rated_2024-01.pgn.zst")
        print("Decompress with: zstd -d lichess_db_standard_rated_2024-01.pgn.zst")
        sys.exit(1)

    print(f"Parsing PGN: {args.input}")
    print(f"  Max games:    {args.max_games or 'all'}")
    print(f"  Min Elo:      {args.min_elo or 'none'}")
    print(f"  Skip first:   {args.skip_first} moves")
    print(f"  Sample every: {args.sample_every} moves")
    print()

    entries = extract_positions(
        pgn_path=args.input,
        max_games=args.max_games,
        min_elo=args.min_elo,
        skip_first_n=args.skip_first,
        sample_every=args.sample_every,
    )

    if not entries:
        print("No positions extracted. Check your filters.")
        sys.exit(1)

    write_training_data(entries, args.output)


if __name__ == "__main__":
    main()
