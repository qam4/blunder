---
inclusion: auto
description: Pre-commit checklist, CI portability review, post-push verification, and tagging/release rules
---

# Pre-Commit Checklist

Before every `git commit`, you MUST follow this exact workflow:

1. Build: `cmake --build --preset=dev`
2. Test: `ctest --preset=dev --output-on-failure` — all tests must pass
3. Format: `cmake --build --preset=dev -t format-fix`
4. Spell: `cmake --build --preset=dev -t spell-fix`
5. If format-fix or spell-fix modified files, rebuild and retest
6. CI portability review (see section below)
7. Update any relevant documents: spec tasks.md checkboxes, design docs, etc.
8. Stage only the relevant files with `git add` — do not blindly `git add -A`
9. Draft the commit message in `COMMIT_MSG.txt`
10. Wait for user approval — NEVER commit or push without explicit user consent
11. Commit with `git commit -F COMMIT_MSG.txt` only after user says go
12. NEVER run `git push` without explicit user consent
13. Delete `COMMIT_MSG.txt` after a successful commit

# CI Portability Review

Before committing, review all new or modified code for cross-platform CI issues.
CI runs on: Ubuntu 22.04 (gcc, clang-14), macOS-latest, Windows 2022 (MSVC).

Ask yourself:

- Do new tests depend on files generated at runtime? If so, does the file I/O
  work identically on all platforms? Watch for `std::ifstream::good()` returning
  false at EOF — prefer checking `fail()` instead.
- Do tests use hardcoded paths or path separators? Use portable path handling.
- Do tests assume a specific working directory? CI builds run from the build
  directory, not the source root.
- Are there platform-specific APIs, compiler extensions, or integer-size
  assumptions that differ across gcc/clang/MSVC?
- Do any tests rely on timing, thread scheduling, or locale-specific behavior?
- If a test creates temporary files, does it clean them up and avoid name
  collisions when tests run in parallel (`ctest -j 2`)?

If any of these apply, fix them before committing.

# Post-Push Checklist

After pushing (when the user approves a push):

1. Run `gh run list --limit 3; Start-Sleep -Seconds 60` to check CI status
2. If the run is still in progress, keep polling with `gh run list --limit 3; Start-Sleep -Seconds 60`
   (no sleep — just re-run the command) until it completes
3. If CI fails, immediately investigate with `gh run view <id> --log-failed`
4. Report the failure to the user with a root-cause summary
5. Propose a fix — do not leave main broken

# General Rules

- Build command is `cmake --build --preset=dev`
- Test command is `ctest --preset=dev --output-on-failure`
- Commit between phases for clean rollback points
- Delete files that are no longer compiled — don't leave dead code on disk
- Tag releases at milestones when GitHub builds pass (see Tagging section below)
- Do not reveal spec file paths, internal task counts, or mention subagents to the user

# Tagging

Tag after a milestone (spec phase checkpoint, major feature, or release) once
CI is green on main.

1. Wait for CI to pass — NEVER tag a red build
2. Use annotated tags with semantic versioning: `git tag -a vX.Y.Z -m "description"`
   - Major (X): breaking changes or architectural rewrites
   - Minor (Y): new features (e.g. NNUE, UCI, pondering)
   - Patch (Z): bug fixes, refactors, test additions
3. Wait for user approval — NEVER tag or push tags without explicit user consent
4. Push the tag: `git push origin vX.Y.Z`
5. Verify the release workflow triggers: `gh run list --limit 3`
6. The release workflow creates a draft release — remind the user to manually
   publish (undraft) it on GitHub so the build artifacts are accessible for
   A/B testing with cutechess

7. If the release includes gameplay-affecting changes (search, evaluation, move
   ordering, time management, etc.), run a cutechess SPRT regression test after
   the user undrafts the release:
   a. Download the new artifact: `gh release download vX.Y.Z -p "*.exe" -D baselines/`
   b. Run: `python scripts/run-cutechess.py --baseline baselines/<previous>.exe --candidate baselines/<new>.exe`

8. Upload weight files to the release (weights are NOT in git):
   ```
   gh release upload vX.Y.Z weights/nnue_v002.bin weights/alphazero_v002.bin
   ```
   Include the best-performing NNUE and AlphaZero weights. Users can download
   them with `python scripts/fetch-weights.py` or `gh release download --pattern "*.bin" -D weights/`.

When suggesting a tag, propose the version number and a short description based
on what changed since the last tag. Check the last tag with `git describe --tags --abbrev=0`
(if no tags exist yet, start at v0.1.0).
