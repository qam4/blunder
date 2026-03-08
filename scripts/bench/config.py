"""Configuration loader for the bench CLI.

Loads and validates bench-config.json, resolves platform-specific paths,
performs variable interpolation, and provides engine registry lookup.
"""

from __future__ import annotations

import json
import os
import sys
from dataclasses import dataclass
from pathlib import Path


@dataclass
class PlatformPaths:
    engine_binary: Path
    test_binary: Path
    fast_chess: Path
    opening_book: Path
    wac_epd: Path
    sts_epd: Path


@dataclass
class EngineEntry:
    name: str
    cmd: str  # may contain ${engine_binary} or absolute path
    protocol: str  # "uci" or "xboard"
    args: list[str]
    options: dict[str, str]


@dataclass
class Defaults:
    preset: str
    evaluator: str
    mode: str
    tc: str
    concurrency: int
    sprt_elo0: int
    sprt_elo1: int
    rounds: int
    book_depth: int
    baseline_engine: str
    candidate_engine: str
    timeout_wac: int = 600
    timeout_sts: int = 3600


@dataclass
class Config:
    paths: PlatformPaths
    engines: dict[str, EngineEntry]
    defaults: Defaults
    project_root: Path


# ---------------------------------------------------------------------------
# Internal helpers
# ---------------------------------------------------------------------------

_REQUIRED_PLATFORM_FIELDS = [
    "engine_binary",
    "test_binary",
    "fast_chess",
    "opening_book",
    "wac_epd",
    "sts_epd",
]

_REQUIRED_DEFAULTS_FIELDS = [
    "preset",
    "evaluator",
    "mode",
    "tc",
    "concurrency",
    "sprt_elo0",
    "sprt_elo1",
    "rounds",
    "book_depth",
    "baseline_engine",
    "candidate_engine",
]

_REQUIRED_ENGINE_FIELDS = ["cmd", "protocol"]


def _error(msg: str) -> None:
    """Print an error message and exit."""
    print(msg, file=sys.stderr)
    sys.exit(1)


def _check_field(data: dict, key: str, parent: str) -> None:
    """Ensure *key* exists in *data*, or exit with a descriptive error."""
    if key not in data:
        _error(f"Error: Config missing required field: {parent}.{key}")


def _detect_platform() -> str:
    """Return 'windows' if on Windows, else 'linux'."""
    return "windows" if os.name == "nt" else "linux"


def _interpolate(value: str, preset: str, engine_binary: str | None = None) -> str:
    """Replace ``${preset}`` and ``${engine_binary}`` placeholders."""
    result = value.replace("${preset}", preset)
    if engine_binary is not None:
        result = result.replace("${engine_binary}", engine_binary)
    return result


def _resolve_path(
    raw: str,
    project_root: Path,
    preset: str,
    engine_binary_str: str | None = None,
    *,
    is_binary: bool = False,
    platform: str = "linux",
) -> Path:
    """Resolve a single config path string to an absolute ``Path``.

    Steps:
    1. Variable interpolation (``${preset}``, ``${engine_binary}``).
    2. ``~`` expansion.
    3. Relative-to-project-root resolution.
    4. ``.exe`` appending on Windows for binary paths.
    """
    interpolated = _interpolate(raw, preset, engine_binary_str)
    p = Path(interpolated)

    # Expand ~
    if "~" in interpolated:
        p = p.expanduser()

    # Resolve relative paths against project root
    if not p.is_absolute():
        p = project_root / p

    # Append .exe on Windows for binary paths (if not already present)
    if is_binary and platform == "windows" and p.suffix != ".exe":
        p = p.with_suffix(".exe")

    return p


# ---------------------------------------------------------------------------
# Validation helpers
# ---------------------------------------------------------------------------

def _validate_structure(data: dict) -> None:
    """Validate the top-level JSON structure has all required sections."""
    _check_field(data, "platforms", "")
    _check_field(data, "engines", "")
    _check_field(data, "defaults", "")

    platforms = data["platforms"]
    _check_field(platforms, "linux", "platforms")
    _check_field(platforms, "windows", "platforms")

    for plat_name in ("linux", "windows"):
        plat = platforms[plat_name]
        for f in _REQUIRED_PLATFORM_FIELDS:
            _check_field(plat, f, f"platforms.{plat_name}")

    defaults = data["defaults"]
    for f in _REQUIRED_DEFAULTS_FIELDS:
        _check_field(defaults, f, "defaults")

    engines = data["engines"]
    for eng_name, eng_data in engines.items():
        for f in _REQUIRED_ENGINE_FIELDS:
            _check_field(eng_data, f, f"engines.{eng_name}")


# ---------------------------------------------------------------------------
# Public API
# ---------------------------------------------------------------------------

def load_config(
    config_path: Path,
    preset_override: str | None = None,
    *,
    _platform_override: str | None = None,
) -> Config:
    """Load, validate, and resolve the bench config file.

    Parameters
    ----------
    config_path:
        Path to the JSON config file.
    preset_override:
        If given, overrides ``defaults.preset`` for path resolution.
    _platform_override:
        **Testing only.** Force platform to ``"linux"`` or ``"windows"``
        instead of auto-detecting.
    """
    # --- 1. File existence ---
    if not config_path.exists():
        _error(
            f"Error: Config file not found: {config_path}. "
            "Expected at scripts/bench-config.json"
        )

    # --- 2. JSON parsing ---
    try:
        raw_text = config_path.read_text(encoding="utf-8")
        data = json.loads(raw_text)
    except json.JSONDecodeError as exc:
        _error(f"Error: Invalid JSON in config file: {exc}")

    # --- 3. Schema validation ---
    _validate_structure(data)

    # --- 4. Resolve project root (parent of the directory containing scripts/) ---
    project_root = config_path.resolve().parent.parent

    # --- 5. Platform detection ---
    platform = _platform_override or _detect_platform()
    plat_key = "windows" if platform == "windows" else "linux"
    plat_data = data["platforms"][plat_key]

    # --- 6. Preset ---
    preset = preset_override or data["defaults"]["preset"]

    # --- 7. Resolve platform paths ---
    # engine_binary is resolved first (no ${engine_binary} reference in itself)
    engine_binary_path = _resolve_path(
        plat_data["engine_binary"],
        project_root,
        preset,
        is_binary=True,
        platform=plat_key,
    )
    engine_binary_str = str(engine_binary_path)

    paths = PlatformPaths(
        engine_binary=engine_binary_path,
        test_binary=_resolve_path(
            plat_data["test_binary"],
            project_root,
            preset,
            engine_binary_str,
            is_binary=True,
            platform=plat_key,
        ),
        fast_chess=_resolve_path(
            plat_data["fast_chess"],
            project_root,
            preset,
            engine_binary_str,
            is_binary=True,
            platform=plat_key,
        ),
        opening_book=_resolve_path(
            plat_data["opening_book"],
            project_root,
            preset,
            engine_binary_str,
            platform=plat_key,
        ),
        wac_epd=_resolve_path(
            plat_data["wac_epd"],
            project_root,
            preset,
            engine_binary_str,
            platform=plat_key,
        ),
        sts_epd=_resolve_path(
            plat_data["sts_epd"],
            project_root,
            preset,
            engine_binary_str,
            platform=plat_key,
        ),
    )

    # --- 8. Build engine registry ---
    engines: dict[str, EngineEntry] = {}
    for eng_name, eng_data in data["engines"].items():
        cmd_raw = eng_data["cmd"]
        cmd_resolved = _interpolate(cmd_raw, preset, engine_binary_str)
        engines[eng_name] = EngineEntry(
            name=eng_name,
            cmd=cmd_resolved,
            protocol=eng_data["protocol"],
            args=eng_data.get("args", []),
            options=eng_data.get("options", {}),
        )

    # --- 9. Build defaults ---
    d = data["defaults"]
    defaults = Defaults(
        preset=preset,
        evaluator=d["evaluator"],
        mode=d["mode"],
        tc=d["tc"],
        concurrency=int(d["concurrency"]),
        sprt_elo0=int(d["sprt_elo0"]),
        sprt_elo1=int(d["sprt_elo1"]),
        rounds=int(d["rounds"]),
        book_depth=int(d["book_depth"]),
        baseline_engine=d["baseline_engine"],
        candidate_engine=d["candidate_engine"],
        timeout_wac=int(d.get("timeout_wac", 600)),
        timeout_sts=int(d.get("timeout_sts", 3600)),
    )

    return Config(
        paths=paths,
        engines=engines,
        defaults=defaults,
        project_root=project_root,
    )


def resolve_engine(config: Config, name: str) -> EngineEntry:
    """Look up an engine by name in the registry.

    If *name* is not found, exit with an error listing available names.
    """
    if name in config.engines:
        return config.engines[name]
    available = ", ".join(sorted(config.engines.keys()))
    _error(f"Error: Unknown engine '{name}'. Available: {available}")
    # _error calls sys.exit, but mypy needs a return for type-checking
    raise SystemExit(1)  # pragma: no cover
