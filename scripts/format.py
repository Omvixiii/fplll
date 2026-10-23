#!/usr/bin/env python3
"""Apply or check clang-format over the project's C++ sources.

Used by the Meson "format" and "lint" run targets. Bundled third-party
headers keep their upstream formatting and are skipped.
"""

import pathlib
import subprocess
import sys

DIRECTORIES = ("fplll", "tests", "tools", "bench")
EXTENSIONS = (".cpp", ".h", ".hpp", ".inl")
VENDORED = {"fplll/io/json.hpp", "fplll/io/thread_pool.hpp"}


def sources(root):
    found = []
    for directory in DIRECTORIES:
        for extension in EXTENSIONS:
            found.extend((root / directory).rglob(f"*{extension}"))
    return sorted(
        path for path in found if path.is_file() and path.relative_to(root).as_posix() not in VENDORED
    )


def main(argv):
    if len(argv) != 3 or argv[2] not in ("--check", "--format"):
        print(f"usage: {argv[0]} CLANG_FORMAT --check|--format", file=sys.stderr)
        return 2
    clang_format, mode = argv[1], argv[2]
    root = pathlib.Path(__file__).resolve().parent.parent
    files = sources(root)
    if not files:
        print("error: no C++ sources found", file=sys.stderr)
        return 1
    style = f"--style=file:{root / '.clang-format'}"
    if mode == "--check":
        command = [clang_format, style, "--Werror", "--fail-on-incomplete-format", "--dry-run"]
        print(f"Checking {len(files)} C++ files")
    else:
        command = [clang_format, style, "-i"]
        print(f"Formatting {len(files)} C++ files")
    return subprocess.run(command + [str(path) for path in files]).returncode


if __name__ == "__main__":
    sys.exit(main(sys.argv))
