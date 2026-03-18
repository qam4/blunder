"""Profiling subcommand: perf record + flamegraph generation."""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

from bench.config import Config


def _check_tool(name: str) -> str | None:
    """Return path to tool if found, else None."""
    return shutil.which(name)


def cmd_profile(config: Config, args: argparse.Namespace) -> int:
    """Run perf record on the engine and generate a flamegraph SVG."""
    engine = str(config.paths.engine_binary)
    epd = str(config.paths.wac_epd)
    outdir = Path(getattr(args, "output", None) or config.project_root / "output" / "profile")
    freq = getattr(args, "freq", 997)
    title = getattr(args, "title", "Blunder Engine Profile")

    outdir.mkdir(parents=True, exist_ok=True)

    # --- Preflight checks ---
    if not Path(engine).exists():
        print(f"Error: engine binary not found: {engine}", file=sys.stderr)
        print("  Run: cmake --build --preset=dev", file=sys.stderr)
        return 1

    if not Path(epd).exists():
        print(f"Error: EPD file not found: {epd}", file=sys.stderr)
        return 1

    perf_bin = _check_tool("perf")
    if not perf_bin:
        print("Error: 'perf' not found. Install linux-tools for your kernel.", file=sys.stderr)
        return 1

    collapse_bin = _check_tool("inferno-collapse-perf")
    fg_bin = _check_tool("inferno-flamegraph")
    if not collapse_bin or not fg_bin:
        print(
            "Error: inferno tools not found. Install with: cargo install inferno",
            file=sys.stderr,
        )
        return 1

    perf_data = outdir / "perf.data"
    stacks_folded = outdir / "stacks.folded"
    flamegraph_svg = outdir / "flamegraph.svg"

    # --- Git info for subtitle ---
    try:
        commit = subprocess.check_output(
            ["git", "rev-parse", "--short", "HEAD"],
            cwd=str(config.project_root),
            text=True,
        ).strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        commit = "unknown"

    epd_lines = sum(1 for _ in open(epd))

    print(f"=== Profiling Blunder engine ===")
    print(f"  Engine:  {engine}")
    print(f"  EPD:     {epd} ({epd_lines} positions)")
    print(f"  Output:  {outdir}/")
    print()

    # --- Step 1: perf record ---
    print("[1/3] Recording with perf...")
    perf_cmd = [
        perf_bin, "record",
        "-g", "--call-graph", "dwarf,16384",
        "-F", str(freq),
        "-o", str(perf_data),
        "--", engine, "--test-positions", epd,
    ]
    print(f"  $ {' '.join(perf_cmd)}")
    rc = subprocess.call(perf_cmd)
    if rc != 0:
        print(f"Error: perf record exited with code {rc}", file=sys.stderr)
        return 1

    # --- Step 2: collapse stacks ---
    print()
    print("[2/3] Collapsing stacks...")
    with open(stacks_folded, "w") as out_f:
        perf_script = subprocess.Popen(
            [perf_bin, "script", "-i", str(perf_data)],
            stdout=subprocess.PIPE,
        )
        collapse = subprocess.Popen(
            [collapse_bin, "--all"],
            stdin=perf_script.stdout,
            stdout=out_f,
        )
        perf_script.stdout.close()  # type: ignore[union-attr]
        collapse.wait()
        perf_script.wait()

    if collapse.returncode != 0:
        print(f"Error: collapse exited with code {collapse.returncode}", file=sys.stderr)
        return 1

    # --- Step 3: generate flamegraph ---
    print("[3/3] Generating flamegraph...")
    subtitle = f"{commit}"
    with open(stacks_folded) as in_f, open(flamegraph_svg, "w") as out_f:
        fg_rc = subprocess.call(
            [fg_bin, "--title", title, "--subtitle", subtitle],
            stdin=in_f,
            stdout=out_f,
        )

    if fg_rc != 0:
        print(f"Error: flamegraph exited with code {fg_rc}", file=sys.stderr)
        return 1

    print()
    print("=== Done ===")
    print(f"  Flamegraph: {flamegraph_svg}")
    print(f"  Raw data:   {perf_data}")
    print(f"  Folded:     {stacks_folded}")
    print()
    print("Open the SVG in a browser for interactive exploration.")
    return 0
