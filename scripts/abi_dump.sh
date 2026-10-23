#!/usr/bin/env bash
# Dump the exported symbol table of a shared library in a stable, sorted form.
#
# Usage: scripts/abi_dump.sh <library> [output-file]
# With no output file the symbols are written to stdout.

set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "usage: $0 <library> [output-file]" >&2
  exit 2
fi

library="$1"
output="${2:-}"

if [[ ! -f "$library" ]]; then
  echo "error: library not found: $library" >&2
  exit 2
fi

case "$(uname -s)" in
  Darwin)
    symbols="$(nm -gU "$library" | awk '{print $3}' | sed 's/^_//')"
    ;;
  Linux)
    symbols="$(nm -D --defined-only "$library" | awk '{print $3}')"
    ;;
  *)
    echo "error: unsupported platform $(uname -s)" >&2
    exit 2
    ;;
esac

if [[ -n "$output" ]]; then
  printf '%s\n' "$symbols" | sort -u > "$output"
  echo "wrote $(wc -l < "$output" | tr -d ' ') symbols to $output"
else
  printf '%s\n' "$symbols" | sort -u
fi
