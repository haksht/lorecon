"""Regenerate data/webapp/*.gz from their sources before uploadfs.

ESPAsyncWebServer serves foo.js.gz with Content-Encoding: gzip when a request
for foo.js arrives, saving ~4x on LittleFS and transfer. We keep both raw and
.gz in the source tree so raw stays editable — this script keeps .gz in sync
with its raw source automatically on every `pio run -t uploadfs`.

Hooked as a pre-action on the uploadfs target (not on every build).
"""
import gzip
import os
from pathlib import Path

Import("env")  # noqa: F821  (injected by PlatformIO)

WEBAPP_DIR = Path(env.subst("$PROJECT_DIR")) / "data" / "webapp"  # noqa: F821
EXTENSIONS = (".html", ".js", ".css")


def regenerate_gz(*_args, **_kwargs):
    if not WEBAPP_DIR.exists():
        print(f"[webapp-gz] skip: {WEBAPP_DIR} not found")
        return
    rebuilt = 0
    for src in WEBAPP_DIR.rglob("*"):
        if not src.is_file() or src.suffix not in EXTENSIONS:
            continue
        dst = src.with_suffix(src.suffix + ".gz")
        if dst.exists() and dst.stat().st_mtime >= src.stat().st_mtime:
            continue
        data = src.read_bytes()
        # mtime=0 keeps output deterministic (no timestamp in the gz header)
        with gzip.GzipFile(filename=str(dst), mode="wb", compresslevel=9, mtime=0) as fh:
            fh.write(data)
        rel = src.relative_to(WEBAPP_DIR)
        print(f"[webapp-gz] {rel} -> {rel}.gz  ({len(data)} -> {dst.stat().st_size} bytes)")
        rebuilt += 1
    if rebuilt == 0:
        print("[webapp-gz] all .gz files up to date")
    else:
        print(f"[webapp-gz] regenerated {rebuilt} file(s)")


env.AddPreAction("uploadfs", regenerate_gz)  # noqa: F821
env.AddPreAction("buildfs", regenerate_gz)   # noqa: F821
