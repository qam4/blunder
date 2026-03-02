# NNUE Model Weights

This directory contains trained NNUE (Efficiently Updatable Neural Network) evaluation weights for the Blunder chess engine.

## Versioning Scheme

Weights are versioned as `nnue_vXXX.bin` where XXX is a zero-padded version number:
- `nnue_v001.bin` - Initial model (300 positions, 5 epochs)
- `nnue_v002.bin` - Improved model (planned: 10k+ games, 50+ epochs)
- etc.

## Training a New Model

1. **Generate training data:**
   ```bash
   ./blunder --selfplay --selfplay-games 10000 --selfplay-depth 6 --selfplay-output training_data.bin
   ```

2. **Train the model:**
   ```bash
   python scripts/train_nnue.py --input training_data.bin --output weights/nnue_vXXX.bin --epochs 50
   ```

3. **Test the model:**
   ```bash
   python scripts/compare_nnue_vs_handcrafted.py --nnue weights/nnue_vXXX.bin --games 100
   ```

## Model Architecture

- **Input:** 768 features (HalfKP encoding: 6 piece types × 2 colors × 64 squares)
- **Hidden layers:** 768 → 256 → 32 → 32 → 1
- **Output:** Evaluation score (centipawns)
- **Activation:** ReLU

## Usage

Load a trained model with the `--nnue` flag:
```bash
./blunder --nnue weights/nnue_v001.bin
```

The engine will fall back to HandCrafted evaluation if no NNUE weights are loaded.

## Notes

- Model weights are **not** committed to git (they're large binary files)
- Each version should be documented with training parameters and performance metrics
- Keep the best-performing models for regression testing
