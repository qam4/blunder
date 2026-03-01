#!/usr/bin/env python3
"""
Download a blunder release binary from GitHub for use as a cutechess baseline.

Usage:
    python scripts/fetch-baseline.py v0.3
    python scripts/fetch-baseline.py v0.3 --arch win-x64
    python scripts/fetch-baseline.py --list

Requires: gh CLI (https://cli.github.com/) authenticated with your repo.
"""

import argparse
import os
import shutil
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path

REPO = "qam4/blunder"
BASELINES_DIR = Path(__file__).resolve().parent.parent / "baselines"


def run_gh(args: list[str], capture: bool = True) -> str:
    """Run a gh CLI command and return stdout."""
    cmd = ["gh"] + args
    try:
        result = subprocess.run(
            cmd, capture_output=capture, text=True, check=True
        )
        return result.stdout.strip() if capture else ""
    except FileNotFoundError:
        sys.exit("ERROR: 'gh' CLI not found. Install from https://cli.github.com/")
    except subprocess.CalledProcessError as e:
        sys.exit(f"ERROR: gh command failed: {' '.join(cmd)}\n{e.stderr}")


def list_releases() -> None:
    """List available releases."""
    output = run_gh(["release", "list", "--repo", REPO, "--limit", "20"])
    if not output:
        print("No releases found.")
        return
    print("Available releases:")
    print(output)


def detect_arch() -> str:
    """Detect the appropriate architecture for this platform."""
    if os.name == "nt":
        return "win-x64"
    elif sys.platform == "darwin":
        return "macos-x64"
    else:
        return "linux-x64"


def exe_name(arch: str) -> str:
    """Return the engine executable name for the given architecture."""
    return "blunder.exe" if arch.startswith("win") else "blunder"


def fetch(tag: str, arch: str) -> Path:
    """Download and extract a release binary. Returns path to the executable."""
    BASELINES_DIR.mkdir(parents=True, exist_ok=True)

    suffix = ".exe" if arch.startswith("win") else ""
    dest = BASELINES_DIR / f"blunder-{tag}-{arch}{suffix}"

    if dest.exists():
        print(f"Already downloaded: {dest}")
        return dest

    asset_pattern = f"blunder-{arch}-{tag}.zip"
    print(f"Downloading {asset_pattern} from release {tag}...")

    with tempfile.TemporaryDirectory() as tmpdir:
        tmp = Path(tmpdir)
        # gh release download writes assets to the current directory
        run_gh([
            "release", "download", tag,
            "--repo", REPO,
            "--pattern", asset_pattern,
            "--dir", str(tmp),
        ])

        zip_path = tmp / asset_pattern
        if not zip_path.exists():
            # Try without tag suffix (some releases may name differently)
            alt = f"blunder-{arch}.zip"
            zip_path = tmp / alt
            if not zip_path.exists():
                sys.exit(
                    f"ERROR: Could not find asset matching '{asset_pattern}' "
                    f"or '{alt}' in release {tag}"
                )

        # Extract and find the binary
        extract_dir = tmp / "extracted"
        with zipfile.ZipFile(zip_path, "r") as zf:
            zf.extractall(extract_dir)

        # Search for the engine binary in the extracted files
        target = exe_name(arch)
        found = list(extract_dir.rglob(target))
        if not found:
            sys.exit(f"ERROR: '{target}' not found in {zip_path.name}")

        src = found[0]
        shutil.copy2(src, dest)

        # Make executable on Unix
        if not arch.startswith("win"):
            dest.chmod(dest.stat().st_mode | 0o755)

    print(f"Saved: {dest}")
    return dest


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(
        description="Download a blunder release for cutechess baseline testing"
    )
    p.add_argument("tag", nargs="?", help="Release tag (e.g. v0.3)")
    p.add_argument(
        "--arch", default=None,
        help="Architecture: win-x64, linux-x64, macos-x64 (auto-detected)"
    )
    p.add_argument("--list", action="store_true", help="List available releases")
    return p.parse_args()


def main() -> None:
    args = parse_args()

    if args.list:
        list_releases()
        return

    if not args.tag:
        sys.exit("ERROR: Please provide a release tag (e.g. v0.3) or use --list")

    arch = args.arch or detect_arch()
    path = fetch(args.tag, arch)
    print(f"\nUse as baseline:")
    print(f"  python scripts/run-cutechess.py --baseline {path}")


if __name__ == "__main__":
    main()
