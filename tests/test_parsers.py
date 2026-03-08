"""Unit and property-based tests for the parsers module.

# Feature: benchmarking-infra, Property 7: Benchmark output parsing round-trip
# Feature: benchmarking-infra, Property 11: Fast-chess output parsing
"""

from __future__ import annotations

import pytest
from hypothesis import given, settings, assume
from hypothesis import strategies as st

from bench.parsers import (
    BenchmarkResult,
    CategoryResult,
    GauntletResult,
    format_benchmark_output,
    parse_benchmark_output,
    parse_fastchess_output,
)

# ---------------------------------------------------------------------------
# Strategies
# ---------------------------------------------------------------------------

# Category names that won't confuse the regex (no colons, no leading/trailing
# whitespace, no newlines, and at least one character).
_cat_name_st = st.text(
    alphabet=st.characters(
        whitelist_categories=("L", "N", "Zs"),
        blacklist_characters=":\n\r",
    ),
    min_size=1,
    max_size=30,
).map(str.strip).filter(lambda s: len(s) > 0)

_category_st = st.builds(
    CategoryResult,
    score=st.integers(min_value=0, max_value=9999),
    max_score=st.integers(min_value=1, max_value=9999),
    positions=st.integers(min_value=1, max_value=9999),
)


@st.composite
def benchmark_result_strategy(draw: st.DrawFn) -> BenchmarkResult:
    """Generate a valid BenchmarkResult suitable for round-trip testing."""
    score = draw(st.integers(min_value=0, max_value=9999))
    max_score = draw(st.integers(min_value=1, max_value=9999))
    score_pct = round(score / max_score * 100, 1)

    # Generate unique category names
    n_cats = draw(st.integers(min_value=0, max_value=5))
    cat_names = draw(
        st.lists(_cat_name_st, min_size=n_cats, max_size=n_cats, unique=True)
    )
    cats: dict[str, CategoryResult] = {}
    for name in cat_names:
        cats[name] = draw(_category_st)

    return BenchmarkResult(
        score_pct=score_pct,
        score=score,
        max_score=max_score,
        elo=draw(st.integers(min_value=0, max_value=4000)),
        nps=draw(st.integers(min_value=0, max_value=100_000_000)),
        nodes=draw(st.integers(min_value=0, max_value=10**12)),
        time_secs=round(draw(st.floats(min_value=0, max_value=99999.9, allow_nan=False, allow_infinity=False)), 1),
        categories=cats,
    )


# ---------------------------------------------------------------------------
# Property 7: Benchmark output parsing round-trip
# ---------------------------------------------------------------------------
# For any valid BenchmarkResult, formatting it in the Catch2 output format
# and then parsing that string should produce an equivalent BenchmarkResult.
#
# **Validates: Requirements 3.6, 3.7, 10.1**
# ---------------------------------------------------------------------------


@settings(max_examples=100)
@given(result=benchmark_result_strategy())
def test_benchmark_round_trip(result: BenchmarkResult) -> None:
    """Property 7: format then parse produces an equivalent BenchmarkResult."""
    text = format_benchmark_output(result)
    parsed = parse_benchmark_output(text)

    assert parsed.score_pct == result.score_pct
    assert parsed.score == result.score
    assert parsed.max_score == result.max_score
    assert parsed.elo == result.elo
    assert parsed.nps == result.nps
    assert parsed.nodes == result.nodes
    assert parsed.time_secs == result.time_secs
    assert set(parsed.categories.keys()) == set(result.categories.keys())
    for name, cat in result.categories.items():
        p = parsed.categories[name]
        assert p.score == cat.score
        assert p.max_score == cat.max_score
        assert p.positions == cat.positions


# ---------------------------------------------------------------------------
# Property 11: Fast-chess output parsing
# ---------------------------------------------------------------------------
# For any valid fast-chess result output containing W/L/D, Elo difference
# with error margin, and optionally an SPRT conclusion (H0/H1), the parser
# should extract the correct numeric values and conclusion string.
#
# **Validates: Requirements 5.6, 12.3, 12.4**
# ---------------------------------------------------------------------------


@st.composite
def fastchess_output_strategy(draw: st.DrawFn) -> tuple[str, GauntletResult]:
    """Generate a valid fast-chess output string and the expected result."""
    wins = draw(st.integers(min_value=0, max_value=9999))
    losses = draw(st.integers(min_value=0, max_value=9999))
    draws = draw(st.integers(min_value=0, max_value=9999))
    total = wins + losses + draws
    assume(total > 0)

    elo_diff = round(draw(st.floats(min_value=-999.9, max_value=999.9, allow_nan=False, allow_infinity=False)), 1)
    elo_error = round(draw(st.floats(min_value=0.1, max_value=999.9, allow_nan=False, allow_infinity=False)), 1)

    sprt_choice = draw(st.sampled_from(["H0", "H1", None]))

    lines = [
        f"Score of engine1 vs engine2: {wins} - {losses} - {draws}  [0.500] {total}",
        f"Elo difference: {elo_diff} +/- {elo_error}",
    ]
    if sprt_choice is not None:
        lines.append(
            f"SPRT: llr 2.97 (100.0%), lbound -2.94, ubound 2.94 - {sprt_choice} was accepted"
        )

    expected = GauntletResult(
        wins=wins,
        losses=losses,
        draws=draws,
        elo_diff=elo_diff,
        elo_error=elo_error,
        sprt_conclusion=sprt_choice,
    )
    return "\n".join(lines), expected


@settings(max_examples=100)
@given(data=fastchess_output_strategy())
def test_fastchess_parse(data: tuple[str, GauntletResult]) -> None:
    """Property 11: fast-chess output parsing extracts correct values."""
    text, expected = data
    parsed = parse_fastchess_output(text)

    assert parsed.wins == expected.wins
    assert parsed.losses == expected.losses
    assert parsed.draws == expected.draws
    assert parsed.elo_diff == pytest.approx(expected.elo_diff, abs=0.01)
    assert parsed.elo_error == pytest.approx(expected.elo_error, abs=0.01)
    assert parsed.sprt_conclusion == expected.sprt_conclusion


# ---------------------------------------------------------------------------
# Unit tests — parse_benchmark_output
# ---------------------------------------------------------------------------


def test_parse_benchmark_basic() -> None:
    """Parse a minimal Catch2 output with score, ELO, NPS, totals."""
    stdout = (
        "Score=75.0% (225/300)\n"
        "ELO=1850\n"
        "NPS=1500000\n"
        "Nodes=450000000 Time=300.0s\n"
    )
    r = parse_benchmark_output(stdout)
    assert r.score_pct == 75.0
    assert r.score == 225
    assert r.max_score == 300
    assert r.elo == 1850
    assert r.nps == 1500000
    assert r.nodes == 450000000
    assert r.time_secs == 300.0
    assert r.categories == {}


def test_parse_benchmark_with_categories() -> None:
    """Parse Catch2 output that includes STS per-category lines."""
    stdout = (
        "Score=62.5% (937/1500)\n"
        "ELO=2100\n"
        "NPS=2000000\n"
        "Nodes=3000000000 Time=1500.0s\n"
        "  Undermining: 70.0% (70/100, 100 pos)\n"
        "  Open Files and Diagonals: 55.0% (55/100, 100 pos)\n"
    )
    r = parse_benchmark_output(stdout)
    assert len(r.categories) == 2
    assert r.categories["Undermining"].score == 70
    assert r.categories["Undermining"].max_score == 100
    assert r.categories["Undermining"].positions == 100
    assert r.categories["Open Files and Diagonals"].score == 55


def test_parse_benchmark_missing_score_raises() -> None:
    """ValueError when the score line is missing."""
    with pytest.raises(ValueError, match="Could not parse score"):
        parse_benchmark_output("no useful output here")


def test_parse_benchmark_missing_optional_fields() -> None:
    """Missing ELO/NPS/totals default to zero."""
    stdout = "Score=50.0% (150/300)\n"
    r = parse_benchmark_output(stdout)
    assert r.elo == 0
    assert r.nps == 0
    assert r.nodes == 0
    assert r.time_secs == 0.0


# ---------------------------------------------------------------------------
# Unit tests — parse_fastchess_output
# ---------------------------------------------------------------------------


def test_parse_fastchess_basic() -> None:
    """Parse a typical fast-chess output with SPRT conclusion."""
    stdout = (
        "Score of engine1 vs engine2: 150 - 120 - 230  [0.530] 500\n"
        "Elo difference: 20.9 +/- 12.3\n"
        "SPRT: llr 2.97 (100.0%), lbound -2.94, ubound 2.94 - H1 was accepted\n"
    )
    r = parse_fastchess_output(stdout)
    assert r.wins == 150
    assert r.losses == 120
    assert r.draws == 230
    assert r.elo_diff == pytest.approx(20.9)
    assert r.elo_error == pytest.approx(12.3)
    assert r.sprt_conclusion == "H1"


def test_parse_fastchess_h0() -> None:
    """Parse fast-chess output where H0 was accepted."""
    stdout = (
        "Score of engine1 vs engine2: 100 - 110 - 290  [0.490] 500\n"
        "Elo difference: -6.9 +/- 11.5\n"
        "SPRT: llr -2.94 (100.0%), lbound -2.94, ubound 2.94 - H0 was accepted\n"
    )
    r = parse_fastchess_output(stdout)
    assert r.sprt_conclusion == "H0"
    assert r.elo_diff == pytest.approx(-6.9)


def test_parse_fastchess_no_sprt() -> None:
    """Parse fast-chess output without SPRT conclusion (round-robin)."""
    stdout = (
        "Score of engine1 vs engine2: 200 - 180 - 120  [0.520] 500\n"
        "Elo difference: 13.9 +/- 18.2\n"
    )
    r = parse_fastchess_output(stdout)
    assert r.sprt_conclusion is None


def test_parse_fastchess_missing_score_raises() -> None:
    """ValueError when the score line is missing."""
    with pytest.raises(ValueError, match="Could not parse score"):
        parse_fastchess_output("nothing useful")


def test_parse_fastchess_missing_elo_line() -> None:
    """Missing Elo line defaults to 0.0."""
    stdout = "Score of engine1 vs engine2: 50 - 50 - 100  [0.500] 200\n"
    r = parse_fastchess_output(stdout)
    assert r.elo_diff == 0.0
    assert r.elo_error == 0.0
