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

### nnue_mcts_v001.bin
- 5,799 positions from 50 MCTS games (100 sims/move), 10 epochs, val loss 19092.81
- Result: 9-11 vs HandCrafted (Elo -34.9 ±164.5, essentially even — small dataset)
```bash
./blunder --selfplay --mcts --selfplay-games 50 --mcts-simulations 100 --selfplay-output mcts_training.bin
python scripts/train_nnue.py --input mcts_training.bin --output weights/nnue_mcts_v001.bin --format mcts --epochs 10
python scripts/compare_nnue_vs_handcrafted.py --nnue weights/nnue_mcts_v001.bin --games 20 --tc "5+0.1"
```

### alphazero_v001.bin
- 6,360 positions from 50 MCTS games (100 sims/move, uniform priors), 10 epochs, val loss 2.7742
- Dual-head architecture: 768→256→128 trunk, 128→32→1 value, 128→4096 policy (762k params)
- Result: 0-20 vs HandCrafted (expected for first-iteration model with uniform priors)
```bash
./blunder --selfplay --mcts --selfplay-games 50 --mcts-simulations 100 --selfplay-output mcts_training.bin
python scripts/train_alphazero.py --input mcts_training.bin --output weights/alphazero_v001.bin --epochs 10
python scripts/compare_nnue_vs_handcrafted.py --alphazero --nnue weights/alphazero_v001.bin --games 20 --tc "60+1" --mcts-simulations 200
```

### alphazero_v002.bin
- 63,964 positions from 500 MCTS games (200 sims/move, uniform priors), 20 epochs, batch 256, lr 0.001
- Dual-head architecture: 768→256→128 trunk, 128→32→1 value, 128→4096 policy (762k params)
- Result: 0-20 vs HandCrafted (MCTS with 200 sims + untrained policy still too weak vs alpha-beta)
- Note: fixed stack smashing bug — AlphaZero mode was incorrectly loading single-head NNUE weights
```bash
./blunder --selfplay --mcts --selfplay-games 500 --mcts-simulations 200 --selfplay-output mcts_training_v002.bin
python scripts/train_alphazero.py --input mcts_training_v002.bin --output weights/alphazero_v002.bin --epochs 20 --batch-size 256 --learning-rate 0.001
python scripts/compare_nnue_vs_handcrafted.py --alphazero --nnue weights/alphazero_v002.bin --games 20 --tc "60+1" --mcts-simulations 200
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

### Method C: MCTS Self-Play (AlphaZero-style)

Uses Monte Carlo Tree Search to generate training data with policy (visit
distribution) and value (game outcome) targets. The current 768→256→32→32→1
architecture only uses the value target; policy targets are stored for future
use with a dual-head network.

```bash
# 1. Generate MCTS training data
./blunder --selfplay --mcts \
          --selfplay-games 500 \
          --mcts-simulations 400 \
          --mcts-temperature 1.0 \
          --mcts-temp-drop 30 \
          --selfplay-output mcts_training.bin

# 2. Train (use --format mcts to read variable-length format)
python scripts/train_nnue.py \
    --input mcts_training.bin \
    --output weights/nnue_mcts_vXXX.bin \
    --format mcts \
    --epochs 50 --batch-size 256 --learning-rate 0.001

# 3. Test
python scripts/compare_nnue_vs_handcrafted.py \
    --nnue weights/nnue_mcts_vXXX.bin --games 100
```

MCTS parameters:
- `--mcts-simulations`: More sims = stronger play/better labels (200-800)
- `--mcts-temperature`: Exploration temperature (1.0 = diverse, 0.1 = greedy)
- `--mcts-temp-drop`: Ply at which temperature drops to near-zero (20-40)
- `--mcts-cpuct`: Exploration constant (default 1.41)
- `--mcts-dirichlet-alpha`: Root noise for exploration (0.3 typical, 0 to disable)
- `--mcts-dirichlet-eps`: Noise mixing fraction (0.25 typical)

### Method E: AlphaZero Dual-Head Training

Trains a dual-head network (shared trunk + value head + policy head) using
MCTS self-play data. This is the AlphaZero-style training pipeline where
the policy head learns from MCTS visit distributions and the value head
learns from game outcomes.

Architecture: 768 → 256 → 128 (shared trunk), 128 → 32 → 1 (value head,
tanh), 128 → 4096 (policy head, masked softmax). Total: ~762k parameters.

```bash
# 1. Generate MCTS self-play data with policy targets
./blunder --selfplay --mcts \
          --selfplay-games 500 \
          --mcts-simulations 400 \
          --mcts-temperature 1.0 \
          --mcts-temp-drop 30 \
          --selfplay-output mcts_training.bin

# 2. Train dual-head network
python scripts/train_alphazero.py \
    --input mcts_training.bin \
    --output weights/alphazero_v001.bin \
    --epochs 20 --batch-size 256 --learning-rate 0.001

# 3. Load in engine with --alphazero flag
./blunder --alphazero --nnue weights/alphazero_v001.bin

# 4. Test against HandCrafted
python scripts/compare_nnue_vs_handcrafted.py \
    --nnue weights/alphazero_v001.bin --games 100
```

Training options:
- `--epochs`: Training iterations (10-50)
- `--batch-size`: Batch size (128-512)
- `--learning-rate`: Start at 0.001, reduce if loss oscillates

The combined loss is MSE (value head vs game outcome) + cross-entropy
(policy head vs MCTS visit distribution). The script saves the best model
based on validation loss.

For iterative improvement, re-generate self-play data using the trained
weights so MCTS benefits from the policy head, then retrain. See
`scripts/alphazero_loop.py` for automation.

### Method F: Iterative AlphaZero Training Loop

Automates the full AlphaZero self-improvement cycle: generate MCTS self-play
data with the current network, train on that data, evaluate, and repeat. Each
iteration produces a stronger network because the policy head guides MCTS to
explore better moves, generating higher-quality training data.

```bash
# Basic: 5 iterations, 100 games each, with evaluation
python scripts/alphazero_loop.py \
    --iterations 5 \
    --games 100 \
    --simulations 400 \
    --epochs 10

# Full run: more games and simulations for stronger training
python scripts/alphazero_loop.py \
    --iterations 10 \
    --games 500 \
    --simulations 800 \
    --epochs 20 \
    --eval-games 50

# Skip evaluation (faster iterations)
python scripts/alphazero_loop.py \
    --iterations 5 \
    --games 200 \
    --eval-games 0
```

How it works:
1. **Iteration 1**: MCTS self-play with uniform priors (no network yet).
   Generates baseline training data.
2. **Train**: Dual-head network learns value and policy from the data.
3. **Iteration 2+**: MCTS self-play uses `--alphazero --nnue <weights>` so
   the policy head guides exploration and the value head evaluates leaves.
4. **Evaluate** (optional): Cutechess match of new weights vs HandCrafted.
5. **Repeat**: Each iteration builds on the previous network's knowledge.

Script options:
- `--engine`: Path to blunder executable (default: build/dev/blunder.exe)
- `--iterations`: Number of training iterations (default: 5)
- `--games`: Self-play games per iteration (default: 100)
- `--simulations`: MCTS simulations per move (default: 400)
- `--epochs`: Training epochs per iteration (default: 10)
- `--eval-games`: Cutechess games for evaluation (0 to skip, default: 20)
- `--output-dir`: Directory for weights and data (default: weights/)

Output structure:
```
weights/
  alphazero_latest.bin              # Always points to most recent weights
  alphazero_run_YYYYMMDD_HHMMSS/
    selfplay_iter001.bin            # Training data from iteration 1
    alphazero_iter001.bin           # Weights after iteration 1
    selfplay_iter002.bin            # Training data from iteration 2
    alphazero_iter002.bin           # Weights after iteration 2
    ...
```

### Method D: Hybrid (best quality)

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
