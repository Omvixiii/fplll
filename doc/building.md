# Building fplll

## Requirements and first build

The library requires CMake 3.25 or newer, Ninja, a C++23-capable compiler,
GMP 4.2 or newer, and MPFR 2.3 or newer.
QD is optional. CMake discovers the compiler automatically; use
`CMAKE_PREFIX_PATH` for dependencies installed outside standard search paths.
ccache is optional.

For installation, choose the final prefix at configure time:

```sh
cmake -S . -B build/install -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build/install --parallel
ctest --test-dir build/install --output-on-failure --no-tests=error
cmake --install build/install
```

The build directory retains its compiler, options, and prefix. Use a separate
directory when switching toolchains. In-source builds, including through a
symlink, are rejected; always pass a separate `-B` directory. CMake can create an
initial cache before that check, so do not try configuring directly in the source
directory.

## Development presets

The default contribution check configures, builds, and tests in order:

```sh
cmake --workflow --preset debug
```

Use the `release` configure, build, and test presets for optimized validation.
The Debug and distcheck workflows build the test executables before running
them: **CTest does not compile changed code.**
For the edit/build loop, build the default targets or select a particular one:

```sh
cmake --build --preset debug
cmake --build --preset debug --target fplll_cli
ctest --preset debug -R '^test_lll$'
```

The last command is useful after the relevant targets have been rebuilt. The
`fplll_cli` target builds its library and CLI, not the test executables. Add
`--verbose` to a build command to inspect compiler and linker commands.

All configure presets except `distcheck` have matching build and test presets;
the `distcheck` build preset targets the release build tree.

| Preset | Purpose |
| --- | --- |
| `debug` | Default development build and validation |
| `release` | Optimized build and validation |
| `ubsan` | Debug build with UndefinedBehaviorSanitizer |
| `warnings` | Debug build with the project's strict warnings and `-Werror` |
| `asan` | Debug build with AddressSanitizer and UndefinedBehaviorSanitizer |
| `tsan` | Debug build with ThreadSanitizer |

The sanitizer and warnings presets require a matching GCC or Clang toolchain.
Configure, build, and test each separately:

```sh
cmake --preset ubsan
cmake --build --preset ubsan
ctest --preset ubsan
```

Each preset uses `build/<preset>/` and an installation prefix of
`_install/<preset>/`. Project presets define shared build scenarios, not machine
settings. Pass occasional compiler, dependency, or launcher choices on the
command line; save repeated choices in an untracked `CMakeUserPresets.json` only
when useful. For an installed ccache, configure with
`-DCMAKE_CXX_COMPILER_LAUNCHER=ccache`. Set parallelism with `--parallel` or
`CMAKE_BUILD_PARALLEL_LEVEL`; no fixed job count is part of the shared presets.
List all available presets with `cmake --list-presets=all`.

CMake Tools can select the shared presets and build targets directly. The
checked-in `.clangd` reads `build/debug/compile_commands.json` after Debug has
been configured. It does not follow preset selection automatically; select the
appropriate compilation database in your editor when using another build.

## Output ownership

| Output | Location relative to the binary directory |
| --- | --- |
| Public configuration header | `fplll/fplll_config.h` |
| Generated enumeration translation units | `generated/enum-parallel/` |
| Executables, debug tools and tests | `bin/` |
| Shared and static libraries, including variants | `lib/` |
| Per-test working directories and generated test data | `test-work/<CONFIG>/<test-name>/` |
| CMake test metadata (`CMakeFiles/`, `CTestTestfile.cmake`) | `tests/` |
| Package metadata | `fplll.pc`, `fplllConfig*.cmake` |
| Source archive | `fplll-<version>.tar.gz` |
| API documentation | `doc/html/` |

The single-configuration Ninja presets keep a flat layout: the Debug CLI is
`build/debug/bin/fplll`. In `test-work/`, `<CONFIG>` is uppercase, for example
`DEBUG`; each configuration and test is isolated.

Targets inherit the generated header directories ahead of source directories.
Do not use relative includes into the source tree for generated headers.
New generated sources must use binary-directory paths for outputs and
byproducts. Test fixtures in `tests/lattices/` and `strategies/` are read-only
inputs; tests must write relative to their isolated CTest working directories.
Run tests through CTest to establish those directories.
The test registrations live in `tests/CMakeLists.txt`; CMake's `CMakeFiles/`
directories are generated in the binary tree and are never source files.

## Build options and targets

| Option | Default / purpose |
| --- | --- |
| `BUILD_TESTING` | `ON`; build and register the 15 numerical tests and `llldiff` |
| `FPLLL_WITH_QD` | `OFF`; enable QD arithmetic |
| `FPLLL_MAX_ENUM_DIM` | `256`; maximum enumeration dimension |
| `FPLLL_MAX_PARALLEL_ENUM_DIM` | `120`; `0` disables parallel enumeration |
| `FPLLL_EXTENUM_DIR`, `FPLLL_EXTENUM_LIBRARY`, `FPLLL_EXTENUM_FUNCTION` | External enumeration library and symbol |
| `FPLLL_WARNINGS`, `FPLLL_WARNINGS_AS_ERRORS` | `OFF`; enable `-Wall -Wextra -Wpedantic -Wshadow` (and optionally `-Werror`) |
| `FPLLL_BUILD_BENCHMARKS` | `OFF`; build the `fplll_bench` microbenchmark target |

Parallel dimensions are multiples of ten between 20 and 160 and must be smaller
than `FPLLL_MAX_ENUM_DIM`. External enumeration requires an independently
installed dependency; its availability is separate from the default build.
The ubsan build also requires the matching compiler runtime library.
The project requires C++23. Presets or embedding projects may request a newer
standard, but not an older one.

QD is explicitly disabled by default. `-DFPLLL_WITH_QD=ON` requires a usable QD
installation and fails if it cannot be found; unlike the historical Autotools
auto-detection, installing QD does not silently change the enabled arithmetic.
For a shorter local build, `-DFPLLL_MAX_PARALLEL_ENUM_DIM=20` compiles fewer
parallel-enumeration specializations. It changes the configured capability;
retain the default 120 for normal release validation.

CMake's `configure_file()` generates the public configuration header from
`cmake/fplll_config.h.in`. The shorter per-dimension enumeration sources and
pkg-config metadata use `file(CONFIGURE CONTENT ...)` directly. Bracket
arguments keep these short snippets readable without escaping quotes, and
`@ONLY` preserves pkg-config's own `${variable}` references. Both commands
update outputs only when their content changes, so a no-op reconfiguration
does not trigger recompilation.
Feature macros retain their defined/undefined semantics for existing consumers.
The CLI programs use the version in `fplll/fplll_config.h`, so a second private
version header is unnecessary.

The public umbrella header is `fplll/fplll.h`, used as
`#include <fplll/fplll.h>` in both the build and installation trees.

`cmake/configuration.cmake` owns generated headers and enumeration sources.
`cmake/packaging.cmake` owns header and target installation, pkg-config metadata,
CMake package exports, and source archives. The installed package is generated
from `cmake/fplllConfig.cmake.in` using `configure_package_config_file()`.
Build-time and installed-package dependency discovery share the find modules
under `cmake/modules/`; consumers can locate dependencies through
`CMAKE_PREFIX_PATH` or the corresponding package's `<PackageName>_ROOT` variable.
The installed configuration keeps its module search path local and propagates
`QUIET` and `REQUIRED` through `find_dependency()`.

The two multiline templates remain separate from generation logic. Local
VS Code language mappings in the ignored `.vscode/settings.json` can associate
`.h.in` and `.cpp.in` with C++, and `.cmake.in` with CMake.
For C++ templates under `cmake/`, `.clangd` suppresses diagnostics and skips
background indexing because CMake placeholders and directives are not valid C++.
Generated headers and sources retain normal clangd analysis.

Default targets include shared/static `fplll` libraries, `fplll_cli` (output
name `fplll`), `latticegen`, and tests when enabled. Optional targets are
`fplllv`, `fplllv_static`, `fpllld`, `fpllld_static`, `fplll_dbg` and
`latticegen_dbg`; `dbg` builds both debug executables. Shared/static variants
reuse OBJECT libraries. The `fpllld` and `*_dbg` variants enable fplll's `DEBUG`
macro; this is separate from the Debug build type, which controls compiler
optimization and debugging information.

When embedded with `add_subdirectory()`, fplll provides its library and tool
targets and installation rules. Tests and project lifecycle targets are only
registered in a top-level build.

## Version management

`project(fplll VERSION ...)` in `CMakeLists.txt` is the version source for CMake
package compatibility, the generated public configuration header, pkg-config,
documentation, and archive names. Building a checkout or extracted source archive
does not require Git or a version override. Shared-library ABI versions are
maintained separately from the release version.

## Installation and downstream use

```cmake
find_package(fplll 5.5 CONFIG REQUIRED)
target_link_libraries(my_program PRIVATE fplll::fplll)
# Or use fplll::fplll_static.
```

Include the public API with `#include <fplll/fplll.h>`. Point
`CMAKE_PREFIX_PATH` at the installation prefix; the exported targets carry
required dependencies and include paths. The historical `<fplll.h>` entry point
remains as a forwarding header. For non-CMake consumers, add the
installation's `lib/pkgconfig/` (or configured library directory) to
`PKG_CONFIG_PATH`, and use `pkg-config --cflags --libs fplll`. Use
`pkg-config --static --libs fplll` when linking statically. The `gmp` and
`mpfr` modules (and `qd` when enabled) must be resolvable by pkg-config; the
CMake package path does not depend on pkg-config.
The install includes public headers, both libraries, the two CLI programs and
the default strategy data. `llldiff` and debug tools are not installed.
On macOS and Unix, installed tools use a relative runtime library path when
the library directory is relative to the install prefix.

The default strategy-data path is compiled into the library. Configure the
intended final `CMAKE_INSTALL_PREFIX` before building. Installing with a
different `--prefix` is not rejected up front, but the installed tools then
fail at runtime with a clear message because the compiled strategy directory
does not exist there. Reconfigure and rebuild to change the final prefix. The
installed tree is not generally relocatable merely because its libraries use
relative runtime paths.

Package managers can stage that fixed final layout with `DESTDIR`:

```sh
DESTDIR=/absolute/staging cmake --install build/install
```

This prepends the staging root to installed paths without changing the compiled
final prefix. A staged tree is not the final runtime location.

## Clean and uninstall

```sh
cmake --build --preset debug --target clean
cmake --build build/install --target uninstall
```

`clean` is the generator's native target: it removes compiled outputs and
preserves the configuration, configure-generated sources, and installation
manifest. The next build can run without reconfiguring. It does not uninstall
files. To discard configuration state as well, remove that specific build
directory after checking it contains no needed files.

`uninstall` removes files and symlinks listed in that build directory's
`install_manifest.txt`; it neither rebuilds nor deletes an installation prefix
recursively. Missing manifests fail with a diagnostic. Retain the build
directory until uninstalling. For a staged install, supply the same `DESTDIR`:

```sh
DESTDIR=/absolute/staging cmake --build build/install --target uninstall
```

The manifest describes the most recent installation from that build directory.
Uninstall a staging tree before using the same directory for a different
installation. Package-manager installations should be removed by their package
manager.

## Source packages

```sh
cmake --preset release
cmake --build --preset release --target package_source
```

Source archives exclude Git metadata, local notes, editor/agent state, caches,
build outputs and installation prefixes. The `package_source` target creates the
archive; archive creation alone does not verify a release.
Run the complete distribution check with:

```sh
cmake --workflow --preset distcheck
```

The check creates an archive in `build/release/`, then validates that exact
archive without relying on the original checkout or build outputs. It extracts
read-only sources, builds in a separate directory, runs the numerical tests,
and checks the installed package metadata, strategy-data loading, and
manifest-based uninstall. It separately checks `DESTDIR` staging,
clean/rebuild behavior, and source-tree cleanliness.

The archive and stage logs remain in the build directory. Logs are under
`build/release/distcheck/logs/`; on failure the sandbox is retained at
`build/release/distcheck/work/` so the failing command and inputs can be
inspected. Successful checks remove the sandbox. The ordinary test suite does
not run this full release lifecycle.

## Development utilities

The `lint` target checks C++ formatting without changing source files.
The `format` target applies clang-format.
Both use `cmake/lints.cmake`, scan `.cpp`, `.h`, `.hpp`, and `.inl` files under
`fplll/`, `tests/`, `tools/`, and `bench/`, and preserve bundled `json.hpp`
and `thread_pool.hpp`.
The repository's `.clang-format` is always used, and formatting warnings or
incomplete formatting fail the command. These are formatting checks; semantic
static analysis requires a separate tool such as clang-tidy.

The script can also run before configuring the project, without compiling or
discovering library dependencies:

```sh
cmake -P cmake/lints.cmake
cmake -DFPLLL_FORMAT=ON -P cmake/lints.cmake
```

With Doxygen 1.9.8 or newer installed at configure time, build the `doc` target
to generate API documentation:

```sh
cmake --preset debug
cmake --build --preset debug --target doc
```

CMake's `doxygen_add_docs()` generates `Doxyfile.doc` in the binary directory
using the project's name and version. Project-specific settings live in
`cmake/documentation.cmake`; the root `CMakeLists.txt` loads the module and
calls `fplll_add_documentation()`. Its function scope keeps Doxygen settings
local. No source-tree Doxyfile or custom HTML templates are needed.
The inputs are library sources, the README, changelog,
and the usage, build, and contribution guides. Heading links use GitHub-style
anchors so they work in both the repository and generated pages. Undocumented
API entries remain visible; namespace macros are expanded so classes and
functions appear in their C++ scopes.

HTML output is written to `doc/html/` under the binary directory. Doxygen's
native theme supplies navigation and search. Formulas use MathJax 3.2.2 from
a CDN, so formula rendering requires network access. Graphviz is optional;
when available, dependency and hierarchy diagrams use SVG. Doxygen is optional
for normal builds, and documentation is generated only when `doc` is requested.
Reconfigure after installing Doxygen to make the target available. Review
generated pages and external links before publishing: correcting namespace
scopes can change API page URLs.

## Local code coverage

This optional recipe collects gcov-format data locally. Run the following block
from the repository root with Bash and the existing Apple
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
  -DBUILD_TESTING=ON
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
C++23 Debug, GMP/MPFR, QD disabled, and parallel enumeration dimension 20.
All 15 tests passed in two clean measurements, with identical covered-line sets.
The table below predates moving the command-line programs to `tools/`; the
coverage filter only includes `fplll/` library sources, so regenerate the
baseline after that layout change.

| Files | Executable lines | Covered | Uncovered | Coverage (rounded down) |
| --- | --- | --- | --- | --- |
| 63 | 8,090 | 5,179 | 2,911 | 64.01% |

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
