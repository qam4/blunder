"""Output parsers for Catch2 benchmark and fast-chess gauntlet results."""

from __future__ import annotations

import re
from dataclasses import dataclass, field


@dataclass
class CategoryResult:
    """Per-category result from an STS benchmark run."""

    score: int
    max_score: int
    positions: int


@dataclass
class BenchmarkResult:
    """Parsed result from a Catch2 benchmark test binary run."""

    score_pct: float
    score: int
    max_score: int
    elo: int
    nps: int
    nodes: int
    time_secs: float
    categories: dict[str, CategoryResult] = field(default_factory=dict)


@dataclass
class GauntletResult:
    """Parsed result from a fast-chess gauntlet run."""

    wins: int
    losses: int
    draws: int
    elo_diff: float
    elo_error: float
    sprt_conclusion: str | None  # "H0", "H1", or None (inconclusive)


# ---------------------------------------------------------------------------
# Regex patterns for Catch2 output parsing
# ---------------------------------------------------------------------------

_RE_SCORE = re.compile(r"Score=(\d+\.\d+)% \((\d+)/(\d+)\)")
_RE_ELO = re.compile(r"ELO=(\d+)")
_RE_NPS = re.compile(r"NPS=(\d+)")
_RE_TOTALS = re.compile(r"Nodes=(\d+) Time=(\d+\.\d+)s")
_RE_CATEGORY = re.compile(r"\s+(.+?):\s+(\d+\.\d+)% \((\d+)/(\d+), (\d+) pos\)")

# ---------------------------------------------------------------------------
# Regex patterns for fast-chess output parsing
# ---------------------------------------------------------------------------

_RE_FC_SCORE = re.compile(
    r"Score of .+ vs .+:\s+(\d+)\s+-\s+(\d+)\s+-\s+(\d+)"
)
_RE_FC_ELO = re.compile(r"Elo difference:\s+([-+]?\d+\.?\d*)\s+\+/-\s+(\d+\.?\d*)")
_RE_FC_SPRT = re.compile(r"(H[01]) was accepted")


def parse_benchmark_output(stdout: str) -> BenchmarkResult:
    """Parse Catch2 test binary stdout for score, ELO, NPS, categories.

    Raises ``ValueError`` if the required score line is not found.
    """
    score_m = _RE_SCORE.search(stdout)
    if not score_m:
        raise ValueError("Could not parse score from benchmark output")

    score_pct = float(score_m.group(1))
    score = int(score_m.group(2))
    max_score = int(score_m.group(3))

    elo_m = _RE_ELO.search(stdout)
    elo = int(elo_m.group(1)) if elo_m else 0

    nps_m = _RE_NPS.search(stdout)
    nps = int(nps_m.group(1)) if nps_m else 0

    totals_m = _RE_TOTALS.search(stdout)
    if totals_m:
        nodes = int(totals_m.group(1))
        time_secs = float(totals_m.group(2))
    else:
        nodes = 0
        time_secs = 0.0

    categories: dict[str, CategoryResult] = {}
    for cat_m in _RE_CATEGORY.finditer(stdout):
        name = cat_m.group(1)
        cat_score = int(cat_m.group(3))
        cat_max = int(cat_m.group(4))
        cat_positions = int(cat_m.group(5))
        categories[name] = CategoryResult(
            score=cat_score, max_score=cat_max, positions=cat_positions
        )

    return BenchmarkResult(
        score_pct=score_pct,
        score=score,
        max_score=max_score,
        elo=elo,
        nps=nps,
        nodes=nodes,
        time_secs=time_secs,
        categories=categories,
    )


def parse_fastchess_output(stdout: str) -> GauntletResult:
    """Parse fast-chess stdout for W/L/D, Elo diff, SPRT result.

    Raises ``ValueError`` if the score line is not found.
    """
    score_m = _RE_FC_SCORE.search(stdout)
    if not score_m:
        raise ValueError("Could not parse score from fast-chess output")

    wins = int(score_m.group(1))
    losses = int(score_m.group(2))
    draws = int(score_m.group(3))

    elo_m = _RE_FC_ELO.search(stdout)
    if elo_m:
        elo_diff = float(elo_m.group(1))
        elo_error = float(elo_m.group(2))
    else:
        elo_diff = 0.0
        elo_error = 0.0

    sprt_m = _RE_FC_SPRT.search(stdout)
    sprt_conclusion = sprt_m.group(1) if sprt_m else None

    return GauntletResult(
        wins=wins,
        losses=losses,
        draws=draws,
        elo_diff=elo_diff,
        elo_error=elo_error,
        sprt_conclusion=sprt_conclusion,
    )


def format_benchmark_output(result: BenchmarkResult) -> str:
    """Format a BenchmarkResult as Catch2-style output text.

    This is the inverse of ``parse_benchmark_output`` and is used for
    round-trip property testing.
    """
    lines: list[str] = []
    lines.append(
        f"Score={result.score_pct:.1f}% ({result.score}/{result.max_score})"
    )
    lines.append(f"ELO={result.elo}")
    lines.append(f"NPS={result.nps}")
    lines.append(f"Nodes={result.nodes} Time={result.time_secs:.1f}s")

    for name, cat in result.categories.items():
        cat_pct = (cat.score / cat.max_score * 100) if cat.max_score else 0.0
        lines.append(
            f"  {name}: {cat_pct:.1f}% ({cat.score}/{cat.max_score}, {cat.positions} pos)"
        )

    return "\n".join(lines)

def format_fastchess_output(result: GauntletResult) -> str:
    """Format a GauntletResult as fast-chess-style output text.

    This is the inverse of ``parse_fastchess_output`` and is used for
    round-trip property testing (Property 11).
    """
    total = result.wins + result.losses + result.draws
    pct = (result.wins + result.draws * 0.5) / total if total else 0.0
    lines: list[str] = [
        f"Score of engine1 vs engine2: {result.wins} - {result.losses} - {result.draws}  [{pct:.3f}] {total}",
        f"Elo difference: {result.elo_diff} +/- {result.elo_error}",
    ]
    if result.sprt_conclusion is not None:
        lines.append(
            f"SPRT: llr 2.97 (100.0%), lbound -2.94, ubound 2.94 - {result.sprt_conclusion} was accepted"
        )
    return "\n".join(lines)

