# Building fplll

## Requirements and presets

Use CMake 3.21 or newer for the checked-in presets, Ninja, ccache, GMP and MPFR.
The project defaults to C++11, which remains the minimum supported standard for
consumers. CMake discovers the compiler automatically. For nonstandard dependency
locations, set `CMAKE_PREFIX_PATH`. Direct configuration without presets supports
CMake 3.20 or newer.

```sh
cmake --list-presets=all
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
cmake --install build/debug
```

The `debug`, `release`, and `relwithdebinfo` configure presets have matching build
and test presets. Each resolves `binaryDir` to `build/<preset>` and the install
prefix to `_install/<preset>`. Put compiler selection, dependency paths, and build
parallelism in an untracked `CMakeUserPresets.json`, or pass them on the command
line. The ccache location is controlled by the local ccache configuration.
For a shorter development build, set `FPLLL_MAX_PARALLEL_ENUM_DIM=20`; the default
is 120. CI supplies its compiler, C++23 checks, sanitizer flags, and coverage flags
explicitly, keeping diagnostic settings out of the portable project presets.

For an edit/build loop, select the `fplll_cli` target in CMake Tools or build
that target with the `debug` preset. This builds the library and CLI without
compiling every test executable. Build the default `all` target before running
the full CTest suite. Changing the compiler, C++ standard, or configured
features still requires recompiling the affected objects; a no-op reconfigure
does not.

Consumers and package managers may configure a different binary directory
using `cmake -S <source> -B <binary>`. A binary directory equal to the source
directory, including through a symlink, is rejected before compiler detection.
CMake itself may create an initial cache before this check runs, so use presets
instead of attempting an in-source configure in the checkout.

## Output ownership

| Output | Location relative to the binary directory |
| --- | --- |
| Public configuration header | `fplll/fplll_config.h` |
| Public umbrella header | `fplll.h` |
| Generated enumeration translation units | `generated/enum-parallel/` |
| Executables, debug tools and tests | `bin/<CONFIG>/` |
| Shared and static libraries, including variants | `lib/<CONFIG>/` |
| Per-test working directories and generated test data | `test-work/<CONFIG>/<test-name>/` |
| CMake test metadata (`CMakeFiles/`, `CTestTestfile.cmake`) | `tests/` |
| Package metadata | `fplll.pc`, `fplllConfig*.cmake` |
| Source archive | `fplll-<version>.tar.gz` |
| API documentation | `doc/html/` |

`<CONFIG>` is uppercase, for example `DEBUG` or `RELEASE`. The same rule applies
to custom configurations and multi-config generators. A build without a
configuration uses `bin/`, `lib/` and `test-work/` directly.

Targets inherit the generated header directories ahead of source directories.
Do not use relative includes into the source tree for generated headers.
New generated sources must use binary-directory paths for outputs and
byproducts. Test fixtures in `tests/lattices/` and `strategies/` are read-only
inputs; tests must write relative to their isolated CTest working directories.
Run tests through CTest to establish those directories.
The test registrations live in `tests/CMakeLists.txt`; CMake's `CMakeFiles/`
directories are generated in the binary tree and are never source files.
The source-package and installed-consumer regression fixtures live in
`tests/cmake/`.

## Build options and targets

| Option | Default / purpose |
| --- | --- |
| `BUILD_TESTING` | `ON`; build and register the 15 numerical tests and `llldiff` |
| `FPLLL_VERSION` | Empty; automatically determine the version, or supply `X.Y.Z` for a release |
| `FPLLL_WITH_QD` | `OFF`; enable QD arithmetic |
| `FPLLL_USE_MPIR`, `FPLLL_MPIR_ROOT` | Select MPIR instead of GMP |
| `FPLLL_WITH_RECURSIVE_ENUM` | `ON`; select recursive enumeration |
| `FPLLL_MAX_ENUM_DIM` | `256`; maximum enumeration dimension |
| `FPLLL_MAX_PARALLEL_ENUM_DIM` | `120`; `0` disables parallel enumeration |
| `FPLLL_EXTENUM_DIR`, `FPLLL_EXTENUM_LIBRARY`, `FPLLL_EXTENUM_FUNCTION` | External enumeration library and symbol |
| `FPLLL_BUILD_ISOLATION_TESTS` | `OFF`; enable the slower source-package regression test |
| `FPLLL_ISOLATION_WITH_QD` | `OFF`; enable QD in that regression test's standalone build |

Parallel dimensions are multiples of ten between 20 and 160 and must be smaller
than `FPLLL_MAX_ENUM_DIM`. MPIR and external enumeration require independently
installed dependencies; their availability is separate from the default build.
Sanitizer builds also require the matching compiler runtime libraries.
The `CMAKE_CXX_STANDARD` value in a preset or embedding project takes precedence
over the C++11 default. The source-package isolation test builds without a
standard override and verifies the C++11 baseline.

The remaining `.in` files contain configure-time substitutions: feature and
version macros, per-dimension enumeration sources, package metadata, or Doxygen
paths. The CLI programs use the version in `fplll/fplll_config.h`, so a second
private version header is unnecessary. CMake's `configure_file()` updates
generated outputs only when their content changes, keeping no-op
reconfiguration from recompiling the project.
The static umbrella header is stored as `cmake/fplll.h` and copied into the
binary directory without template processing.

Default targets include shared/static `fplll` libraries, `fplll_cli` (output
name `fplll`), `latticegen`, and tests when enabled. Optional targets are
`fplllv`, `fplllv_static`, `fpllld`, `fpllld_static`, `fplll_dbg` and
`latticegen_dbg`; `dbg` builds both debug executables. Shared/static variants
reuse OBJECT libraries.

## Version management

The checkout has no manually maintained version file. CMake resolves the version
in this order: an explicit `-DFPLLL_VERSION=X.Y.Z`, generated source-archive
metadata, then the nearest reachable Git tag named `X.Y.Z` or `vX.Y.Z`. An empty
`FPLLL_VERSION` cache entry keeps Git detection automatic on each configure.
To return a release build directory to automatic detection, configure with
`-DFPLLL_VERSION=`.

`project(fplll VERSION ...)` supplies the numeric version for CMake package
compatibility, pkg-config, documentation, and archive names. CLI version strings
also identify commits after the tag and uncommitted tracked changes, for example
`5.5.0-58-g123abcd-dirty`. With no reachable release tag, local builds use `0.0.0`
and identify the Git commit when available; a tree without Git or archive metadata
uses `0.0.0-dev`. Such builds do not claim to be a release.

Release CI takes the numeric version from the triggering tag and passes it to
CMake. CPack writes that resolved version into `cmake/FplllArchiveVersion.cmake`
inside the archive staging directory. Extracted archives therefore retain the
same version without Git, even after their source directory is renamed, and the
checkout remains untouched. Shared-library ABI versions remain independent of
the release version.

## Installation and downstream use

```cmake
find_package(fplll 5.5 CONFIG REQUIRED)
target_link_libraries(my_program PRIVATE fplll::fplll)
# Or use fplll::fplll_static.
```

Point `CMAKE_PREFIX_PATH` at the installation prefix. For non-CMake consumers,
add its `lib/pkgconfig/` to `PKG_CONFIG_PATH` and use `pkg-config fplll`.
The install includes public headers, both libraries, the two CLI programs and
the default strategy data. `llldiff` and debug tools are not installed.
On macOS and Unix, installed tools use a relative runtime library path when
the library directory is relative to the install prefix.

## Source packages and build isolation

```sh
cmake --preset debug -DFPLLL_BUILD_ISOLATION_TESTS=ON
cmake --build --preset debug --target package_source
ctest --preset debug -R '^test_build_isolation$'
```

The regression test unpacks the release source archive into a private binary
directory, checks that local notes and legacy toolchain files are absent, and
verifies rejection of direct and symlinked in-source builds. It then plants
an invalid source-tree configuration header to detect accidental inclusion,
builds the core and optional variants, runs all 15 numerical tests, installs,
starts both installed tools, and runs shared/static downstream consumers. Source
file names and hashes must remain unchanged. Its logs stay in
`test-work/DEBUG/build-isolation/sandbox/`.
CI enables `FPLLL_ISOLATION_WITH_QD` to exercise the optional QD arithmetic in
the C++11 build and installed consumers; local runs leave it off unless QD is
available. The standalone build reuses the parent compiler launcher, including
ccache when configured.

Source archives exclude Git metadata, local notes, editor/agent state, caches,
build outputs and installation prefixes. The `package_source` target replaces
the old source distribution workflow; the isolation test verifies the archive.

## Development utilities

The `check-style` target checks C++ formatting without changing source files.
The `format` target applies clang-format. With Doxygen installed at configure
time, the `doc` target generates API documentation under the binary directory.
The root [`Doxyfile`](../Doxyfile) supplies the API layout, source exclusions,
and HTML template settings; CMake includes it through
[`cmake/Doxyfile.in`](../cmake/Doxyfile.in) to set the current version and
source/build paths. Keep both files while this documentation target exists:
deleting the base configuration would also discard the project's custom
documentation settings. Doxygen is optional for normal builds.

## Local code coverage

Keep Codecov as the report service and collect gcov-format data locally. Run
the following block from the repository root with Bash and the existing Apple
Clang toolchain. With GCC, change `clang++` to the chosen GCC C++ compiler and
`coverage_gcov=(llvm-cov gcov)` to its matching `coverage_gcov=(gcov)` command
(use matching version suffixes when applicable).

The dedicated `build/coverage` directory uses Debug, the default library
features, parallel enumeration dimension 20, and all 15 numerical tests. Start
with a fresh coverage directory when changing compiler or enabled source sets.
Each measurement clears only its previous runtime counters and generated
coverage reports; normal development builds are separate.

```bash
bash <<'SH'
set -euo pipefail
coverage_source=$PWD
coverage_build="$coverage_source/build/coverage"
coverage_reports="$coverage_build/gcov"
coverage_gcov=(llvm-cov gcov)
export CCACHE_DIR="$coverage_build/ccache"

cmake --preset debug -B "$coverage_build" \
  -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_CXX_FLAGS=--coverage \
  -DCMAKE_EXE_LINKER_FLAGS=--coverage -DCMAKE_SHARED_LINKER_FLAGS=--coverage \
  -DFPLLL_MAX_PARALLEL_ENUM_DIM=20 \
  -DBUILD_TESTING=ON -DFPLLL_BUILD_ISOLATION_TESTS=OFF
cmake --build "$coverage_build" --parallel 4
find "$coverage_build" -type f \( -name '*.gcda' -o -name '*.gcov' \) -delete
rm -f "$coverage_build/coverage-lines.tsv" "$coverage_build/coverage-summary.txt"
ctest --test-dir "$coverage_build" --output-on-failure --no-tests=error

mkdir -p "$coverage_reports"
find "$coverage_build" -type f -name '*.gcno' | LC_ALL=C sort \
  > "$coverage_build/coverage-notes.txt"
test -s "$coverage_build/coverage-notes.txt"
while IFS= read -r coverage_note; do
  coverage_output="$coverage_reports/${coverage_note#"$coverage_build/"}"
  mkdir -p "$coverage_output"
  (cd "$coverage_output"
   "${coverage_gcov[@]}" -p -r -s "$coverage_source" "$coverage_note" > gcov.log 2>&1)
done < "$coverage_build/coverage-notes.txt"

find "$coverage_reports" -type f -name '*.gcov' -exec cat {} + |
  awk -F: -v root="$coverage_source/" '
    $2 == 0 && $3 == "Source" {
      source = substr($0, index($0, "Source:") + 7)
      if (index(source, root) == 1) source = substr(source, length(root) + 1)
    }
    source ~ /^fplll\// && source != "fplll/io/json.hpp" && $2 > 0 {
      count = $1
      gsub(/[[:space:]*]/, "", count)
      if (count != "-") {
        key = source "\t" ($2 + 0)
        covered[key] = covered[key] || (count ~ /^[0-9]+$/ && count + 0 > 0)
      }
    }
    END { for (key in covered) print key "\t" (covered[key] ? 1 : 0) }
  ' | LC_ALL=C sort > "$coverage_build/coverage-lines.tsv"

awk -F '\t' '
  { files[$1] = 1; lines++; hits += $3 }
  END {
    if (!lines) exit 1
    for (file in files) total_files++
    printf "Files: %d\nLines: %d\nHits: %d\nMisses: %d\nCoverage: %.2f%%\n", \
      total_files, lines, hits, lines - hits, int(10000 * hits / lines) / 100
  }
' "$coverage_build/coverage-lines.tsv" | tee "$coverage_build/coverage-summary.txt"
SH
```

Collect every `.gcno`, including objects in enumeration, pruning, and test
subdirectories. An object without runtime counts produces zero-covered lines.
Separate output directories preserve all copies of headers instantiated by
different objects. The summary merges each source line across those copies:
a line is covered if any object executed it. It includes the project's `fplll/`
sources and excludes tests, external headers, and the vendored JSON header.
Codecov's root `codecov.yml` retains the project and patch coverage policies
and excludes tests and the JSON header during service-side processing.

`build/coverage/gcov/` contains the raw `.gcov` reports and per-object logs.
`coverage-lines.tsv` contains source path, line number, and covered status
(`1` or `0`); `coverage-summary.txt` records the file count and line totals.
All of these files stay under `build/coverage`. The collection block performs
no upload. Use the corrected report as the baseline for subsequent comparisons
with the same compiler, features, and test selection.

Verified local baseline on 2026-10-22: Apple Clang 21.0.0 on arm64 macOS,
C++11 Debug, GMP/MPFR, QD disabled, and parallel enumeration dimension 20.
All 15 tests passed in two clean measurements, with identical covered-line sets.

| Files | Executable lines | Covered | Uncovered | Coverage (rounded down) |
| --- | --- | --- | --- | --- |
| 63 | 8,090 | 5,179 | 2,911 | 64.01% |

## Continuous integration

Pull requests run three checks on Ubuntu: C++ formatting and workflow syntax,
the GCC C++23 build
and numerical tests, and a C++11 source-package build with QD, numerical tests,
installation, and downstream consumers. The formatting check installs only
formatting tools. The two build checks use separate compiler-cache scopes.
Upstream `master` pushes also update coverage. The weekly and manually triggered
CMake workflow runs focused address- and undefined-behavior sanitizer tests.
Release tags validate the C++11 release preset before publishing a source
archive; manually triggered source-package runs also upload the archive as an
artifact. Docker image publishing is a separate upstream-only workflow.

## Historical baseline and cleanup

The original toolchain is retained in Git history. Keep its reference checkout
outside this source tree and outside all build/installation directories:

```sh
git worktree add --detach ../fplll-autotools-reference 360cfc6b
git worktree list
```

If that worktree already exists, reuse it. Its historical build instructions
belong to that checkout. Moving a configured reference checkout can invalidate
its cached absolute paths; regenerate its configuration there before rebuilding.
Do not share generated headers or caches with the CMake checkout.

Local migration notes are ignored under `notes/`. To inspect disposable outputs
before deleting them:

```sh
git status --short
git clean -ndX -- build/ _install/ .cache/
```

Only after reviewing that list, use `git clean -fdX` with the same path list.
Do not run an unrestricted ignored-file cleanup: local notes are ignored too.
