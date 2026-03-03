#!/usr/bin/env python3
"""
AlphaZero Dual-Head Training Script for Blunder Chess Engine

Trains a dual-head neural network with shared trunk, value head, and
policy head using MCTS self-play data.

Architecture:
  Shared trunk:  768 -> 256 (ReLU) -> 128 (ReLU)
  Value head:    128 -> 32 (ReLU) -> 1 (tanh)    => output in [-1, +1]
  Policy head:   128 -> 4096 (linear)             => masked softmax

The policy head uses 64x64 from-to encoding (4096 outputs). During
training, only legal move slots contribute to the cross-entropy loss.

Usage:
    python scripts/train_alphazero.py --input mcts_data.bin --output weights.bin
"""

import argparse
import struct
import torch
import torch.nn as nn
import torch.nn.functional as F
import torch.optim as optim
from torch.utils.data import Dataset, DataLoader
from pathlib import Path


class DualHeadNNUE(nn.Module):
    """Dual-head network: shared trunk + value head + policy head.

    Architecture matches DualHeadNetwork in source/DualHeadNetwork.h:
      Shared trunk:  768 -> 256 (ReLU) -> 128 (ReLU)
      Value head:    128 -> 32 (ReLU) -> 1 (tanh)
      Policy head:   128 -> 4096 (linear, masked softmax at inference)
    """

    def __init__(self):
        super(DualHeadNNUE, self).__init__()
        # Shared trunk
        self.trunk1 = nn.Linear(768, 256)
        self.trunk2 = nn.Linear(256, 128)
        # Value head
        self.value1 = nn.Linear(128, 32)
        self.value2 = nn.Linear(32, 1)
        # Policy head: 128 -> 4096 (64*64 from-to pairs)
        self.policy = nn.Linear(128, 4096)

    def forward(self, features, move_mask=None):
        """Forward pass through the dual-head network.

        Args:
            features: Tensor of shape (batch, 768) — binary HalfKP features.
            move_mask: Optional bool tensor (batch, 4096). True = legal slot.
                       Used to mask policy logits before softmax.

        Returns:
            value: Tensor (batch, 1) in [-1, +1] (tanh output).
            policy: Tensor (batch, 4096) — softmax probabilities over
                    legal moves (masked slots get ~0 probability).
        """
        # Shared trunk
        x = F.relu(self.trunk1(features))
        x = F.relu(self.trunk2(x))

        # Value head
        v = F.relu(self.value1(x))
        v = torch.tanh(self.value2(v))

        # Policy head
        p = self.policy(x)
        if move_mask is not None:
            p = p.masked_fill(~move_mask, float('-inf'))
        p = F.softmax(p, dim=-1)

        return v, p


class MCTSPolicyDataset(Dataset):
    """Dataset for MCTS self-play data with policy and value targets.

    Reads the variable-length binary format produced by SelfPlay.cpp
    and returns (features, policy_target, move_mask, value_target) tuples.

    Binary format per entry:
        768 floats:       features (HalfKP binary features)
        1 int:            num_moves
        num_moves floats: policy (normalized visit distribution)
        num_moves ints:   move_indices (from*64+to, each in [0, 4096))
        1 float:          value (game outcome: +1=win, 0=draw, -1=loss)
    """

    def __init__(self, data_path: str):
        """Load MCTS training data from binary file.

        Args:
            data_path: Path to binary training data file.
        """
        with open(data_path, 'rb') as f:
            data = f.read()

        self.features_list = []
        self.policy_targets = []
        self.move_masks = []
        self.value_targets = []

        offset = 0
        features_bytes = 768 * 4

        while offset + features_bytes + 4 < len(data):
            # Read 768 features
            features = struct.unpack_from('768f', data, offset)
            offset += features_bytes

            # Read num_moves
            num_moves = struct.unpack_from('i', data, offset)[0]
            offset += 4

            if num_moves <= 0 or num_moves > 256:
                break  # corrupt data guard

            # Read policy (visit distribution) and move_indices
            policy_vals = struct.unpack_from(
                f'{num_moves}f', data, offset
            )
            offset += num_moves * 4

            move_indices = struct.unpack_from(
                f'{num_moves}i', data, offset
            )
            offset += num_moves * 4

            # Read value (game outcome)
            value = struct.unpack_from('f', data, offset)[0]
            offset += 4

            # Build 4096-dim policy target and move mask
            policy_4096 = [0.0] * 4096
            mask_4096 = [False] * 4096
            for i in range(num_moves):
                idx = move_indices[i]
                if 0 <= idx < 4096:
                    policy_4096[idx] = policy_vals[i]
                    mask_4096[idx] = True

            self.features_list.append(features)
            self.policy_targets.append(policy_4096)
            self.move_masks.append(mask_4096)
            self.value_targets.append(value)

        num_entries = len(self.features_list)
        print(f"Loaded {num_entries} MCTS positions from {data_path}")

        self.features_t = torch.tensor(
            self.features_list, dtype=torch.float32
        )
        self.policy_t = torch.tensor(
            self.policy_targets, dtype=torch.float32
        )
        self.mask_t = torch.tensor(
            self.move_masks, dtype=torch.bool
        )
        self.value_t = torch.tensor(
            self.value_targets, dtype=torch.float32
        ).unsqueeze(1)

        print(f"Features shape: {self.features_t.shape}")
        print(f"Value range: [{self.value_t.min():.2f}, "
              f"{self.value_t.max():.2f}]")

    def __len__(self):
        return len(self.features_list)

    def __getitem__(self, idx):
        return (
            self.features_t[idx],
            self.policy_t[idx],
            self.mask_t[idx],
            self.value_t[idx],
        )


def dual_head_loss(pred_value, pred_policy, target_value, target_policy,
                   move_mask):
    """Combined loss for dual-head network.

    Args:
        pred_value:    (batch, 1) predicted value in [-1, +1].
        pred_policy:   (batch, 4096) predicted policy (softmax output).
        target_value:  (batch, 1) game outcome in {-1, 0, +1}.
        target_policy: (batch, 4096) MCTS visit distribution.
        move_mask:     (batch, 4096) bool mask of legal moves.

    Returns:
        total_loss, value_loss, policy_loss as scalars.
    """
    # Value loss: MSE between predicted value and game outcome
    value_loss = F.mse_loss(pred_value, target_value)

    # Policy loss: cross-entropy between predicted and MCTS distribution
    # Only consider legal move slots. Add small epsilon to avoid log(0).
    eps = 1e-8
    log_pred = torch.log(pred_policy + eps)
    # Cross-entropy: -sum(target * log(pred)) over legal moves
    policy_loss = -(target_policy * log_pred * move_mask).sum(dim=1).mean()

    total_loss = value_loss + policy_loss
    return total_loss, value_loss, policy_loss


def export_weights(model: DualHeadNNUE, output_path: str):
    """Export trained weights in binary format for DualHeadNetwork::load().

    Layer order (must match C++ load()):
      1. trunk1 weights (768*256 floats) then trunk1 bias (256 floats)
      2. trunk2 weights (256*128 floats) then trunk2 bias (128 floats)
      3. value1 weights (128*32 floats)  then value1 bias (32 floats)
      4. value2 weights (32 floats)      then value2 bias (1 float)
      5. policy weights (128*4096 floats) then policy bias (4096 floats)

    PyTorch nn.Linear stores weights as [out_features, in_features].
    The C++ code reads w[out][in] in row-major order, which matches
    PyTorch's layout directly — so we flatten and write as-is.
    """
    print(f"\nExporting weights to {output_path}")

    with open(output_path, 'wb') as f:
        for name, param in model.named_parameters():
            weights = param.detach().cpu().numpy().flatten()
            print(f"  {name}: {len(weights)} values")
            for w in weights:
                f.write(struct.pack('f', w))

    total = sum(p.numel() for p in model.parameters())
    print(f"Exported {total} total parameters")


def train_alphazero(
    data_path: str,
    output_path: str,
    epochs: int = 20,
    batch_size: int = 256,
    learning_rate: float = 0.001,
    device: str = 'cuda' if torch.cuda.is_available() else 'cpu',
):
    """Train dual-head network on MCTS self-play data."""

    print(f"Using device: {device}")

    dataset = MCTSPolicyDataset(data_path)

    # Train/validation split (90/10)
    train_size = int(0.9 * len(dataset))
    val_size = len(dataset) - train_size
    train_dataset, val_dataset = torch.utils.data.random_split(
        dataset, [train_size, val_size]
    )

    train_loader = DataLoader(
        train_dataset, batch_size=batch_size, shuffle=True
    )
    val_loader = DataLoader(
        val_dataset, batch_size=batch_size, shuffle=False
    )

    print(f"Train set: {train_size} positions")
    print(f"Validation set: {val_size} positions")

    model = DualHeadNNUE().to(device)
    print(f"\nModel architecture:")
    print(model)
    total_params = sum(p.numel() for p in model.parameters())
    print(f"Total parameters: {total_params}")

    optimizer = optim.Adam(model.parameters(), lr=learning_rate)

    print(f"\nTraining for {epochs} epochs...")
    best_val_loss = float('inf')

    for epoch in range(epochs):
        # --- Training ---
        model.train()
        train_total = 0.0
        train_vloss = 0.0
        train_ploss = 0.0

        for features, policy_tgt, mask, value_tgt in train_loader:
            features = features.to(device)
            policy_tgt = policy_tgt.to(device)
            mask = mask.to(device)
            value_tgt = value_tgt.to(device)

            optimizer.zero_grad()
            pred_v, pred_p = model(features, mask)
            loss, vl, pl = dual_head_loss(
                pred_v, pred_p, value_tgt, policy_tgt, mask
            )
            loss.backward()
            optimizer.step()

            train_total += loss.item()
            train_vloss += vl.item()
            train_ploss += pl.item()

        n = len(train_loader)
        train_total /= n
        train_vloss /= n
        train_ploss /= n

        # --- Validation ---
        model.eval()
        val_total = 0.0
        val_vloss = 0.0
        val_ploss = 0.0

        with torch.no_grad():
            for features, policy_tgt, mask, value_tgt in val_loader:
                features = features.to(device)
                policy_tgt = policy_tgt.to(device)
                mask = mask.to(device)
                value_tgt = value_tgt.to(device)

                pred_v, pred_p = model(features, mask)
                loss, vl, pl = dual_head_loss(
                    pred_v, pred_p, value_tgt, policy_tgt, mask
                )
                val_total += loss.item()
                val_vloss += vl.item()
                val_ploss += pl.item()

        m = len(val_loader)
        val_total /= m
        val_vloss /= m
        val_ploss /= m

        print(
            f"Epoch {epoch+1}/{epochs} — "
            f"Train: {train_total:.4f} "
            f"(v={train_vloss:.4f} p={train_ploss:.4f}) | "
            f"Val: {val_total:.4f} "
            f"(v={val_vloss:.4f} p={val_ploss:.4f})"
        )

        if val_total < best_val_loss:
            best_val_loss = val_total
            export_weights(model, output_path)
            print("  -> New best model saved!")

    print(f"\nTraining complete! Best val loss: {best_val_loss:.4f}")
    print(f"Weights saved to: {output_path}")


def main():
    parser = argparse.ArgumentParser(
        description='Train AlphaZero dual-head network for Blunder'
    )
    parser.add_argument(
        '--input', type=str, default='mcts_training.bin',
        help='Input MCTS training data file (default: mcts_training.bin)'
    )
    parser.add_argument(
        '--output', type=str, default='alphazero_weights.bin',
        help='Output weights file (default: alphazero_weights.bin)'
    )
    parser.add_argument(
        '--epochs', type=int, default=20,
        help='Number of training epochs (default: 20)'
    )
    parser.add_argument(
        '--batch-size', type=int, default=256,
        help='Batch size (default: 256)'
    )
    parser.add_argument(
        '--learning-rate', type=float, default=0.001,
        help='Learning rate (default: 0.001)'
    )

    args = parser.parse_args()

    if not Path(args.input).exists():
        print(f"Error: Input file '{args.input}' not found!")
        print("\nGenerate MCTS training data first:")
        print("  ./blunder --selfplay --mcts "
              "--selfplay-games 500 "
              "--mcts-simulations 400 "
              "--selfplay-output mcts_training.bin")
        return

    train_alphazero(
        data_path=args.input,
        output_path=args.output,
        epochs=args.epochs,
        batch_size=args.batch_size,
        learning_rate=args.learning_rate,
    )


if __name__ == '__main__':
    main()
