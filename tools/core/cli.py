"""Shared CLI helpers: UTF-8 stdout/stderr + common argparse scaffolding."""
from __future__ import annotations

import argparse
import os
import sys
import tempfile
import webbrowser
from pathlib import Path


def setup_utf8() -> None:
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
        sys.stderr.reconfigure(encoding="utf-8", errors="replace")
    except AttributeError:
        pass


def base_parser(description: str) -> argparse.ArgumentParser:
    """Parser with input/--api/-o options common to analysis tools."""
    setup_utf8()
    p = argparse.ArgumentParser(description=description)
    p.add_argument("input", nargs="?", help="CSV or PCAP path")
    p.add_argument("--api", metavar="HOST", help="Load from live sniffer (replay slots only)")
    p.add_argument("-o", "--output", help="Output file path")
    return p


def resolve_source(args: argparse.Namespace) -> str:
    """Convert parsed args into a single source string for capture.load()."""
    if getattr(args, "api", None):
        return f"api:{args.api}"
    if not getattr(args, "input", None):
        raise SystemExit("ERROR: provide a capture file or --api HOST")
    return args.input


def temp_output(suffix: str, stem: str = "lorarecon") -> str:
    """Create a uniquely-named file in the OS temp dir and return its path.
    Used when the user didn't pass -o / --output."""
    fd, path = tempfile.mkstemp(suffix=suffix, prefix=f"{stem}_")
    os.close(fd)
    return path


def open_in_browser(path: str) -> None:
    """Open the given file in the default browser (cross-platform).
    Uses file:// URL so it works on Windows, macOS, and Linux."""
    try:
        url = Path(path).resolve().as_uri()
        webbrowser.open(url)
    except Exception as e:
        print(f"(could not auto-open browser: {e})", file=sys.stderr)
