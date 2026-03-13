---
inclusion: auto
description: Blunder-specific build commands and release rules
---

# Build & Test Commands

1. Build: `cmake --build --preset=dev`
2. Test: `ctest --preset=dev --output-on-failure`
3. Format: `cmake --build --preset=dev -t format-fix`
4. Spell: `cmake --build --preset=dev -t spell-fix`

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
