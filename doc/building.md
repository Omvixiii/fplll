# Building fplll

fplll is built with [Meson](https://mesonbuild.com) and Ninja. Run commands
from the repository root.

## Requirements

- Meson 1.8 or newer and Ninja.
- A C++23 compiler. The validated floor is GCC 14, Clang 18 (libstdc++ 14 or
  libc++ 18) and AppleClang 16; C++23 is mandatory and cannot be downgraded.
- GMP 4.2 or newer and MPFR 2.3 or newer, found through pkg-config, plus
  pkg-config itself.
- Optional: QD (quad-double arithmetic), Doxygen (API documentation), and
  gcovr or lcov/genhtml (code coverage).

Install Meson with `uv tool install meson` (`uvx meson` runs it ephemerally)
or `pipx install meson`.

## First build

```sh
meson setup build
meson compile -C build
meson test -C build --print-errorlogs
```

The default configuration builds shared and static libraries, enables the
tests, and uses Meson's default build type. For an optimized installation,
choose the final prefix at configure time:

```sh
meson setup build-release --buildtype=release --prefix "$HOME/.local"
meson compile -C build-release
meson test -C build-release --print-errorlogs
meson install -C build-release
```

With the default `build` directory, the command-line tools are
`build/tools/fplll` and `build/tools/latticegen`.
A build directory keeps its compiler, options, and prefix; use one directory
per configuration.

## Build options

| Option | Default | Purpose |
| --- | --- | --- |
| `-Dqd=enabled/disabled/auto` | `disabled` | QD arithmetic; `enabled` fails if QD is missing |
| `-Dmax_enum_dim=N` | `256` | maximum enumeration dimension (positive integer) |
| `-Dmax_parallel_enum_dim=N` | `120` | maximum parallel enumeration dimension; `0` disables it, valid values are `20` to `160` in steps of ten, and it must stay below `max_enum_dim` |
| `-Dextenum_dir=DIR` | empty | directory hint for an external enumeration library |
| `-Dextenum_library=LIB` | empty | external enumeration library name or path |
| `-Dextenum_function=SYM` | empty | exported enumeration symbol; defaults to the library basename |
| `-Dtests=true/false` | `true` | build and register the tests (top-level builds only) |
| `-Dbenchmarks=true/false` | `false` | build the `fplll_bench` microbenchmark |

Built-in Meson options replace project-specific switches: `--buildtype`,
`--default-library` (`both` by default; `shared` and `static` are also
accepted), `--prefix`, `-Dwarning_level=3 -Dwerror=true` (or
`--warnlevel=3 --werror`), `-Db_sanitize`, `-Db_coverage`, `-Dc_args`,
`-Dcpp_args`, and `-Dcpp_std`. C++23 is mandatory; `-Dcpp_std` accepts a newer
standard but rejects older ones.

Strict warnings are available with `-Dwarning_level=3 -Dwerror=true`. The
sources still contain pre-existing diagnostics, so CI runs this configuration
as an advisory job; it is expected to fail until the warning baseline is
cleaned up.

```sh
meson setup build --default-library=static   # or shared; both is the default
```

Change an option after configuring with `meson configure -Doption=value build`,
or recreate the build directory with `meson setup --wipe build`. Use a new
directory when switching compilers.

## Build output layout

| Output | Location relative to the build directory |
| --- | --- |
| Libraries | `fplll/libfplll.{dylib,so*,a}` |
| Tools | `tools/fplll`, `tools/latticegen`, plus `tools/fplll_dbg` and `tools/latticegen_dbg` |
| Tests | `tests/test_*`, `tests/llldiff` |
| Benchmark | `bench/fplll_bench` |
| Generated files | `fplll/fplll_config.h`, `fplll/enumlib_dim.<dim>.cpp` |
| API documentation | `doc/html/index.html` |
| Logs | `meson-logs/testlog.txt`, `meson-logs/install-log.txt` |
| Source archive | `meson-dist/fplll-6.0.0.tar.xz` |

All build products stay inside the build directory.

## Testing

The suite contains 15 numerical tests and the `llldiff` diagnostic tool:

```sh
meson compile -C build
meson test -C build --print-errorlogs
```

Standard Meson filters apply: select a suite with `--suite`, an individual test
by name (for example `meson test -C build test_lll`), or raise timeouts with
`-t 300`. Results are logged to `build/meson-logs/testlog.txt`.

Fixtures under `tests/lattices/` and `strategies/` are read-only inputs. Tests
write only into their build-tree working directory, `build/tests/`; do not run
the test executables from the source tree. `llldiff` compares two reduction
outputs and is not installed.

## Installation

```sh
meson install -C build
meson install -C build --destdir /abs/stage
```

`meson install` copies the libraries, the two command-line tools, public
headers, pkg-config metadata, and the default strategy data into the configured
prefix. `--destdir` stages the same layout for package managers without
changing the compiled final prefix. Installed layout, relative to the prefix:

| Path | Contents |
| --- | --- |
| `bin/fplll`, `bin/latticegen` | command-line tools |
| `include/fplll.h` | compatibility forwarding header |
| `include/fplll/fplll.h`, `defs.h`, ...; `include/fplll/fplll_config.h` | public headers and the generated configuration header |
| `include/fplll/enum/*.h`; `include/fplll/io/thread_pool.hpp` | enumeration and thread-pool headers |
| `include/fplll/nr/*.h`, `*.inl`; `include/fplll/pruner/pruner.h`, `pruner_simplex.inl` | number, matrix, and pruning headers |
| `lib/libfplll.so.10.0.0` (Linux) or `lib/libfplll.10.dylib` (macOS), plus `lib/libfplll.a` | shared library with SONAME 10 and the static library |
| `lib/pkgconfig/fplll.pc` | pkg-config metadata |
| `share/fplll/strategies/default.json` | default BKZ strategies |

On macOS the shared library's compatibility and current versions are 11.0.0.
Public headers are installed from the source tree; there is no separate
`include` directory in the repository.

The default strategy path is compiled into `bkz_param.cpp` as
`$prefix/$datadir/fplll/strategies`. Configure the final `--prefix` before
building; installing the same build with a different prefix later is
unsupported. To change it, reconfigure and rebuild (for example with
`meson setup --wipe build --prefix /new/prefix`), then reinstall.

CI and release validation check a staged install at `_install`:

```sh
python3 scripts/check_install_manifest.py --prefix _install \
  --forbid-installed-cpp --forbid-private-fragments
python3 scripts/check_headers.py --include-dir _install/include \
  --compiler c++ --std c++23
```

## Downstream use

pkg-config is the installed interface for downstream projects. Add the
prefix's `lib/pkgconfig` to `PKG_CONFIG_PATH` and query the module:

```sh
export PKG_CONFIG_PATH="$HOME/.local/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
pkg-config --cflags --libs fplll
pkg-config --static --cflags --libs fplll
```

The module requires `gmp` and `mpfr`, and `qd` when QD is enabled; `--static`
adds the private libraries. Include the public API as
`#include <fplll/fplll.h>`; the historical `<fplll.h>` entry point remains as a
forwarding header.

Meson consumers discover the installed module through pkg-config and must
request C++23 themselves:

```meson
project('consumer', 'cpp', default_options: ['cpp_std=c++23'])

fplll_dep = dependency('fplll')
executable('consumer', 'main.cpp', dependencies: fplll_dep)
```

Set `PKG_CONFIG_PATH` to the prefix's `lib/pkgconfig` directory before running
`meson setup build`. The C++ standard is not carried by pkg-config, so direct
compilation must select C++23 as well.

## Sanitizers

```sh
meson setup build-asan --buildtype=debug \
  -Db_sanitize=address,undefined -Db_lundef=false
meson compile -C build-asan
meson test -C build-asan --print-errorlogs
```

`-Db_lundef=false` keeps the sanitizer-instrumented shared libraries linkable.

## Code coverage

```sh
meson setup build --buildtype=debug -Db_coverage=true
meson compile -C build
meson test -C build --print-errorlogs
ninja -C build coverage
```

Coverage requires `gcovr` or `lcov`/`genhtml`. The HTML report
is written under `build/meson-logs/coveragereport/`. `coverage` is Meson's
built-in target, run through the Ninja backend. Use a dedicated directory such
as `build-coverage` when other configurations share the tree.

## Development targets

```sh
meson compile -C build dbg       # fplll_dbg and latticegen_dbg
meson compile -C build variants  # fplllv and fpllld variant libraries
meson compile -C build format    # apply clang-format in place
meson compile -C build lint      # check formatting without changing files
meson compile -C build doc       # API documentation, if Doxygen was found
```

`dbg` builds the `DEBUG`-enabled tools. `variants` builds `fplllv` and
`fpllld`, optional variant libraries that are not built by default and never
installed. To build a single target, use its unambiguous Meson specification,
for example `meson compile -C build tools/fplll:executable`; plain `fplll` is
ambiguous because the library target has the same name. `format` and
`lint` use the repository `.clang-format` and skip the bundled third-party
headers `fplll/io/json.hpp` and `fplll/io/thread_pool.hpp`; the targets are
available when Python 3 and `clang-format` are found at configure time, and
`lint` is non-mutating. `doc` writes HTML to `build/doc/html/`, requires
Doxygen at configure time, and generates its Doxygen configuration with Meson.

## Source packages

```sh
meson dist -C build
```

Meson creates `build/meson-dist/fplll-6.0.0.tar.xz` from a clean Git checkout
and, by default, configures, builds, and tests the extracted archive itself;
`--no-tests` skips that validation. The release version comes from `project(..., version: '6.0.0')` in
the root `meson.build`; the ABI version (10.0.0) is maintained independently.

## Clean and uninstall

```sh
meson compile -C build --clean     # remove build outputs, keep configuration
ninja -C build uninstall           # remove files from the last install
meson setup --wipe build           # recreate the build directory
```

`--clean` keeps the configuration, so the next build does not need to
reconfigure. `uninstall` is Meson's built-in target, run through the Ninja
backend; it removes the files recorded by the most recent `meson install` from
that build directory, without deleting build outputs. Reconfigure or wipe the
build directory when changing options or the prefix instead of installing over
an existing tree.

## Troubleshooting

**pkg-config cannot find GMP or MPFR.** Install the development packages (for
example `libgmp-dev` and `libmpfr-dev` on Debian/Ubuntu, `gmp` and `mpfr` with
Homebrew) and check that `pkg-config --modversion gmp mpfr` works. For
dependencies in a nonstandard prefix, set `PKG_CONFIG_PATH` before configuring.

**The installed tools cannot load the strategies.** The default strategy
directory is compiled in from the prefix chosen at configure time; installing
the same build with a different prefix is unsupported. Reconfigure and rebuild
to change it.

**The `doc` target does not exist.** Doxygen was not found when the build
directory was configured. Install Doxygen and wipe and reconfigure the
directory, or configure a new one.

**QD fails to configure.** `-Dqd=enabled` requires a QD installation
discoverable through `qd.pc` or, failing that, through `libqd` and
`qd/dd_real.h` in the compiler search paths; install QD, or leave it disabled
(`-Dqd=disabled`, the default) or automatic (`-Dqd=auto`).

**Meson is too old, or the compiler is rejected.** Meson 1.8 or newer and a
C++23 compiler (GCC 14, Clang 18, AppleClang 16, or newer) are required; C++23
cannot be downgraded.
