#!/usr/bin/env python3
"""
Download weight files from a GitHub Release.

Usage:
    python scripts/fetch-weights.py              # latest release
    python scripts/fetch-weights.py --tag v0.5.0 # specific release
"""

import argparse
import shutil
import subprocess
import sys
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description="Download weights from GitHub Release")
    parser.add_argument("--tag", type=str, default=None,
                        help="Release tag (default: latest)")
    parser.add_argument("--output", type=str, default="weights",
                        help="Output directory (default: weights/)")
    args = parser.parse_args()

    if not shutil.which("gh"):
        sys.exit("ERROR: GitHub CLI (gh) not found. Install from https://cli.github.com/")

    output_dir = Path(args.output)
    output_dir.mkdir(parents=True, exist_ok=True)

    cmd = ["gh", "release", "download", "--pattern", "*.bin", "-D", str(output_dir)]
    if args.tag:
        cmd.insert(3, args.tag)

    print(f"Downloading weights to {output_dir}/...")
    result = subprocess.run(cmd, text=True, capture_output=True)

    if result.returncode != 0:
        print(f"ERROR: {result.stderr.strip()}")
        sys.exit(1)

    # List downloaded files
    bins = sorted(output_dir.glob("*.bin"))
    if bins:
        print(f"Downloaded {len(bins)} file(s):")
        for f in bins:
            size_mb = f.stat().st_size / (1024 * 1024)
            print(f"  {f.name} ({size_mb:.1f} MB)")
    else:
        print("No .bin files found in the release.")


if __name__ == "__main__":
    main()
