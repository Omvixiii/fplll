# Building fplll

## Requirements and presets

Use CMake 3.20 or newer, Ninja, ccache, a C++23 compiler, GMP and MPFR.
The presets use `g++` or `clang++` from `PATH`. On macOS, install the libraries
and tools with Homebrew and use a Clang preset. Linux and WSL use the same
CMake commands. For nonstandard dependency locations, set `CMAKE_PREFIX_PATH`.

```sh
cmake --list-presets=all
cmake --preset gcc-debug
cmake --build --preset gcc-debug
ctest --preset gcc-debug
cmake --install build/gcc-debug
```

All ten configure presets have matching build and test presets. Each resolves
`binaryDir` to `build/<preset>` and the install prefix to `_install/<preset>`,
including inherited QD, enumeration, sanitizer and coverage variants.
Build presets use eight jobs; override this with `--parallel` when needed.
The ccache location is controlled by the local ccache configuration.

Consumers and package managers may configure a different binary directory
using `cmake -S <source> -B <binary>`. A binary directory equal to the source
directory, including through a symlink, is rejected before compiler detection.
CMake itself may create an initial cache before this check runs, so use presets
instead of attempting an in-source configure in the checkout.

## Output ownership

| Output | Location relative to the binary directory |
| --- | --- |
| Public configuration header | `fplll/fplll_config.h` |
| Private configuration header | `config.h` |
| Public umbrella header | `fplll.h` |
| Generated enumeration translation units | `generated/enum-parallel/` |
| Executables, debug tools and tests | `bin/<CONFIG>/` |
| Shared and static libraries, including variants | `lib/<CONFIG>/` |
| Per-test working directories and generated test data | `test-work/<CONFIG>/<test-name>/` |
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

Parallel dimensions are multiples of ten between 20 and 160 and must be smaller
than `FPLLL_MAX_ENUM_DIM`. MPIR and external enumeration require independently
installed dependencies; their availability is separate from the default build.

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
invalid source-tree configuration headers to detect accidental inclusion,
builds the core and optional variants, runs all 15 numerical tests, installs,
and runs shared/static downstream consumers. Source file names and hashes must
remain unchanged. Its logs stay in `test-work/DEBUG/build-isolation/sandbox/`.

Source archives exclude Git metadata, local notes, editor/agent state, caches,
build outputs and installation prefixes. The `package_source` target replaces
the old source distribution workflow; the isolation test verifies the archive.

## Development utilities

The `check-style` target checks C++ formatting without changing source files.
The `format` target applies clang-format. With Doxygen installed at configure
time, the `doc` target generates API documentation under the binary directory.

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
