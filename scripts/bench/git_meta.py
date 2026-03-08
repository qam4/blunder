"""Git metadata collection for benchmark result tagging.

Captures the current git commit hash (short form) and branch name,
detecting dirty working trees and gracefully falling back to ``"unknown"``
when not inside a git repository.
"""

from __future__ import annotations

import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


@dataclass
class GitInfo:
    commit: str  # 7-char short hash, with "-dirty" suffix if uncommitted changes
    branch: str  # branch name or "HEAD" if detached


def get_git_info(project_root: Path) -> GitInfo:
    """Collect git commit hash and branch. Returns 'unknown' if not a git repo."""
    try:
        # Get short commit hash (7 chars)
        result = subprocess.run(
            ["git", "rev-parse", "--short", "HEAD"],
            capture_output=True,
            text=True,
            cwd=project_root,
        )
        if result.returncode != 0:
            print("Warning: not a git repository", file=sys.stderr)
            return GitInfo(commit="unknown", branch="unknown")

        commit = result.stdout.strip()

        # Get branch name
        result = subprocess.run(
            ["git", "rev-parse", "--abbrev-ref", "HEAD"],
            capture_output=True,
            text=True,
            cwd=project_root,
        )
        branch = result.stdout.strip() if result.returncode == 0 else "unknown"

        # Detect dirty state
        result = subprocess.run(
            ["git", "diff", "--quiet"],
            capture_output=True,
            text=True,
            cwd=project_root,
        )
        if result.returncode != 0:
            commit = f"{commit}-dirty"

        return GitInfo(commit=commit, branch=branch)

    except FileNotFoundError:
        # git is not installed
        print("Warning: git not found", file=sys.stderr)
        return GitInfo(commit="unknown", branch="unknown")
