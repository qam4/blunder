"""Property-based and unit tests for the csv_store module.

# Feature: benchmarking-infra, Property 8: CSV result round-trip
"""

from __future__ import annotations

import tempfile
from pathlib import Path

from hypothesis import given, settings
from hypothesis import strategies as st

from bench.csv_store import (
    CATEGORY_COLUMNS,
    CATEGORY_FILENAME,
    MAIN_COLUMNS,
    MAIN_FILENAME,
    append_category_results,
    append_main_result,
    get_previous_result,
    read_main_results,
)

# ---------------------------------------------------------------------------
# Strategies for generating valid benchmark rows
# ---------------------------------------------------------------------------

_safe_text = st.text(
    alphabet=st.characters(
        whitelist_categories=("L", "N"),
    ),
    min_size=1,
    max_size=20,
)

_timestamp_st = st.from_regex(
    r"20[0-9]{2}-[01][0-9]-[0-3][0-9] [0-2][0-9]:[0-5][0-9]:[0-5][0-9]",
    fullmatch=True,
)

_evaluator_st = st.sampled_from(["hce", "nnue", "alphazero"])
_mode_st = st.sampled_from(["nodes", "depth8", "time1s"])
_suite_st = st.sampled_from(["WAC", "STS"])
_passed_st = st.sampled_from(["True", "False"])


@st.composite
def main_row_strategy(draw: st.DrawFn) -> dict:
    """Generate a valid main benchmark result row."""
    return {
        "timestamp": draw(_timestamp_st),
        "commit": draw(st.from_regex(r"[0-9a-f]{7}(-dirty)?", fullmatch=True)),
        "branch": draw(_safe_text),
        "evaluator": draw(_evaluator_st),
        "mode": draw(_mode_st),
        "suite": draw(_suite_st),
        "score_pct": str(draw(st.floats(min_value=0, max_value=100, allow_nan=False, allow_infinity=False))),
        "score": str(draw(st.integers(min_value=0, max_value=10000))),
        "max_score": str(draw(st.integers(min_value=1, max_value=10000))),
        "elo": str(draw(st.integers(min_value=0, max_value=4000))),
        "nps": str(draw(st.integers(min_value=0, max_value=100000000))),
        "nodes": str(draw(st.integers(min_value=0, max_value=10**12))),
        "time_secs": str(draw(st.floats(min_value=0, max_value=100000, allow_nan=False, allow_infinity=False))),
        "passed": draw(_passed_st),
    }


# ---------------------------------------------------------------------------
# Property 8: CSV result round-trip
# ---------------------------------------------------------------------------
# For any valid benchmark result row, appending it to the CSV file and then
# reading back the last row should produce a dict with identical field values.
# The CSV header should always match the expected column order.
#
# **Validates: Requirements 4.1, 4.2, 4.5, 10.2**
# ---------------------------------------------------------------------------


@settings(max_examples=100)
@given(row=main_row_strategy())
def test_csv_main_round_trip(row: dict) -> None:
    """Property 8: appending a main result and reading it back preserves all fields."""
    with tempfile.TemporaryDirectory() as td:
        out = Path(td)
        append_main_result(out, row)
        results = read_main_results(out)
        assert len(results) >= 1
        last = results[-1]
        for col in MAIN_COLUMNS:
            assert last[col] == row[col], f"Mismatch on column '{col}': {last[col]!r} != {row[col]!r}"


@settings(max_examples=100)
@given(row=main_row_strategy())
def test_csv_header_matches_expected_columns(row: dict) -> None:
    """Property 8 (header check): CSV header matches the expected column order."""
    with tempfile.TemporaryDirectory() as td:
        out = Path(td)
        append_main_result(out, row)
        csv_path = out / MAIN_FILENAME
        with open(csv_path, encoding="utf-8") as fh:
            header_line = fh.readline().strip()
        expected = ",".join(MAIN_COLUMNS)
        assert header_line == expected


# ---------------------------------------------------------------------------
# Unit tests — edge cases
# ---------------------------------------------------------------------------


def test_read_main_results_no_file(tmp_path: Path) -> None:
    """Reading from a non-existent directory returns an empty list."""
    results = read_main_results(tmp_path / "nonexistent")
    assert results == []


def test_auto_creates_directory_and_file(tmp_path: Path) -> None:
    """append_main_result creates the output dir and CSV file if missing."""
    output_dir = tmp_path / "deep" / "nested" / "output"
    row = {col: "test" for col in MAIN_COLUMNS}
    append_main_result(output_dir, row)
    assert (output_dir / MAIN_FILENAME).exists()


def test_append_multiple_rows(tmp_path: Path) -> None:
    """Multiple appends accumulate rows correctly."""
    for i in range(3):
        row = {col: str(i) for col in MAIN_COLUMNS}
        append_main_result(tmp_path, row)
    results = read_main_results(tmp_path)
    assert len(results) == 3


def test_get_previous_result_match(tmp_path: Path) -> None:
    """get_previous_result returns the most recent matching row."""
    base = {col: "x" for col in MAIN_COLUMNS}
    row1 = {**base, "suite": "WAC", "mode": "nodes", "evaluator": "hce", "score_pct": "50.0"}
    row2 = {**base, "suite": "WAC", "mode": "nodes", "evaluator": "hce", "score_pct": "60.0"}
    row3 = {**base, "suite": "STS", "mode": "nodes", "evaluator": "hce", "score_pct": "70.0"}
    append_main_result(tmp_path, row1)
    append_main_result(tmp_path, row2)
    append_main_result(tmp_path, row3)

    prev = get_previous_result(tmp_path, "WAC", "nodes", "hce")
    assert prev is not None
    assert prev["score_pct"] == "60.0"


def test_get_previous_result_no_match(tmp_path: Path) -> None:
    """get_previous_result returns None when no rows match."""
    row = {col: "x" for col in MAIN_COLUMNS}
    row["suite"] = "WAC"
    row["mode"] = "nodes"
    row["evaluator"] = "hce"
    append_main_result(tmp_path, row)

    prev = get_previous_result(tmp_path, "STS", "depth8", "nnue")
    assert prev is None


def test_get_previous_result_empty(tmp_path: Path) -> None:
    """get_previous_result returns None when no CSV exists."""
    prev = get_previous_result(tmp_path, "WAC", "nodes", "hce")
    assert prev is None


def test_append_category_results(tmp_path: Path) -> None:
    """append_category_results writes per-category rows correctly."""
    rows = [
        {col: f"val{i}" for col in CATEGORY_COLUMNS}
        for i in range(3)
    ]
    append_category_results(tmp_path, rows)
    csv_path = tmp_path / CATEGORY_FILENAME
    assert csv_path.exists()
    with open(csv_path, encoding="utf-8") as fh:
        header = fh.readline().strip()
        lines = fh.readlines()
    assert header == ",".join(CATEGORY_COLUMNS)
    assert len(lines) == 3


def test_append_category_creates_header(tmp_path: Path) -> None:
    """Category CSV is created with correct header on first write."""
    append_category_results(tmp_path, [])
    csv_path = tmp_path / CATEGORY_FILENAME
    assert csv_path.exists()
    with open(csv_path, encoding="utf-8") as fh:
        header = fh.readline().strip()
    assert header == ",".join(CATEGORY_COLUMNS)


def test_write_failure_does_not_raise(tmp_path: Path) -> None:
    """If the output path is not writable, append logs a warning but doesn't raise."""
    # Use a file as the "directory" to trigger an OSError
    bad_dir = tmp_path / "not_a_dir"
    bad_dir.write_text("block")
    # This should not raise — it logs to stderr and continues
    row = {col: "x" for col in MAIN_COLUMNS}
    append_main_result(bad_dir / "sub", row)
