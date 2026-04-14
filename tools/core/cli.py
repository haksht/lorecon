"""Shared CLI helpers: UTF-8 stdout/stderr + common argparse scaffolding."""
from __future__ import annotations

import argparse
import os
import subprocess
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
    p.add_argument("--api", metavar="HOST", help="Load device summaries from live sniffer /api/devices")
    p.add_argument("-o", "--output", help="Output file path")
    return p


def temp_output(suffix: str, stem: str = "lorecon") -> str:
    """Create a uniquely-named file in the OS temp dir and return its path.
    Used when the user didn't pass -o / --output."""
    fd, path = tempfile.mkstemp(suffix=suffix, prefix=f"{stem}_")
    os.close(fd)
    return path


def open_in_browser(path: str) -> None:
    """Open the given file in the default browser.
    HTML opens directly. For PNG/JPG, write a tiny HTML wrapper alongside
    and open that — file:// URLs to image files don't reliably launch a
    browser on Windows, and OS default handlers are inconsistent."""
    p = Path(path).resolve()
    suffix = p.suffix.lower()
    try:
        if suffix in ('.html', '.htm'):
            webbrowser.open(p.as_uri())
            return
        if suffix in ('.png', '.jpg', '.jpeg', '.gif', '.svg'):
            wrapper = p.with_suffix(p.suffix + '.html')
            wrapper.write_text(
                f'<!doctype html><title>{p.name}</title>'
                '<style>body{margin:0;background:#111;padding:1rem;'
                'text-align:center}'
                'img{max-width:100%;height:auto}</style>'
                f'<p style="color:#888;font:12px sans-serif">'
                f'Ctrl+scroll or Ctrl +/- to zoom</p>'
                f'<img src="{p.name}" alt="{p.name}">',
                encoding='utf-8',
            )
            webbrowser.open(wrapper.as_uri())
            return
        # Unknown type — try OS default handler.
        if sys.platform.startswith('win'):
            os.startfile(str(p))  # type: ignore[attr-defined]
        elif sys.platform == 'darwin':
            subprocess.Popen(['open', str(p)])
        else:
            subprocess.Popen(['xdg-open', str(p)])
    except Exception as e:
        print(f"(could not auto-open {p}: {e})", file=sys.stderr)
