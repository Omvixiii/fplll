<!-- SPDX-FileCopyrightText: 2026 Omvixiii
     SPDX-License-Identifier: LGPL-2.1-or-later -->

# Contributing to fplll

This document covers the development environment, the developer build, the
layout of the sources, the coding conventions, and the workflow for making
changes.

## Getting the source

The 6.0 line is developed on the `refactor/meson` branch, which is not the
repository default yet and is published with the 6.0 release. Switch to it after
cloning (skip the switch once the 6.0 line is the default):

```sh
git clone https://github.com/Omvixiii/fplll.git
cd fplll
git switch refactor/meson
git switch --create fix-broken-thing
```

## Development environment

Install the dependencies from the [requirements](README.md#requirements), plus
`clang-format`, `clang-tidy` and, for the REUSE check, `reuse`. `ccache` or
`sccache` is optional but speeds up repeated builds; Meson picks either up
automatically.

`reuse` needs a file-encoding backend; install the extra when the distribution
package does not pull one in, e.g.
`uv tool install "reuse[charset-normalizer]"` or
`pipx install "reuse[charset-normalizer]"`.

nlohmann/json comes from a system package when available; otherwise the pinned
wrap in `subprojects/` is fetched once. Catch2 is needed only for the opt-in
test suite (see below) and is resolved the same way.

## Build and test

The development build uses Meson's `debugoptimized` build type:

```sh
meson setup build --buildtype=debugoptimized
meson compile -C build
```

`build/` is the development and release-validation directory. Reconfigure it in
place with `meson configure build -Doption=value`; create a sibling
`build-<purpose>/` directory when a configuration must coexist with the default
one (the test build below is one such case). The user-facing commands in
`README.md` also use `build/`: in a checkout that has already been configured
for a release build, wipe the directory first
(`meson setup --wipe build -Dbuildtype=debugoptimized`).

Options that matter while developing:

| Option | Default | Meaning |
| --- | --- | --- |
| `-Dtests=true` | `false` | build and register the test suite (resolves Catch2; see below) |
| `-Ddebug_checks=true` | `false` | compile internal invariant checks into the library |
| `-Dmax_enum_dim=N` | `256` | compile the enumerator for dimensions up to N |
| `-Dmax_parallel_enum_dim=N` | `120` | parallel-enumeration dimension limit (`0` disables it; otherwise a multiple of 10 from `20` to `160`) |

The test suite is opt-in so that plain builds stay free of Catch2. Enabling it
also compiles the library with the internal test hooks (`-DFPLLL_TESTING`), so
give it a sibling build directory instead of toggling `build/`:

```sh
meson setup build-tests --buildtype=debugoptimized -Dtests=true
meson test -C build-tests --print-errorlogs
```

`meson test` builds the single Catch2 test executable on demand. The suite is
registered once per module tag, so a module can be rerun on its own:

```sh
meson test -C build-tests lll              # one module
meson test -C build-tests --list           # all registered tests
meson test -C build-tests --suite fplll    # the whole suite
```

A test binary can also be run directly, for example
`build-tests/tests/fplll_tests --list-tests` or
`build-tests/tests/fplll_tests "[bkz]" --order decl`. Fixtures under
`tests/lattices/` and `strategies/` are read-only inputs; tests write only into
their working directory.

Internal invariant checks are a separate switch, independent of the build type
and of `NDEBUG`:

```sh
meson setup build-check --buildtype=release -Ddebug_checks=true -Dtests=true
meson compile -C build-check
meson test -C build-check --print-errorlogs
```

Strict warning builds are available with
`-Dwarning_level=3 -Dwerror=true`; the tree is expected to stay clean at that
level.

## Source layout

The library is split by module between `include/fplll/` (public headers) and
`src/` (implementation). The test suite mirrors the same modules under
`tests/`.

```text
include/fplll/
├── defs.h                  # status, options, constants, error and check helpers
├── fplll.h                 # aggregate public header
├── nr/                     # numeric types, vectors and matrices
├── gso/                    # Gram-Schmidt representations (basis, Gram, Householder)
├── reduction/              # LLL, HLLL, BKZ, HKZ and their parameters
├── enum/                   # enumeration core
├── pruner/                 # pruning parameters and optimizer
├── svpcvp.h                # SVP/CVP entry points
├── threadpool.h            # get_threads/set_threads
└── util.h                  # shared helpers

src/
├── detail/                 # private headers (float type/precision resolution,
│                           #   CPU time, worker pool and enumeration session)
├── enum/                   # enumeration core and the parallel backend (enum/parallel/)
├── gso/                    # GSO implementations
├── pruner/                 # pruner implementation
├── reduction/              # LLL (lll/), HLLL (hlll/) and BKZ/HKZ (bkz/): kernels and drivers
├── svpcvp.cpp
├── threadpool.cpp
└── util.cpp
```

Private headers live under `src/`; nothing below `src/` is installed. The
public headers are self-contained and are all reachable from
`<fplll/fplll.h>`.

Names state the layer a translation unit implements. An installed header
declares one layer of the API: the option structs and the entry-point functions
(`lll.h`, `hlll.h`, `bkz.h`), or the configuration data and its loader
(`bkz_param.h`). It never declares an algorithm kernel. A private
`<module>_kernel.{h,cpp}` is an algorithm kernel: its class, the verification
helper and the `extern template` declarations where it has them. A
`<module>_driver.cpp` implements the public entry points of `<module>.h`:
option validation, backend/precision selection and dispatch into the kernels.
A translation unit that directly implements a public header keeps the header's
stem (`bkz/bkz_param.cpp` implements `bkz_param.h`), and a private header named
after the class it declares (`wrapper.h` declares `Wrapper`) is a shared engine
below the drivers. Helpers shared by a set of drivers live at the module root
in a header named after the family (`lll_family.h` holds the checks shared by
the LLL and HLLL drivers).

No translation unit mixes an algorithm kernel with the entry-point layer, and
no installed header declares a kernel: the file name and the layer of a
definition agree. The reduction module is laid out as an example:

```text
include/fplll/reduction/          # installed: options + entry points + data
├── lll.h                         # LLLParameters, LLLOptions, lll_reduction()
├── hlll.h                        # HLLLParameters, HLLLOptions, hlll_reduction()
├── bkz.h                         # HKZOptions, bkz_reduction(), hkz_reduction()
└── bkz_param.h                   # Strategy, BKZParam, load_strategies_json()

src/reduction/                    # library: kernels, drivers, private headers
├── lll/                          # LLL
│   ├── lll_kernel.h/.cpp         # LLLReduction, is_lll_reduced
│   └── lll_driver.cpp            # lll_reduction(): validation, backend, dispatch
├── hlll/                         # HLLL
│   ├── hlll_kernel.h/.cpp        # HLLLReduction, is_hlll_reduced
│   └── hlll_driver.cpp           # hlll_reduction(): validation, backend, dispatch
├── bkz/                          # BKZ, HKZ, SD-BKZ and slide
│   ├── bkz_kernel.h/.cpp         # BKZReduction, BKZAutoAbort
│   ├── bkz_driver.cpp            # bkz_reduction()/hkz_reduction(): validation, backend, dispatch
│   └── bkz_param.cpp             # Strategy table and its JSON loader
├── wrapper.h/.cpp                # Wrapper: self-tuning backend/precision escalation engine
└── lll_family.h                  # checks shared by the LLL and HLLL drivers
```

The BKZ/HKZ units and the wrapper engine reach into `lll/` through relative
private includes (the wrapper also into `hlll/`); the LLL and HLLL drivers
share the module-root `wrapper.h` and `lll_family.h`, and the remaining units
only include private headers under `src/detail/`.

`tools/` contains the installed command-line programs built by Meson;
`scripts/` contains maintainer-only helpers that are not part of the build,
such as the generator of the pruner's compile-time tables.

Include layering:

- every installed header includes the standard-library and project headers it
  uses directly; relying on transitive includes is not supported and is
  checked by clangd's include-cleaner diagnostics;
- `<fplll/fplll.h>` is the aggregate public header and `<fplll/nr/nr.h>` is the
  numeric umbrella that pulls in the backend `.inl` files; both mark their
  re-exports with `// IWYU pragma: export` so that consumers (and include
  analyzers) treat them as the provider of those symbols;
- the backend `.inl` files and everything under `src/` are implementation
  details and are never included by users.

## Design principles

The public API is value-based and contract-first:

- configuration is passed with plain option structs (`LLLOptions`, `BKZParam`,
  `GSOOptions`, ...) that are validated once, before any caller data is
  modified;
- reductions return a `ReductionReport` describing what happened (status,
  backend and precision actually used, guarantees, statistics) instead of a
  bare status code;
- global state is handled with RAII guards: `ScopedPrecision` /
  `ScopedGsoPrecision` for the MPFR working precision, `RowOperations` for
  batches of GSO row operations, and small guards for the FPU rounding mode;
- the floating-point backend is selected once, by
  `detail::dispatch_float_type()`; algorithm code never repeats a `FloatType`
  `switch`;
- hot loops stay free of allocation, exceptions and formatted output; the
  observable contracts (status/report) live at the algorithm boundaries, not
  in the inner loops.

Header layering follows from the same idea: module headers are self-contained,
the aggregate headers (`<fplll/fplll.h>`, `<fplll/nr/nr.h>`) own their
re-exports, and headers that are only needed by an implementation live under
`src/`.

## Coding conventions

- The library, installed headers, tools and tests require **C++23**. Use the
  language where it pays off: `enum class`, designated initializers,
  `std::format`/`std::print`, `std::span`, `std::string_view`, `if constexpr`,
  `inline constexpr`, `[[nodiscard]]`, `std::to_underlying`.
- Types are `CamelCase`; functions, methods, parameters and variables are
  `lower_case`; private data members end in `_` only when a plain name would
  clash with a parameter.
- Public configuration is expressed with **option structs** passed by
  `const&`, not with long parameter lists or integer flag bitsets. Optional
  values use `std::optional`.
- Enumerations are scoped (`enum class`); provide `to_string()` where the value
  is printed.
- Do not add namespace macros, `using namespace std`, or a block of `using
  std::…` re-exports. Qualify standard names with `std::` directly.
- Headers include what they use; do not rely on transitive includes.
- The public integer type is `ZZ_mat<mpz_t>`; the `long` fast path is an
  implementation detail of LLL, and neither dedicated `long` entry points nor a
  `long double` floating-point backend are part of the API.
- Errors use `FPLLL_CHECK(condition, "message {}", args…)` / `FPLLL_ABORT`,
  which throw `fplll::Error`; `FPLLL_DEBUG_CHECK` is reserved for internal
  invariants and aborts. Do not throw enum classes or other control-flow types.
- Prefer value semantics and `std::unique_ptr` over reference members and raw
  owning pointers. Hot loops must not allocate or format strings.
- Preprocessor switches are limited to the build options (`FPLLL_WITH_QD`,
  `FPLLL_DEBUG_CHECKS`, the generated enumeration-dimension limits), the
  `FPLLL_TESTING` hooks of the internal thread pool, and the
  platform/library-version guards (MSVC, MPFR); everything else is plain C++23.
- Every source file keeps its SPDX identifiers; new files get a
  `SPDX-FileCopyrightText` line and the `LGPL-2.1-or-later` license identifier
  (or MIT for the parallel-enumeration code).

### Formatting and static analysis

`.clang-format` is authoritative; `clang-format --style=file -i` the files you
touch. The tree is formatted with clang-format 23 and is also kept clean under
the Apple clang-format 21 shipped with Xcode.

`.clang-tidy` holds the curated check set; its `HeaderFilterRegex` restricts
diagnostics to the project sources, so GMP, MPFR, Catch2 and the other
third-party code never show up. `.clangd` points clangd at
`build/compile_commands.json` and enables the include-cleaner diagnostics for
project files, so headers must include what they use directly (no reliance on
transitive includes). Because the default `build/` configuration skips the
tests, `tests/` translation units have no entry in its compilation database;
use a tests-enabled directory (or point `CompilationDatabase` at `build-tests`)
when working on the test sources. Apply the checks to one file with:

```sh
clang-tidy -p build path/to/file.cpp
```

and to the whole tree (the gate the tree is kept at), using the tests-enabled
database so the `tests/` units are covered too:

```sh
run-clang-tidy -p build-tests -j 8 -source-filter '^(?!.*/subprojects/).*/(include/fplll|src|tools|tests)/'
```

`clang-tidy --verify-config` checks that every configured check name exists in
the installed clang-tidy. The tree is expected to be free of diagnostics under
the configured checks and of missing/unused include diagnostics under clangd;
suppress a deliberate, documented case with
`// NOLINT(check-name)  // reason` instead of adding the check to the exclusion
list.

### License and REUSE

The tree follows the [REUSE specification](https://reuse.software/): every
tracked file carries SPDX information, either in its own header or through
`REUSE.toml` for data files that cannot hold a comment. Run the check from the
repository root:

```sh
reuse lint
```

A new file gets an `SPDX-FileCopyrightText` line and an `SPDX-License-Identifier`
tag; add it to `REUSE.toml` instead when the format has no comment syntax.

## Making changes

- Keep each commit focused: `type(scope): summary`, with `build`, `chore`,
  `docs`, `feat`, `fix`, `license`, `refactor`, `style` or `test` as the type
  and `!` for breaking changes.
- Add a small deterministic test for new behaviour. Tests are Catch2 v3 cases:
  use `TEMPLATE_TEST_CASE` with `FPLLL_TEST_FP_BACKENDS` for every floating
  backend, `CHECK` where execution can continue and `REQUIRE` where it cannot,
  and `INFO`/`CAPTURE` for diagnostics.
- Update the documentation that covers what you changed: `README.md` for user
  visible behaviour, `CHANGELOG.md` for changes worth upgrading for, and the
  Doxygen comments for the API.
- Keep formatting changes separate from functional changes.

## Documentation

`README.md` is the user documentation, `CHANGELOG.md` records the user-visible
changes, and Doxygen generates the reference from the headers and sources (with
`README.md` as the main page). Run

```sh
meson compile -C build doc
```

to write the Doxygen HTML into `build/doc/html/`; the target only exists when
Doxygen was found at configure time. Documentation is never generated into the
source tree.
