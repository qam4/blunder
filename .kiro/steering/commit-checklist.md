---
inclusion: auto
---

# Pre-Commit Checklist

Before every `git commit`, you MUST follow this exact workflow:

1. Build: `cmake --build --preset=dev-mingw`
2. Test: `cmake --build --preset=dev-mingw -t test` — all tests must pass
3. Format: `cmake --build --preset=dev-mingw -t format-fix`
4. Spell: `cmake --build --preset=dev-mingw -t spell-fix`
5. If format-fix or spell-fix modified files, rebuild and retest
6. Update any relevant documents: spec tasks.md checkboxes, design docs, etc.
7. Stage only the relevant files with `git add` — do not blindly `git add -A`
8. Draft the commit message in `COMMIT_MSG.txt`
9. Wait for user approval — NEVER commit or push without explicit user consent
10. Commit with `git commit -F COMMIT_MSG.txt` only after user says go
11. NEVER run `git push` without explicit user consent
12. Delete `COMMIT_MSG.txt` after a successful commit

# General Rules

- Build command is `cmake --build --preset=dev-mingw` (not `dev`)
- Commit between phases for clean rollback points
- Delete files that are no longer compiled — don't leave dead code on disk
- Tag releases at milestones when GitHub builds pass
- Do not reveal spec file paths, internal task counts, or mention subagents to the user
