# Building fplll

## Requirements and presets

Use CMake 3.20 or newer, Ninja, ccache, GMP and MPFR. The GCC development
preset requires a C++23 compiler. Without a preset, the project defaults to
C++11, which remains the minimum supported standard for consumers. The release
preset also uses C++11. The checked-in presets use `g++` from `PATH` on Linux
and WSL. Other toolchains can configure a separate binary directory without a
checked-in preset. For nonstandard dependency locations, set `CMAKE_PREFIX_PATH`.

```sh
cmake --list-presets=all
cmake --preset gcc-debug
cmake --build --preset gcc-debug
ctest --preset gcc-debug
cmake --install build/gcc-debug
```

All five configure presets have matching build and test presets. Each resolves
`binaryDir` to `build/<preset>` and the install prefix to `_install/<preset>`,
including inherited sanitizer and coverage variants.
Hidden common presets share the build and test defaults without adding choices
to the preset list. Build presets use eight jobs; override this with
`--parallel` when needed.
The ccache location is controlled by the local ccache configuration.
For regular development, use `gcc-debug`. It builds only parallel enumeration
dimension 20, leaving one generated dimension source instead of eleven; set
`FPLLL_MAX_PARALLEL_ENUM_DIM=120` to exercise the full range. `gcc-release`
retains the full range and checks the C++11 baseline. These two presets are the
regular build contracts. `gcc-asan`, `gcc-ubsan`, and `gcc-coverage` are periodic
diagnostic configurations. Feature switches such as QD remain CMake options,
so each option combination does not require another persistent build tree.

For an edit/build loop, select the `fplll_cli` target in CMake Tools or build
that target with the `gcc-debug` preset. This builds the library and CLI without
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
Sanitizer presets also require the matching GCC runtime libraries.
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

## Source packages and build isolation

```sh
cmake --preset gcc-debug -DFPLLL_BUILD_ISOLATION_TESTS=ON
cmake --build --preset gcc-debug --target package_source
ctest --preset gcc-debug -R '^test_build_isolation$'
```

The regression test unpacks the release source archive into a private binary
directory, checks that local notes and legacy toolchain files are absent, and
verifies rejection of direct and symlinked in-source builds. It then plants
an invalid source-tree configuration header to detect accidental inclusion,
builds the core and optional variants, runs all 15 numerical tests, installs,
and runs shared/static downstream consumers. Source file names and hashes must
remain unchanged. Its logs stay in `test-work/DEBUG/build-isolation/sandbox/`.
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
