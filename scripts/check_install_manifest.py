#!/usr/bin/env python3
"""Validate the installed fplll tree without relying on an exact file list.

The required files are part of the public packaging contract; the forbidden
patterns are private implementation details that must never leak into the
installation.
"""

import argparse
import pathlib
import sys

REQUIRED = [
    "bin/fplll",
    "bin/latticegen",
    "include/fplll.h",
    "include/fplll/fplll.h",
    "include/fplll/fplll_config.h",
    "include/fplll/defs.h",
    "include/fplll/nr/nr.h",
    "include/fplll/nr/matrix.h",
    "include/fplll/enum/enumerate.h",
    "include/fplll/pruner/pruner.h",
    "include/fplll/io/thread_pool.hpp",
    "share/fplll/strategies/default.json",
]

FORBIDDEN_SUBSTRINGS = [
    "include/fplll/enum-parallel/",
    "include/fplll/enum/topenum.h",
    "include/fplll/main.h",
]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prefix", required=True)
    parser.add_argument(
        "--forbid-installed-cpp",
        action="store_true",
        help="also reject implementation .cpp files under include/ (6.0 layout)",
    )
    parser.add_argument(
        "--forbid-private-fragments",
        action="store_true",
        help="also reject pruner_simplex.h (moved to .inl in the 6.0 layout)",
    )
    args = parser.parse_args()

    prefix = pathlib.Path(args.prefix).resolve()
    if not prefix.is_dir():
        print(f"error: install prefix not found: {prefix}", file=sys.stderr)
        return 2

    installed = sorted(p.relative_to(prefix).as_posix() for p in prefix.rglob("*") if p.is_file())
    installed_set = set(installed)

    errors = []
    for required in REQUIRED:
        if required not in installed_set:
            errors.append(f"missing required file: {required}")

    for entry in installed:
        for forbidden in FORBIDDEN_SUBSTRINGS:
            if entry.startswith(forbidden) or entry == forbidden:
                errors.append(f"installed private path: {entry}")

    if args.forbid_installed_cpp:
        for entry in installed:
            if entry.startswith("include/") and entry.endswith(".cpp"):
                errors.append(f"installed implementation unit: {entry}")

    if args.forbid_private_fragments:
        for entry in installed:
            if entry == "include/fplll/pruner/pruner_simplex.h":
                errors.append(f"installed private fragment: {entry}")

    headers = [entry for entry in installed if entry.startswith("include/")]
    libraries = [entry for entry in installed if pathlib.PurePosixPath(entry).name.startswith("libfplll")]
    if not libraries:
        errors.append("no libfplll library found in the installation")
    # pkg-config is the downstream consumption interface in the 6.0 layout.
    if not any(entry.endswith("pkgconfig/fplll.pc") for entry in installed):
        errors.append("missing pkg-config metadata: fplll.pc")

    for error in errors:
        print(f"FAIL: {error}", file=sys.stderr)
    print(
        f"checked {len(installed)} installed files "
        f"({len(headers)} headers, {len(libraries)} libraries, {len(errors)} errors)"
    )
    return 0 if not errors else 1


if __name__ == "__main__":
    sys.exit(main())
