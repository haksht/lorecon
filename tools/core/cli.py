"""Shared CLI helpers: UTF-8 stdout/stderr + common argparse scaffolding."""
from __future__ import annotations

import argparse
import sys


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
