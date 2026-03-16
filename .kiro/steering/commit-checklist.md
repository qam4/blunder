---
inclusion: auto
description: Blunder-specific build commands and release rules
---

# Build & Test Commands

1. Build: `cmake --build --preset=dev`
2. Test: `ctest --preset=dev --output-on-failure`
3. Format: `cmake --build --preset=dev -t format-fix`
4. Spell: `cmake --build --preset=dev -t spell-fix`

# Gameplay-Affecting Changes

If the commit touches search, evaluation, move ordering, move generation,
or time management, run a regression gauntlet before committing:

```bash
# Save the current binary as baseline before building the change
cp build/dev/blunder baselines/blunder-before

# Build with the change, then run a quick SPRT regression test
bench gauntlet --type sprt --baseline blunder-before --candidate blunder-hce --rounds 50 --tc 5+0.05
```

The baseline engine entry `blunder-before` must be added to bench-config.json
pointing to the saved binary. If the candidate loses significantly (Elo < -20),
investigate before committing.

# CI Platforms

Ubuntu 22.04 (gcc, clang-14), macOS-latest, Windows 2022 (MSVC).

Watch for `std::ifstream::good()` returning false at EOF — prefer `fail()`.

# Release Rules

- When bumping the version, update ALL version references in the codebase:
  - `CMakeLists.txt` — `VERSION X.Y.Z`
  - `pyproject.toml` — `version = "X.Y.Z"`
  - `source/CoachDispatcher.cpp` — `serialize_pong("Blunder", "X.Y.Z")`
  - Git tag: `vX.Y.Z`

- The release workflow creates a draft release — remind the user to manually
  publish (undraft) it on GitHub so build artifacts are accessible for A/B testing

- If the release includes gameplay-affecting changes (search, evaluation, move
  ordering, time management), run a cutechess SPRT regression test after undraft:
  ```
  gh release download vX.Y.Z -p "*.exe" -D baselines/
  python scripts/run-cutechess.py --baseline baselines/<previous>.exe --candidate baselines/<new>.exe
  ```

- Upload weight files to the release (weights are NOT in git):
  ```
  gh release upload vX.Y.Z weights/nnue_v002.bin weights/alphazero_v002.bin
  ```
