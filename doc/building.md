# Building fplll

fplll is built with [Meson](https://mesonbuild.com) and Ninja, and Meson is the
only build interface: run `meson` directly from the repository root. `build/`
is the canonical developer build directory; every other configuration gets its
own sibling directory, and all build products stay inside those directories.

## Requirements

- Meson 1.8 or newer and Ninja.
- A C++23 compiler: GCC 14, Clang 18 (libstdc++ 14 or libc++ 18), AppleClang 16
  or newer. C++23 is mandatory and cannot be downgraded.
- GMP 4.2 or newer, MPFR 2.3 or newer, and pkg-config.
- nlohmann/json 3.12 or newer (header-only). Meson prefers a system package and
  otherwise downloads the pinned `subprojects/nlohmann_json.wrap` during
  `meson setup`.
- Optional: QD (quad-double arithmetic), Doxygen (API documentation), and
  gcovr or lcov/genhtml (code coverage).

Install Meson with `uv tool install meson` (`uvx meson` runs it ephemerally) or
`pipx install meson`.

On macOS, if the selected compiler cannot locate SDK headers or libc++,
set the SDK path in the shell used for configuration:

```sh
export SDKROOT="$(xcrun --show-sdk-path)"
```

## First build

```sh
meson setup build --buildtype=debugoptimized
meson compile -C build
meson test -C build --print-errorlogs
```

The setup step runs once per build directory; after that the loop is
`meson compile -C build` and `meson test -C build`. `debugoptimized` keeps
debug symbols while the numerical suite stays fast. The tools are
`build/tools/fplll` and `build/tools/latticegen`; `meson devenv -C build` opens
a shell with them on `PATH`.

When no suitable system nlohmann/json is installed, the first `meson setup`
downloads the pinned wrap into `subprojects/`, so it needs network access
once; later configurations reuse the downloaded subproject.

## The build directory

| Directory | Role |
| --- | --- |
| `build/` | developer build; Meson writes `build/compile_commands.json` here for editors |
| `build-release/` | optimized build for validation, installation and packaging |
| `build-asan/` | AddressSanitizer and UndefinedBehaviorSanitizer build |
| `build-<name>/` | further one-off configurations (coverage, static or shared) |
| `_install/` | staging prefix for install validation |

Rules that keep the tree predictable:

- `build/` is the only build directory editor tooling reads. Meson rewrites
  `build/compile_commands.json` on every configure; see
  [editor and tooling setup](#editor-and-tooling-setup).
- Configurations are siblings, never nested: use `build-release/`, not
  `build/release/`.
- One directory per configuration. Reconfigure in place with
  `meson configure -Doption=value build`; use a new directory when switching
  compilers, and `meson setup --wipe build` to reset a directory.
- All build directories are ignored by Git, so `git status` stays clean.

## Development loop

```sh
meson compile -C build                   # incremental build
meson test -C build --print-errorlogs    # full suite
meson test -C build test_lll             # single test; --list for all tests
meson compile -C build doc               # API documentation, if Doxygen was found
meson compile -C build dbg               # DEBUG-enabled fplll_dbg and latticegen_dbg
meson compile -C build variants          # fplllv/fpllld variant libraries (never installed)
meson compile -C build --clean           # remove build outputs, keep configuration
```

Meson reconfigures automatically when `meson.build` or an option changes, so
the loop is edit -> compile -> test with no manual configure step. Results are
logged to `build/meson-logs/testlog.txt`. Tests write only into
`build/tests/`; fixtures under `tests/lattices/` and `strategies/` are
read-only inputs.

Optional compiler cache: install `ccache` or `sccache`; Meson detects it on the
next `meson setup`.

## Editor and tooling setup

Language tooling follows the canonical directory, so it is configured once:

- `.clangd` points clangd at `build/`, so indexing, navigation and diagnostics
  use the exact flags of the developer build. Run `meson setup build ...` once
  after cloning, and again after `meson setup --wipe build`.
- `.clang-tidy` is the clang-tidy configuration; apply it with
  `clang-tidy -p build path/to/file.cpp`.
- `.clang-format` is the formatting authority; run `clang-format` directly on
  the files you change. The vendored header `include/fplll/io/thread_pool.hpp` keeps
  its upstream formatting.
- `build/include/fplll/fplll_config.h` and the generated enumeration sources are
  resolved through the compilation database; no manual include paths needed.

VS Code with the clangd extension only needs the clang-tidy toggle (`.vscode/`
is not tracked, so this is a local setting):

```json
{
  "clangd.arguments": ["--background-index", "--clang-tidy", "--header-insertion=never"]
}
```

If clangd reports missing headers or unknown flags, `build/compile_commands.json`
is absent or stale: run `meson setup build ...` and restart the language server.
The database follows the developer build, never `build-release/` or
`build-asan/`.

## Configurations

A configuration is one directory plus one `meson setup` command. The recurring
ones:

```sh
# optimized validation build
meson setup build-release --buildtype=release
meson compile -C build-release
meson test -C build-release --print-errorlogs

# AddressSanitizer + UndefinedBehaviorSanitizer (slower; raise the timeout)
meson setup build-asan --buildtype=debug \
  -Db_sanitize=address,undefined -Db_lundef=false
meson compile -C build-asan
UBSAN_OPTIONS=halt_on_error=1 meson test -C build-asan --print-errorlogs -t 5

# code coverage
meson setup build-coverage --buildtype=debug -Db_coverage=true
meson compile -C build-coverage
meson test -C build-coverage --print-errorlogs
ninja -C build-coverage coverage

# single library flavour
meson setup build-static --buildtype=release --default-library=static
meson setup build-shared --buildtype=release --default-library=shared
```

Strict warnings are available with `-Dwarning_level=3 -Dwerror=true`. The
sources still contain pre-existing diagnostics, so this configuration is
advisory: it is expected to fail until the warning baseline is cleaned up.

## Build options

| Option | Default | Purpose |
| --- | --- | --- |
| `-Dqd=enabled/disabled/auto` | `disabled` | QD arithmetic; `enabled` fails if QD is missing |
| `-Dmax_enum_dim=N` | `256` | maximum enumeration dimension (positive integer) |
| `-Dmax_parallel_enum_dim=N` | `120` | maximum parallel enumeration dimension; `0` disables it, valid values are `20` to `160` in steps of ten, and it must stay below `max_enum_dim` |
| `-Dtests=true/false` | `true` | build and register the tests (top-level builds only) |

The optional external enumerator is imported as the Meson dependency
`fplll_extenum`: install a `fplll_extenum.pc` on `PKG_CONFIG_PATH` (or provide
a wrap) whose library exports `fplll_extenum_enumerate` with the
`extenum_fc_enumerate` signature from `<fplll/enum/enumerate_ext_api.h>`.
Without that dependency the built-in enumerator is used, and applications can
always override the default at run time with `fplll::set_external_enumerator()`.

Built-in Meson options replace project-specific switches: `--buildtype`,
`--default-library` (`both` by default; `shared` and `static` are also
accepted), `--prefix`, `-Dwarning_level=3 -Dwerror=true` (or
`--warnlevel=3 --werror`), `-Db_sanitize`, `-Db_coverage`, `-Dc_args`,
`-Dcpp_args`, and `-Dcpp_std`. C++23 is mandatory; `-Dcpp_std` accepts a newer
standard but rejects older ones. Inspect a configured directory with
`meson configure build`.

## Build output layout

| Output | Location relative to the build directory |
| --- | --- |
| Libraries | `src/libfplll.{dylib,so*,a}` |
| Tools | `tools/fplll`, `tools/latticegen`, plus `tools/fplll_dbg` and `tools/latticegen_dbg` |
| Tests | `tests/test_*`, `tests/llldiff` |
| Generated files | `include/fplll/fplll_config.h`, `src/enumlib_dim.<dim>.cpp` |
| Compilation database | `compile_commands.json` |
| API documentation | `doc/html/index.html` |
| Logs | `meson-logs/testlog.txt`, `meson-logs/install-log.txt` |
| Source archive | `meson-dist/fplll-6.0.0.tar.xz` |

To build a single target, use its unambiguous Meson specification, for example
`meson compile -C build tools/fplll:executable`; plain `fplll` is ambiguous
because the library target has the same name.

## Testing

The suite contains 22 tests (21 numerical tests and the CLI regression test) and the `llldiff` diagnostic tool.
Standard Meson filters apply: select a suite with `--suite fplll`, an
individual test by name, or raise timeouts with `-t 300`. `llldiff` compares two
reduction outputs and is not installed.

## Installing

The final prefix is compiled into the library (the default strategy path is
`$prefix/$datadir/fplll/strategies`), so choose it when configuring:

```sh
meson setup build-release --buildtype=release --prefix "$HOME/.local"
meson compile -C build-release
meson test -C build-release --print-errorlogs
meson install -C build-release
```

`meson install` copies the libraries, the two command-line tools, public
headers, pkg-config metadata, and the default strategy data. `--destdir` stages
the same layout for package managers without changing the compiled final
prefix. Installed layout, relative to the prefix:

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
The installed tools use relative runtime library lookup, including under
`DESTDIR`; staged BKZ runs still need an explicit staged strategy file because
the default strategy path retains the final prefix. Public headers are installed from the source tree; there is no separate
`include/` directory in the repository. Installing the same build with a
different prefix later is unsupported: reconfigure and rebuild instead.

## Source packages

```sh
meson dist -C build-release
```

Meson creates `build-release/meson-dist/fplll-6.0.0.tar.xz` from a clean Git
checkout and, by default, configures, builds, and tests the extracted archive
itself; `--no-tests` skips that validation. `--allow-dirty` suppresses the dirty-tree
error but still archives the last commit, excluding all uncommitted changes. The release version comes from
`project(..., version: '6.0.0')` in the root `meson.build`; the ABI version
(10.0.0) is maintained independently.

The tracked `subprojects/nlohmann_json.wrap` is part of the archive, but the
downloaded subproject source is not. Building the extracted archive therefore
needs a system nlohmann/json package (3.12.0 or newer) or a wrap download,
unless the archive was created with `--include-subprojects`, which vendors the
subproject sources. That option only includes subprojects configured in the
build directory running `meson dist`, so cut the archive from a wrap-based
build when it must be self-contained. Offline and distribution builds can add
`-Dwrap_mode=nodownload` to `meson setup` (or `--wrap-mode=nodownload`) to
turn a missing download into an explicit configure error.

## Downstream use

pkg-config is the installed interface. Add the prefix's `lib/pkgconfig` to
`PKG_CONFIG_PATH` and query the module:

```sh
export PKG_CONFIG_PATH="$HOME/.local/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
pkg-config --cflags --libs fplll
pkg-config --static --cflags --libs fplll
```

The module requires `gmp` and `mpfr`. QD builds require `qd` when `qd.pc`
is available; compiler-discovered QD is exported as a public link dependency
instead. `--static` adds the private libraries. Include the public API as
`#include <fplll/fplll.h>`; the historical `<fplll.h>` entry point remains as a
forwarding header. Meson consumers discover the module through pkg-config and
must request C++23 themselves:

```meson
project('consumer', 'cpp', default_options: ['cpp_std=c++23'])

fplll_dep = dependency('fplll')
executable('consumer', 'main.cpp', dependencies: fplll_dep)
```

Set `PKG_CONFIG_PATH` before `meson setup`; pkg-config does not carry the C++
standard, so direct compilation must select C++23 as well.

## API documentation

With Doxygen installed when the build directory is configured:

```sh
meson compile -C build doc     # writes build/doc/html/index.html
```

Doxygen was not found for the current `build/` directory if the target is
missing: install Doxygen and recreate the directory
(`meson setup --wipe build ...`). Library sources, the README, changelog, and
usage, build, and contribution guides are documentation inputs; build
configuration headers and local notes are excluded. Never generate
documentation into the source tree.

## Local validation

Builds, tests, installs and release archives are validated locally: run the
same Meson commands before submitting changes or tagging a release.

1. Developer build: `meson test -C build --print-errorlogs`.
2. Optimized build with a staging prefix, then its test suite:
   `meson setup build-release --buildtype=release --prefix="$PWD/_install"`,
   `meson compile -C build-release`,
   `meson test -C build-release --print-errorlogs`.
3. Sanitizer build: the [configuration](#configurations) above, with
   `UBSAN_OPTIONS=halt_on_error=1 meson test -C build-asan --print-errorlogs -t 5`.
4. Static and shared library configurations: configure each as
   `build-<library>/` with `--default-library=<library>`, then compile and test.
5. Install and validate the staged tree:

   ```sh
   meson install -C build-release
   python3 scripts/check_install_manifest.py --prefix _install \
     --forbid-installed-cpp --forbid-private-fragments
   python3 scripts/check_headers.py --include-dir _install/include \
     --compiler c++ --std c++23
   ```

6. Release archive: commit first, then `meson dist -C build-release`; it
   requires a clean Git checkout and builds and tests the extracted tree.

Strict warnings (`-Dwarning_level=3 -Dwerror=true`) are advisory as described
[above](#configurations).

## Clean and uninstall

```sh
meson compile -C build --clean     # remove build outputs, keep configuration
meson setup --wipe build           # recreate build/ from scratch
ninja -C build uninstall           # remove files from the last install
```

`--clean` keeps the configuration, so the next build does not need to
reconfigure. `uninstall` is Meson's built-in target, run through the Ninja
backend; it removes the files recorded by the most recent `meson install` from
that build directory, without deleting build outputs.

## Troubleshooting

**`ld: library 'c++' not found`, or `<algorithm> file not found` on macOS.**
The selected compiler may need an explicit SDK path. Set
`export SDKROOT="$(xcrun --show-sdk-path)"` in the build shell (or IDE)
before running `meson setup`. AppleClang can normally locate its SDK itself.

**clangd reports missing includes or unknown flags.** `build/compile_commands.json`
is missing or stale. Run `meson setup build ...`, then restart the language
server.

**pkg-config cannot find GMP or MPFR.** Install the development packages (for
example `libgmp-dev` and `libmpfr-dev` on Debian/Ubuntu, `gmp` and `mpfr` with
Homebrew) and check that `pkg-config --modversion gmp mpfr` works. For
dependencies in a nonstandard prefix, set `PKG_CONFIG_PATH` before configuring.

**The installed tools cannot load the strategies.** The default strategy
directory is compiled in from the prefix chosen at configure time; reconfigure
and rebuild to change it.

**The `doc` target does not exist.** Doxygen was not found when the build
directory was configured. Install Doxygen and wipe and reconfigure the
directory, or configure a new one.

**QD fails to configure.** `-Dqd=enabled` requires a QD installation
discoverable through `qd.pc` or, failing that, through `libqd` and
`qd/dd_real.h` and `qd/qd_real.h` in the compiler search paths; install QD, or leave it disabled
(`-Dqd=disabled`, the default) or automatic (`-Dqd=auto`).

**The nlohmann/json wrap cannot be downloaded.** Configure needs a system
nlohmann/json 3.12.0 or newer, or network access for the pinned wrap. Install
a distro package (for example `nlohmann-json3-dev` on Debian/Ubuntu) or
pre-populate `subprojects/nlohmann_json-3.12.0/`; the error message names
`nlohmann_json`.

**Meson is too old, or the compiler is rejected.** Meson 1.8 or newer and a
C++23 compiler (GCC 14, Clang 18, AppleClang 16, or newer) are required; C++23
cannot be downgraded.
