# blunder

This is the blunder project.

# Architecture

See the [ARCHITECTURE](docs/ARCHITECTURE.md) document for a description of the
search algorithms, move ordering, and evaluation used in the engine.

# Building and installing

See the [BUILDING](BUILDING.md) document.

# Strength

## Benchmark Results

Results from `bench run` on AMD EPYC 9R14. Three search modes:

- `nodes` — fixed node budget per position (hardware-independent search quality)
- `time1s` — 1 second per position (real-world strength, benefits from NPS)
- `depth8` — fixed depth 8 (fast sanity check)

To regenerate all results: `bench run --suite all --mode all`
(report written to `scripts/output/results.txt`).

### HCE (Hand-Crafted Evaluation)

| Suite | Mode | Score | Pct | ELO | NPS | Commit |
|-------|------|-------|-----|-----|-----|--------|
| WAC | nodes | 216/300 | 72.0% | 2962 | 2,228,384 | v0.8.0 |
| WAC | time1s | 240/300 | 80.0% | 3318 | 2,791,969 | v0.8.0 |
| WAC | depth8 | 44/300 | 14.7% | 410 | 1,298,354 | v0.7.0 |
| STS | nodes | 48,689/118,800 | 41.0% | 1581 | 1,492,619 | v0.7.0 |
| STS | time1s | 75,032/118,800 | 63.2% | 2569 | 1,656,549 | v0.7.0 |
| STS | depth8 | 9,396/118,800 | 7.9% | 109 | 561,226 | v0.7.0 |

### NNUE

| Suite | Mode | Score | Pct | ELO | NPS | Commit |
|-------|------|-------|-----|-----|-----|--------|
| WAC | nodes | 187/300 | 62.3% | 2532 | — | v0.7.0 |
| WAC | time1s | 206/300 | 68.7% | 2814 | 1,928,050 | v0.7.0 |
| WAC | depth8 | 44/300 | 14.7% | 410 | 1,276,982 | v0.7.0 |
| STS | depth8 | 9,396/118,800 | 7.9% | 109 | 563,795 | v0.7.0 |

NNUE results are from an early integration and do not yet reflect
tuned weights. HCE currently outperforms NNUE on WAC.

### Stockfish-calibrated Elo

Estimated via binary search against Stockfish 16.1 `UCI_LimitStrength`
(20 games per level, tc=5+0.05, `bench elo` subcommand):

| Evaluator | Estimated Elo | Commit |
|-----------|---------------|--------|
| HCE | ~2553 ±45 | v0.8.0 |

## Performance

Blunder targets ~2.5M+ NPS on modern x86_64 hardware. Key optimizations:

- Hardware `POPCNT` via `-mpopcnt` (GCC/Clang on x86_64)
- Critical accessors inlined in headers for cross-TU optimization
- Color-aggregate bitboards for occupied/friendly masks

Run `bench profile` to generate a flamegraph.
See [ARCHITECTURE.md](docs/ARCHITECTURE.md) for profiling details.

# Contributing

See the [CONTRIBUTING](CONTRIBUTING.md) document.

# Licensing

<!--
Please go to https://choosealicense.com/licenses/ and choose a license that
fits your needs. The recommended license for a project of this type is the
GNU AGPLv3.
-->


