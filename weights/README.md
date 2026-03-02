# NNUE Model Weights

This directory contains trained NNUE (Efficiently Updatable Neural Network) evaluation weights for the Blunder chess engine.

## Versioning Scheme

Weights are versioned as `nnue_vXXX.bin` where XXX is a zero-padded version number:

### nnue_v001.bin
- 300 positions from 5 games at depth 3, 5 epochs, val loss 1167.18
- Result: 0-20 vs HandCrafted
```bash
./blunder --selfplay --selfplay-games 5 --selfplay-depth 3 --selfplay-output training_v001.bin
python scripts/train_nnue.py --input training_v001.bin --output weights/nnue_v001.bin --epochs 5
```

### nnue_v002.bin
- 59,797 positions from 1000 games at depth 6, 50 epochs, batch 256, lr 0.001
- Result: 12-7-1 vs HandCrafted (+88.7 Elo, 87.4% LOS)
```bash
./blunder --selfplay --selfplay-games 1000 --selfplay-depth 6 --selfplay-output training_v002.bin
python scripts/train_nnue.py --input training_v002.bin --output weights/nnue_v002.bin --epochs 50 --batch-size 256 --learning-rate 0.001
python scripts/compare_nnue_vs_handcrafted.py --nnue weights/nnue_v002.bin --games 20 --tc "10+0.1"
```

## Quick Start

```bash
# Load a trained model
./blunder --nnue weights/nnue_v002.bin
```

The engine falls back to HandCrafted evaluation if no NNUE weights are loaded.

## Model Architecture

- Input: 768 features (HalfKP encoding: 6 piece types × 2 colors × 64 squares)
- Hidden layers: 768 → 256 → 32 → 32 → 1
- Output: Evaluation score (centipawns)
- Activation: ReLU
- Total parameters: 206,177

## Training Guide

There are two ways to generate training data:

### Method A: Self-Play (bootstrap)

Uses the engine's own search to generate and label positions. Good for
bootstrapping when no external data is available.

```bash
# 1. Generate training data via self-play
./blunder --selfplay --selfplay-games 1000 --selfplay-depth 6 \
          --selfplay-output training_selfplay.bin

# 2. Train the model
python scripts/train_nnue.py \
    --input training_selfplay.bin \
    --output weights/nnue_vXXX.bin \
    --epochs 50 --batch-size 256 --learning-rate 0.001

# 3. Test against HandCrafted
python scripts/compare_nnue_vs_handcrafted.py \
    --nnue weights/nnue_vXXX.bin --games 100
```

Parameters to tune:
- `--selfplay-games`: More games = more diverse positions (1000-10000)
- `--selfplay-depth`: Higher depth = better labels but slower (4-8)
- `--epochs`: Training iterations (20-100)
- `--batch-size`: Larger = smoother gradients (128-512)
- `--learning-rate`: Start at 0.001, reduce if loss oscillates

### Method B: PGN Database (recommended for stronger models)

Uses positions from real games played by strong players. Labels come from
game results (win/draw/loss) rather than engine evaluation.

**Data source:** Lichess monthly PGN dumps at https://database.lichess.org/.
Files are zstd-compressed and very large (10-50 GB compressed, 50-200 GB
decompressed per month). Always use `--max-games` to limit how many games
are parsed — the script streams games one at a time so memory is fine, but
parsing millions of games takes hours.

```bash
# 1. Download games from Lichess (pick a recent month)
#    https://database.lichess.org/
#    Files are .pgn.zst — decompress first:
#    zstd -d lichess_db_standard_rated_2024-01.pgn.zst
#
#    Tip: for a quick start, use a smaller file like the Lichess Elite
#    database (~200 MB) from https://database.nikonoel.fr/

# 2. Parse PGN into training data (ALWAYS use --max-games!)
python scripts/parse_pgn_training.py \
    --input games.pgn \
    --output training_pgn.bin \
    --max-games 50000 \
    --min-elo 2000 \
    --skip-first 8 \
    --sample-every 4

# 3. Train (same as above)
python scripts/train_nnue.py \
    --input training_pgn.bin \
    --output weights/nnue_vXXX.bin \
    --epochs 50 --batch-size 256

# 4. Test
python scripts/compare_nnue_vs_handcrafted.py \
    --nnue weights/nnue_vXXX.bin --games 100
```

PGN parsing options:
- `--min-elo 2000`: Only use games where both players are 2000+
- `--skip-first 8`: Skip opening moves (book positions aren't useful)
- `--sample-every 4`: Sample every 4th move to reduce correlation
- `--max-games 50000`: Limit number of games to parse

Requires: `pip install python-chess`

### Method C: Hybrid (best quality)

Combine self-play data with PGN data for maximum diversity:

```bash
# Generate both datasets, then concatenate:
cat training_selfplay.bin training_pgn.bin > training_combined.bin

# Or on Windows:
copy /b training_selfplay.bin + training_pgn.bin training_combined.bin
```

## Testing & Elo Measurement

### Against HandCrafted evaluator

```bash
python scripts/compare_nnue_vs_handcrafted.py \
    --nnue weights/nnue_vXXX.bin --games 100 --tc "10+0.1"
```

### Multi-engine tournament (Elo rating)

Run a round-robin or gauntlet tournament against other engines:

```bash
# Auto-detect engines (Blunder NNUE + HC, plus Stockfish if installed)
python scripts/run-tournament.py --games 100

# Gauntlet: Blunder vs all opponents
python scripts/run-tournament.py --games 100 --gauntlet

# Custom engine list
python scripts/run-tournament.py --engines engines.json --games 200
```

Engine config file format (`engines.json`):
```json
[
    {
        "name": "Blunder-NNUE",
        "cmd": "build/dev/blunder.exe",
        "proto": "xboard",
        "args": ["--xboard", "--nnue", "weights/nnue_v002.bin"]
    },
    {
        "name": "Stockfish-Skill5",
        "cmd": "stockfish.exe",
        "proto": "uci",
        "options": {"Skill Level": "5", "Threads": "1"}
    }
]
```

### SPRT regression test

Test a new build against a baseline for regressions:

```bash
python scripts/run-cutechess.py \
    --baseline baselines/blunder_old.exe \
    --candidate build/dev/blunder.exe
```

## Notes

- Model weights are NOT committed to git (large binary files)
- Each version should be documented with training parameters and results
- Keep the best-performing model for regression testing
- More training data generally helps more than more epochs
