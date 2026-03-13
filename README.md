# blunder

This is the blunder project.

# Architecture

See the [ARCHITECTURE](docs/ARCHITECTURE.md) document for a description of the
search algorithms, move ordering, and evaluation used in the engine.

# Building and installing

See the [BUILDING](BUILDING.md) document.

# Strength

Measured via binary search against Stockfish 16.1 with `UCI_LimitStrength`
(20 games per Elo level, tc=5+0.05, `bench elo` subcommand):

| Evaluator | Estimated Elo | STS Score | WAC Score |
|-----------|---------------|-----------|-----------|
| HCE       | ~2212         | —         | 60.3% (181/300) |

STS Elo estimate: 2443 (static positions, no time pressure).

# Contributing

See the [CONTRIBUTING](CONTRIBUTING.md) document.

# Licensing

<!--
Please go to https://choosealicense.com/licenses/ and choose a license that
fits your needs. The recommended license for a project of this type is the
GNU AGPLv3.
-->


