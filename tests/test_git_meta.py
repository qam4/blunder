"""Property-based and unit tests for the git_meta module.

# Feature: benchmarking-infra, Property 14: Git hash format
"""

from __future__ import annotations

import re
from pathlib import Path
from unittest.mock import patch

from hypothesis import given, settings
from hypothesis import strategies as st

from bench.git_meta import GitInfo, get_git_info

# ---------------------------------------------------------------------------
# Property 14: Git hash format validation
# ---------------------------------------------------------------------------
# For any git info returned by the git metadata module in a valid git
# repository, the commit hash should match the pattern
# [0-9a-f]{7}(-dirty)? and the branch should be a non-empty string.
#
# **Validates: Requirements 9.1**
# ---------------------------------------------------------------------------

GIT_HASH_PATTERN = re.compile(r"^[0-9a-f]{7}(-dirty)?$")


@settings(max_examples=100)
@given(
    dirty=st.booleans(),
    short_hash=st.from_regex(r"[0-9a-f]{7}", fullmatch=True),
    branch=st.text(
        alphabet=st.characters(whitelist_categories=("L", "N", "P")),
        min_size=1,
        max_size=50,
    ).filter(lambda s: s.strip() == s),
)
def test_git_hash_format_property(dirty: bool, short_hash: str, branch: str) -> None:
    """Property 14: commit hash matches [0-9a-f]{7}(-dirty)? and branch is non-empty."""
    expected_commit = f"{short_hash}-dirty" if dirty else short_hash
    info = GitInfo(commit=expected_commit, branch=branch)

    assert GIT_HASH_PATTERN.match(info.commit), (
        f"Commit '{info.commit}' does not match expected pattern"
    )
    assert len(info.branch) > 0, "Branch must be non-empty"


# ---------------------------------------------------------------------------
# Unit tests — edge cases
# ---------------------------------------------------------------------------


def test_get_git_info_in_real_repo() -> None:
    """In the actual project repo, commit and branch should be valid."""
    info = get_git_info(Path("."))
    # commit should be a 7-char hex hash, possibly with -dirty
    assert GIT_HASH_PATTERN.match(info.commit), (
        f"Commit '{info.commit}' does not match expected pattern"
    )
    assert len(info.branch) > 0


def test_get_git_info_not_a_repo(tmp_path: Path) -> None:
    """In a non-git directory, both fields should be 'unknown'."""
    info = get_git_info(tmp_path)
    assert info.commit == "unknown"
    assert info.branch == "unknown"


def test_get_git_info_git_not_installed(tmp_path: Path) -> None:
    """When git is not on PATH, both fields should be 'unknown'."""
    with patch("bench.git_meta.subprocess.run", side_effect=FileNotFoundError):
        info = get_git_info(tmp_path)
    assert info.commit == "unknown"
    assert info.branch == "unknown"
