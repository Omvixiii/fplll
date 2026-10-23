# Changelog

## 6.0 (2026-10-23)

fplll 6.0 requires C++23 and a rebuild against SONAME 10.  It removes the
MPIR, DPE and `ZZ_mat<double>` backends and the recursive-enumeration switch,
makes the library silent by default, modernizes the option enums and strategy
API, and replaces the generated pruner tables with runtime computation.  See
[the 6.0 migration guide](doc/migration-6.0.md).

### Build and development

- Use Meson 1.8 or newer with Ninja as the sole build toolchain; remove the
  intermediate CMake build and the legacy Autotools files.
- Require C++23 for the library, installed headers, tools, tests and
  benchmarks.
- Remove the MPIR backend, the recursive-enumeration build switch and the
  `ZZ_mat<double>` integer backend.
- Replace build presets with Meson options and independent build directories.
- Generate configuration headers and enumeration sources in the build tree;
  keep libraries, executables and test working directories there as well.
- Provide shared and static libraries (`both` by default), optional debug and
  variant libraries, pkg-config metadata and `meson dist` source archives.
- Remove the CMake package config and exported targets; downstream projects
  use pkg-config or Meson's `dependency('fplll')`.
- Replace CPack source archives and the distcheck workflow with `meson dist`,
  which builds and tests the generated archive.
- Migrate CI, container builds and contributor instructions to Meson.
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
