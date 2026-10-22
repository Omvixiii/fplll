#!/usr/bin/env python3
"""Compile every installed public header as a standalone translation unit.

Each header is compiled with -fsyntax-only to catch missing includes and
missing include guards. This is intentionally stricter than including
<fplll/fplll.h> once: consumers are allowed to include individual headers.

Known non-standalone headers are listed below and are expected to become
private implementation fragments in the 6.0 cleanup.
"""

import argparse
import os
import pathlib
import platform
import shutil
import subprocess
import sys
import tempfile

# Installed in the current tree, but only valid when included from pruner.h.
KNOWN_NON_STANDALONE = {"fplll/pruner/pruner_simplex.h"}


def pkg_config_cflags():
    if shutil.which("pkg-config") is None:
        return []
    result = subprocess.run(
        ["pkg-config", "--cflags", "gmp", "mpfr"],
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode != 0:
        return []
    return result.stdout.split()


def darwin_sysroot():
    if platform.system() != "Darwin" or shutil.which("xcrun") is None:
        return []
    result = subprocess.run(
        ["xcrun", "--show-sdk-path"], capture_output=True, text=True, check=False
    )
    path = result.stdout.strip()
    return ["-isysroot", path] if result.returncode == 0 and path else []


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--include-dir", required=True, help="installation include directory")
    parser.add_argument("--compiler", default=os.environ.get("CXX", "c++"))
    parser.add_argument("--std", default="c++23")
    parser.add_argument(
        "--extra-cflag",
        action="append",
        default=[],
        help="additional compiler flag (repeatable)",
    )
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args()

    include_dir = pathlib.Path(args.include_dir).resolve()
    if not include_dir.is_dir():
        print(f"error: include directory not found: {include_dir}", file=sys.stderr)
        return 2

    headers = sorted(
        [p for p in include_dir.rglob("*.h")] + [p for p in include_dir.rglob("*.hpp")]
    )
    if not headers:
        print(f"error: no headers found under {include_dir}", file=sys.stderr)
        return 2

    base_flags = [f"-std={args.std}", "-fsyntax-only", "-I", str(include_dir)]
    base_flags += darwin_sysroot()
    base_flags += pkg_config_cflags()
    base_flags += args.extra_cflag

    failures = []
    skipped = []
    with tempfile.TemporaryDirectory(prefix="fplll-headers-") as workdir:
        for header in headers:
            relative = header.relative_to(include_dir).as_posix()
            if relative in KNOWN_NON_STANDALONE:
                skipped.append(relative)
                continue
            source = pathlib.Path(workdir) / "check.cpp"
            source.write_text(f"#include <{relative}>\n")
            command = [args.compiler, *base_flags, str(source)]
            if args.verbose:
                print("compiling:", " ".join(command))
            result = subprocess.run(command, capture_output=True, text=True, check=False)
            if result.returncode != 0:
                failures.append((relative, (result.stderr or result.stdout).strip()))

    for relative in skipped:
        print(f"skip (known non-standalone): {relative}")
    for relative, message in failures:
        print(f"FAIL: {relative}", file=sys.stderr)
        print(message, file=sys.stderr)
        print(file=sys.stderr)

    print(
        f"checked {len(headers) - len(skipped)} headers "
        f"({len(skipped)} skipped, {len(failures)} failed)"
    )
    return 0 if not failures else 1


if __name__ == "__main__":
    sys.exit(main())
