# Changelog

## Unreleased

### Build

- Discover the optional external enumeration library as the Meson dependency
  `fplll_extenum` (a `fplll_extenum.pc` on `PKG_CONFIG_PATH`, or a wrap)
  instead of the `extenum_dir`, `extenum_library` and `extenum_function`
  project options. The library exports `fplll_extenum_enumerate` with the
  `extenum_fc_enumerate` signature and becomes the default external
  enumerator; without that dependency the built-in enumerator is used. The
  configured symbol is no longer written to the installed `fplll_config.h`,
  and `-Dextenum_*` is no longer accepted.

## 6.0 (2026-10-23)

fplll 6.0 requires C++23 and a rebuild against SONAME 10.  It removes the
MPIR, DPE and `ZZ_mat<double>` backends and the recursive-enumeration switch,
makes the library silent by default, modernizes the option enums and strategy
API, and replaces the generated pruner tables with runtime computation.  See
[the 6.0 migration guide](doc/migration-6.0.md).

### Fixed

- Invalid command-line input and configuration files are rejected instead of
  crashing on out-of-bounds indices: lattice-generator parameters, BKZ
  strategy tables, the strategy JSON schema, GSO dump paths and numeric
  option ranges.
- HLLL no longer reports success for failed reductions, handles one-row
  bases and uses eta (not delta) for its weak size-reduction bound; BKZ
  skips bases without non-zero rows.
- The global GMP random state is mutex-protected, initialized safely and no
  longer reseeded in the middle of a stream; concurrent reductions are safe
  but the random stream is no longer reproducible when randomization is used
  from several threads.
- The thread pool tracks outstanding tasks and propagates job exceptions
  through `wait_work()`/`wait_sleep()` instead of terminating the process.
- `dump_mu_d` appends to its output vector; `FPLLL_CHECK`/`FPLLL_ABORT` are
  usable from consumer code outside namespace `fplll`.
- `FP_NR` integer conversions are range-checked instead of relying on
  undefined float-to-integer casts, and the in-place arithmetic
  overloads return by value rather than a dangling rvalue reference.
- Portability: `std::`-qualified math in the optional long-double and QD
  backends, MPFR-based string-to-double parsing (no floating-point
  `std::from_chars`), `std::numbers::e` instead of `M_E`, `_MSVC_LANG` for
  the C++23 check, 64-bit enumeration node counters and a 1024 limit for
  `max_enum_dim`.

### Build and development

- Use Meson 1.11 or newer with Ninja as the sole build toolchain; remove the
  intermediate CMake build and the legacy Autotools files.
- Require C++23 for the library, installed headers, tools and tests.
- Remove the MPIR backend, the recursive-enumeration build switch and the
  `ZZ_mat<double>` integer backend.
- Remove the vendored nlohmann/json copy `fplll/io/json.hpp` and resolve JSON
  support through Meson: prefer a system package (>= 3.12.0), otherwise
  download the pinned `subprojects/nlohmann_json.wrap`.
- Replace build presets with Meson options and independent build directories.
- Make `build/` the canonical Meson developer build directory and point clangd
  at its compilation database; every other configuration uses a sibling
  `build-<name>/` directory.
- Generate configuration headers and enumeration sources in the build tree;
  keep libraries, executables and test working directories there as well.
- Provide shared and static libraries (`both` by default), optional debug and
  variant libraries, pkg-config metadata and `meson dist` source archives.
- Remove the CMake package config and exported targets; downstream projects
  use pkg-config or Meson's `dependency('fplll')`.
- Replace CPack source archives and the distcheck workflow with `meson dist`,
  which builds and tests the generated archive.
- Remove the GitHub Actions workflow; builds, tests, installs and release
  archives are validated locally with the Meson commands documented in the
  build guide.
- Migrate contributor instructions to Meson.
- Consolidate historical release notes here; keep local migration notes out of
  version control and source packages.

### Removed

- The DPE floating-point backend (`FT_DPE`, `-f dpe`, `nr/dpe.h`,
  `nr_FP_dpe.inl`).  Heuristic LLL now uses `double` with GSO row-exponent
  tracking, and falls back to MPFR when the required precision exceeds double.
  Benchmarked heuristic LLL is about 60% faster on 60-100 dimensional integer
  relation lattices with identical reduction quality; the default wrapper path
  is unchanged.

### Changed

- Library diagnostics are silent by default and go through
  `fplll::set_log_stream()`; the CLI enables them for verbose and pruning runs.
- Integer randomization uses `std::mt19937_64` with a deterministic default
  seed; the GMP random state is seeded from the same API.
- CPU timings use `clock_gettime`/`std::clock` through an internal helper; the
  POSIX headers are gone from the public headers.
- Numeric command-line arguments and `Z_NR<long>`/`FP_NR<double>` string
  parsing use `std::from_chars` and reject malformed input; long-double string
  parsing uses MPFR and is locale-independent.
- The 4,100-line generated factorial and unit-ball-volume tables were replaced
  by runtime GMP/MPFR computation in the pruner; numeric output is unchanged.

## Historical releases

The entries below preserve the release notes previously stored in NEWS.
Dates and missing release entries have not been reconstructed; consult Git tags
and history for releases not covered by this archive.

## 5.0

- switched to C++11
- switched to more open development model on GitHub, cf. CONTRIBUTING.md
- support for computing optimal pruning parameters for a given lattice
  dimension/shape/radius
- BKZ 2.0: recursive BKZ preprocessing, extreme pruning, gaussian
  heuristic bound
- precomputed BKZ (2.0) reduction strategies up to block size 90, cf.
  <https://github.com/fplll/strategizer>
- Self-Dual BKZ and Slide reduction
- faster, recursive enumeration implementation
- Gaussian lattice sieving
- Python interface at <https://github.com/fplll/fpylll>
- optional dumping Gram-Schmidt vectors during execution of algorithm
- q-ary lattice generator
- instructions on how to cite
- optional support for doubledouble and quaddouble
- pkg-config support
- API documentation at <https://fplll.github.io/fplll/>
- revised build system supporting parallel build
- copyright headers
- dropped fplllv3 compatibility layer
- greatly increased test coverage, tests are run on every commit or
  pull request
- coding/naming convention
- FP API improved to allow for more readable/natural code

## 4.0.4

- compatibility for packages that still expect to build against
  libfplll version 3.
- updates of config.sub and config.guess
- better check that MPFR and GMP are installed
- compilation issue with NaN's

## 4.0.3

- Compiler issues with Cygwin

## 4.0.2

- Changed formulation for the license, in README.html

## 4.0.1

- Compiler warnings about unused parameters fixed

## 4.0.0

- BKZ reduction, available with 'fplll -a bkz -b block_size'

## 3.1.3

- Fixed build process when DESTDIR is defined

## 3.1.2

- Memory leak fixed in the wrapper.

## 3.1.1

- Some code is no longer included in the header files but is compiled in
  libfplll instead. The -lfplll option will now be required for apps which could
  be compiled without it.
- 'make install' puts all header files but fplll.h in a subdirectory named fplll
- New programming interface for the library (documented in README.html).
- 'generate' renamed to 'latticegen'.
- Removal of fplll_verbose. Use the '-v' option instead.
- Minor bugs fixed in all versions of LLL.
- More important bugs fixed in the SVP solver.
- Optimization of the SVP solver.
- By default, 'fplll -a svp' now gives the coordinates of a shortest vector in
  the _standard_ basis.

## 3.0

- short lattice vector enumeration algorithm (by Xavier Pujol).
- GPL -> LGPLv2
- no more need to specify the number of rows and columns in the input
  of the fplll binary.
- new version of dpe.h [Patrick Pelissier and Paul Zimmermann]

## 2.1

- "configure/make/make install" packaging (thanks to Martin Albrecht)
- few minor changes to make fplll portable to SAGE (thanks to Martin
  Albrecht)
- conversion scripts between MAGMA and fplll formats.
- a bug fixed in fast_early (discovered by Martin Albrecht)

## 2.0

fplll-2.0 is an improved version of fplll-1.3. Parts of it resemble
Magma's LLL (Allan Steel, Damien Stehle) and/or NTL's LLL (Victor
Shoup).

The improvements from fplll-1.3 to fplll-2.0 were mostly performed
by David Cade. Here is a brief summary of the changes:

- the major algorithmic improvement is the so-called wrapper, which
  chooses for the user a guess of the right sequence of heuristic/proved
  variants to use, in order to finish as quick as possible, but in a
  reliable way.

- the early-reduction strategy of Allan Steel has been integrated. It
  consists in size-reducing vectors earlier than what would have been
  done in the standard LLL. This is not integrated into the wrapper yet.

- we switched from C to C++, to simplify our lives with the wrapper.
  It is extremely convenient for the use of the underlying arrithmetics
  (integers and floating-point numbers).

- we translated the matrix indices by 1: the first matrix entry is
  `B[0][0]` instead of `B[1][1]`.

- automatised triangular option: the code looks at the matrix and
  decides how close to upper-triangular it is.
