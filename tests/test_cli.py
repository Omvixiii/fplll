#!/usr/bin/env python3
"""CLI regression tests: invalid inputs must fail cleanly, never crash."""

import json
import os
import pathlib
import subprocess
import sys
import tempfile

FPLLL = pathlib.Path(sys.argv[1])
LATTICEGEN = pathlib.Path(sys.argv[2])

MATRIX_6x7 = (
    "[[1 0 0 0 0 0 0][0 1 0 0 0 0 0][0 0 1 0 0 0 0]"
    "[0 0 0 1 0 0 0][0 0 0 0 1 0 0][0 0 0 0 0 1 0]]\n"
)

failures = []


def run(name, args, stdin="", expect_success=False, timeout=60):
    result = subprocess.run(
        [str(a) for a in args],
        input=stdin.encode(),
        capture_output=True,
        timeout=timeout,
    )
    # A negative return code means the process was killed by a signal.
    if result.returncode < 0:
        failures.append(f"{name}: killed by signal {-result.returncode}")
        return result
    if expect_success and result.returncode != 0:
        failures.append(f"{name}: expected success, got {result.returncode}")
    if not expect_success and result.returncode == 0:
        failures.append(f"{name}: expected failure, got success")
    return result


def write_json(name, payload):
    handle, path = tempfile.mkstemp(suffix=".json", prefix=name + "-", dir=os.getcwd())
    with os.fdopen(handle, "w") as out:
        json.dump(payload, out)
    return path


def main():
    run("latticegen --help", [LATTICEGEN, "--help"], expect_success=True)
    run("latticegen r valid", [LATTICEGEN, "r", "10", "20"], expect_success=True)

    # Invalid generator parameters used to crash (negative bit lengths) or
    # silently produce meaningless matrices.
    for name, args in [
        ("negative bits r", ["r", "10", "-5"]),
        ("negative bits u", ["u", "10", "-5"]),
        ("negative bits s", ["s", "10", "-5", "3"]),
        ("negative alpha t", ["t", "10", "-5"]),
        ("zero dimension", ["r", "0", "10"]),
        ("huge dimension", ["r", "2147483647", "5"]),
        ("qary k out of range", ["q", "5", "10", "b", "3"]),
    ]:
        run("latticegen " + name, [LATTICEGEN, *args])

    run("fplll lll", [FPLLL, "-a", "lll"], MATRIX_6x7, expect_success=True)
    run("fplll hlll one row", [FPLLL, "-a", "hlll"], "[[1 2]]\n", expect_success=True)
    run("fplll bkz zero matrix", [FPLLL, "-a", "bkz", "-b", "2"], "[[0 0][0 0]]\n",
        expect_success=True)

    # Invalid scalar options.
    for name, args in [
        ("delta out of range", ["-d", "2"]),
        ("delta nan", ["-d", "nan"]),
        ("eta out of range", ["-e", "2"]),
        ("negative precision", ["-p", "-5"]),
        ("negative hlll constant", ["-c", "-1"]),
        ("block size too small", ["-a", "bkz", "-b", "1"]),
    ]:
        run("fplll " + name, [FPLLL, *args], MATRIX_6x7)

    # Unreadable input file.
    run("fplll missing input file", [FPLLL, "/no/such/input/file"])

    # Malformed strategy files.
    strategy = {"block_size": 1, "pruning_parameters": [[1.0, [1.0], 1.0]]}
    short = write_json("short", [strategy])
    missing = write_json("missing", [{"pruning_parameters": [[1.0, [1.0], 1.0]]}])
    not_array = write_json("not_array", {"block_size": 1})
    bad_json = write_json("bad", [strategy])
    with open(bad_json, "w") as out:
        out.write("{not json")

    run("fplll short strategy table",
        [FPLLL, "-a", "bkz", "-b", "5", "-s", short], MATRIX_6x7)
    run("fplll missing block_size",
        [FPLLL, "-a", "bkz", "-b", "2", "-s", missing], MATRIX_6x7)
    run("fplll strategy root not array",
        [FPLLL, "-a", "bkz", "-b", "2", "-s", not_array], MATRIX_6x7)
    run("fplll malformed strategy json",
        [FPLLL, "-a", "bkz", "-b", "2", "-s", bad_json], MATRIX_6x7)

    # Unwritable GSO dump destination.
    run("fplll bad gso dump path",
        [FPLLL, "-a", "bkz", "-b", "2", "-bkzdumpgso", "/no/such/dir/gso.json"],
        MATRIX_6x7)

    for path in (short, missing, not_array, bad_json):
        os.unlink(path)

    if failures:
        for failure in failures:
            print("FAIL:", failure, file=sys.stderr)
        return 1
    print("CLI regression tests passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
