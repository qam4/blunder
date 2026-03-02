---
inclusion: auto
description: Pre-commit checklist, CI portability review, and post-push verification rules
---

# Pre-Commit Checklist

Before every `git commit`, you MUST follow this exact workflow:

1. Build: `cmake --build --preset=dev-mingw`
2. Test: `cmake --build --preset=dev-mingw -t test` — all tests must pass
3. Format: `cmake --build --preset=dev-mingw -t format-fix`
4. Spell: `cmake --build --preset=dev-mingw -t spell-fix`
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

1. Run `gh run list --limit 3` to check CI status
2. If CI is still running, wait a reasonable time and check again
3. If CI fails, immediately investigate with `gh run view <id> --log-failed`
4. Report the failure to the user with a root-cause summary
5. Propose a fix — do not leave main broken

# General Rules

- Build command is `cmake --build --preset=dev-mingw` (not `dev`)
- Commit between phases for clean rollback points
- Delete files that are no longer compiled — don't leave dead code on disk
- Tag releases at milestones when GitHub builds pass
- Do not reveal spec file paths, internal task counts, or mention subagents to the user
