<!-- SPDX-FileCopyrightText: 2015-2024 The FPLLL development team
     SPDX-FileCopyrightText: 2026 Omvixiii
     SPDX-License-Identifier: LGPL-2.1-or-later -->

# fplll

fplll is a C++23 library for lattice reduction and lattice problems, which provides a command-line toolkit. It supports LLL (including the self-tuning wrapper, the proved and heuristic variants and Householder LLL), BKZ 2.0 with pruned enumeration (self-dual BKZ, slide reduction and HKZ), exact and heuristic shortest- and closest-vector solvers, and a pruning-parameter optimizer. The basis arithmetic is exact (GMP); the Gram-Schmidt step runs in double, in MPFR at arbitrary precision, or, when libqd is enabled, in double-double or quad-double precision.

## Requirements

- Meson 1.8.0 or newer and Ninja;
- a C++23 compiler and standard library;
- GMP and MPFR;
- libqd, when configured with `-Dqd=enabled`;
- Catch2 3.0.0 or newer, only when the test suite is enabled (`-Dtests=true`);
- pkg-config, when the installed library is used from another build system.

The default build is a shared library with Meson's `debug` build type,
**installed under `/usr/local`**, with QD support disabled and the test suite
off by default.
nlohmann/json (strategy tables) comes from a system package when available;
otherwise Meson fetches the pinned wrap in `subprojects/` once. Catch2 is
resolved only when tests are enabled, so the default build compiles just the
library and the two tools.

## Build and install

```sh
meson setup build --buildtype=release
meson compile -C build
meson install -C build
```

`meson install` writes to `/usr/local` by default, so either run it as root or
pass a `--prefix` you can write to. `-Dqd=enabled` adds the DD/QD backends and
`meson configure build` lists the other options.

The test suite is opt-in and lives in its own build directory, because enabling
it also compiles the library with test-only hooks:

```sh
meson setup build-tests --buildtype=release -Dtests=true
meson test -C build-tests --print-errorlogs
```

`-Dtests=true` pulls in Catch2 — from a system package when available,
otherwise from the pinned `subprojects/catch2.wrap` — and `meson test` builds
the single test executable on demand. The default `build/` stays free of
Catch2.

## Command-line tools

The build produces `fplll`, which runs a reduction or solver on a matrix read
from a file or stdin (one basis vector per row), and `latticegen`, which
generates test lattices:

```sh
build/tools/latticegen u 20 30 | build/tools/fplll -a bkz -b 10
```

`fplll --help` lists all options, including the output-format language. BKZ
strategy tables are loaded with `-s`; `-s default.json` selects the table
installed with the library.

## Library

Everything is available through the aggregate header `<fplll/fplll.h>`:

```cpp
#include <fplll/fplll.h>
#include <print>

int main()
{
  fplll::ZZ_mat<mpz_t> basis(3, 4);
  basis.gen_intrel(3);

  const fplll::ReductionReport report = fplll::lll_reduction(
      basis, fplll::LLLOptions{.parameters = {.delta = 0.99, .eta = 0.51}});
  std::println("backend: {}, precision: {}, guaranteed: {}",
               fplll::to_string(report.float_type), report.precision,
               report.guaranteed);
}
```

Reductions return a `ReductionReport` with the status, the backend and
precision actually used, the proved guarantee, and the BKZ/HKZ statistics;
diagnostics go to the stream installed with `set_log_stream()`. Parallelism is
controlled with `fplll::set_threads()`. Library errors throw `fplll::Error`
(misusing the thread pool throws `std::logic_error`). Downstream builds use the
pkg-config module `fplll` or Meson's `dependency('fplll')`.

## Documentation

- `CONTRIBUTING.md` — development build, source layout and conventions;
- `CHANGELOG.md` — user-visible changes and the 6.0 upgrade notes;
- Doxygen API reference: build the `doc` target (Doxygen must be found at
  configure time), which writes HTML to `build/doc/html/`.

## Citation

If you use fplll in academic work, please cite:

```bibtex
@unpublished{fplll,
  author = {The {FPLLL} development team},
  title = {{fplll}, a lattice reduction library, {Version}: 5.5.0},
  year = 2023,
  note = {Available at \url{https://github.com/fplll/fplll}},
  url = {https://github.com/fplll/fplll}
}
```

## License

fplll is licensed under the GNU Lesser General Public License, version 2.1 or
later. Sources carry SPDX license identifiers; the third-party parallel
enumeration code under `src/enum/parallel/` is MIT licensed. The tree follows
the REUSE specification (data files are covered by `REUSE.toml`), and `LICENSE`
and the per-license texts under `LICENSES/` are installed under
`share/doc/fplll`.
