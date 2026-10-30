<!-- SPDX-FileCopyrightText: 2026 Omvixiii
     SPDX-License-Identifier: LGPL-2.1-or-later -->

# Changelog

All notable user-visible changes to fplll since 5.5.0 are recorded in this
file.

## 6.0.0 (2026-10-30)

fplll 6.0.0 is a major release. It requires a C++23 compiler and standard
library, moves the build system to Meson, and removes the compatibility layers
that the old API carried. Consumers must be rebuilt against the new shared
library (`libfplll.so.10` / `libfplll.10.dylib`, library version 10.0.0).

### Upgrading from 5.x

- Build with Meson 1.8.0 or newer and Ninja; the Autotools files are gone.
- Compile the library, installed headers, tools and tests in C++23 mode with a
  C++23 standard library. pkg-config does not carry the language standard, so
  consumers must select C++23 themselves.
- libqd is no longer auto-detected: 5.x enabled QD support whenever libqd was
  found (`configure` acted as if `--with-qd` was passed by default), while 6.0
  disables it unless configured with `-Dqd=enabled`.
- Include `<fplll/fplll.h>` instead of `<fplll.h>`. The component headers moved
  into module directories: `<fplll/gso/...>`, `<fplll/reduction/...>`,
  `<fplll/enum/...>`, `<fplll/pruner/...>`, `<fplll/nr/...>`.
- Two installed headers changed extension: `fplll/nr/matrix.cpp` is now
  `fplll/nr/matrix.inl`, and `fplll/pruner/pruner_simplex.h` is now
  `fplll/pruner/pruner_simplex.inl`, matching the `.inl` convention of the
  other implementation headers.
- Reduction entry points return `fplll::ReductionReport` instead of an `int`
  error code.  The report carries `status` plus the backend and precision that
  were actually used, whether the proved guarantee was achieved (`guaranteed`)
  and, for BKZ/HKZ, the number of completed tours and visited nodes.  Use
  `fplll::succeeded(report)`, `fplll::to_string(report)` and `report.status`;
  the `RED_*` constants, `RED_STATUS_STR` and `get_red_status_str()` are gone.
- The GSO row-operation protocol is RAII: replace pairs of
  `gso.row_op_begin(first, last)` / `gso.row_op_end(first, last)` with
  `auto batch = gso.row_operations(first, last);`; the cache is invalidated when
  the guard is destroyed.  The dimension and the row exponents are read-only
  through `get_d()`, `get_row_expo(i)` and `is_enable_row_expo()`; modifying the
  basis directly is not supported.  `GSOOptions` and `HouseholderOptions` take
  a `precision` field: a positive value is installed for the lifetime of the
  object and restored afterwards (MPFR backends only).
- Configuration is passed with option structs and designated initializers
  (`LLLOptions`, `HLLLOptions`, `BKZParam`, `HKZOptions`, `SVPOptions`,
  `CVPOptions`, `PrunerOptions`, `GSOOptions`, `HouseholderOptions`). The
  integer flag bit-sets (`LLL_*`, `BKZ_*`, `HKZ_*`, `SVP_*`, `CVP_*`,
  `PRUNER_*`, `GSO_*`, `HOUSEHOLDER_*`) are gone.
- `BKZParam::max_time` is expressed as `std::optional<std::chrono::milliseconds>`
  (process CPU time) instead of an `optional<double>` holding seconds; the
  `--bkz-max-time` option of the tools still takes seconds.
- `Matrix::operator[]` on a `const` matrix returns a read-only
  `ConstMatrixRow`; `MatrixRow` can only be constructed from a mutable matrix.
  Code that needs a writable row must have non-const access (the previous
  `const_cast`-based view let a `const Matrix &` be written through).  The row
  views use C++23 deducing this, so they need Clang 19+ or GCC 14+.
- `closest_vector()` requires the target to have exactly as many coordinates
  as the basis has columns and rejects any other length with `fplll::Error`;
  `sol_coord` is cleared when the solver fails.
- `Pruner`, `prune()` and `svp_probability()` require a lattice dimension of
  at least 2 (smaller dimensions used to index out of bounds).
- `EnumerationBase::save_rounding()`, `restore_rounding()` and
  `rounding_backup` were removed; sequential enumeration manages the rounding
  mode with the public RAII guard `fplll::ScopedRoundingMode`.  `sym_g()` on
  `MatGSO`/`MatGSOGram` is no longer part of the public surface; use
  `get_g_matrix()`/`symmetrize_g()`, which validate their preconditions.
- `FloatType`, `LLLMethod`, `SVPMethod`, `CVPMethod`, `PrunerMetric`,
  `EvaluatorMode` and `EvaluatorStrategy` are scoped enumerations with
  lower-case enumerators (`FloatType::double_precision`, `LLLMethod::proved`,
  …); the old upper-case spellings (`FT_DOUBLE`, `LM_PROVED`, …) no longer
  exist. `IntType`, the dedicated `long` reduction entry points and the
  `FPLLL_WITH_ZLONG` configuration were removed; the public integer type is
  `ZZ_mat<mpz_t>` and the `long` fast path is an implementation detail of LLL.
- Library errors throw `fplll::Error` (derived from `std::runtime_error`)
  instead of aborting: `FPLLL_CHECK(condition, "message {}", args…)` and
  `FPLLL_ABORT` report the failing expression with its source location.
  `FPLLL_DEBUG_CHECK` remains an abort for internal invariants.
- Control the parallel enumerator only through `fplll::get_threads()` and
  `fplll::set_threads()`. The external-enumerator plugin
  (`set_external_enumerator`, `get_external_enumerator`, `ExternalEnumeration`,
  the `enumerate_ext*` headers and the `fplll_extenum` hook) was removed;
  the built-in parallel enumerator is the only backend.
- The `long double` floating-point backend is gone (`FT_LONG_DOUBLE`,
  `-f longdouble`, `FPLLL_WITH_LONG_DOUBLE`). Use `double`, `dd`, `qd` or
  `mpfr`; DD/QD support still comes from `-Dqd=enabled`.
- The legacy DPE backend is gone as well (`FT_DPE`, `-f dpe`, `nr/dpe.h`,
  `nr/nr_FP_dpe.inl`, `FPLLL_WITH_DPE`), together with the `Z_NR<double>`
  specialization (`nr/nr_Z_d.inl`, `FPLLL_WITH_ZDOUBLE`). Heuristic LLL runs in
  `double` with GSO row-exponent tracking; escalating to MPFR when more
  precision is needed is part of the self-tuning `wrapper` method.
- `Wrapper`, `BKZReduction`, `BKZAutoAbort`, `LLLReduction`, `HLLLReduction`,
  `is_lll_reduced()` and `is_hlll_reduced()` are implementation details and no
  longer installed. Use the free functions `lll_reduction`, `hlll_reduction`,
  `bkz_reduction`, `hkz_reduction`, and `shortest_vector`/`closest_vector`.
- `BKZParam` is a plain aggregate: it holds its strategy table by value, has
  no throwing constructor and no `flags` member. `load_strategies_json()` is
  the only strategy-loading entry point; bare file names are resolved against
  the installed strategy directory. The `default_strategy()`,
  `default_strategy_path()` and `strategy_full_path()` helpers were removed.
- The GSO dump feature (`BKZ_DUMP_GSO`, `-bkzdumpgso`, `BKZReduction::dump_gso`,
  `dump_gso_filename`, the `tools/plot_gso_dump.py` helper) was removed.
  Inspect `MatGSOInterface::get_mu_matrix()`/`get_r_matrix()` instead.
- `MatrixPrintMode` is a scoped enumeration (`MatrixPrintMode::compact`,
  `::regular`) and `Matrix::set_print_mode` replaces the `MAT_PRINT_*` integer
  constants.
- The tools use long option names (`--block-size`, `--bkz-max-loops`, …) and
  keep the historical short forms where they are unambiguous. `-z` (integer
  type), `-r`/`-delta`/`-eta`/`-theta` aliases, `-l` (Lovász flag) and
  `-bkzdumpgso` were removed; `--siegel` selects Siegel's condition, and
  `--strategies` is the long form of `-s`. See `fplll --help`.

### Changed

- The build defaults to Meson's `debug` build type and a shared library, and
  the test suite is off by default, so a plain `meson compile` builds only the
  library and the tools and never resolves Catch2.  `-Dtests=true` adds the
  test suite, `-Ddebug_checks=true` compiles internal invariant checks into the
  regular library/tools/tests, and `-Dqd=enabled` enables the DD/QD backends.
- Release builds define `NDEBUG` (`b_ndebug=if-release`), so the standard
  `assert` is compiled out as in a distribution build; the library's own
  invariant checks stay behind the independent `-Ddebug_checks=true` option.
- The integer random source is a mutex-protected `std::mt19937_64`;
  `RandGen::init_with_seed` seeds it together with the GMP state, so a seeded
  `latticegen` run is reproducible across platforms. CPU timings use a portable
  clock instead of POSIX `cputime()`.
- The Meson build takes the library and test target names, the strategies
  install directory and the Doxygen project name from
  `meson.project_name()`; the redundant `fplll_soversion` and
  `fplll_darwin_versions` variables are gone because Meson derives both from
  the library version.  `max_parallel_enum_dim` is limited to `0` or a multiple
  of 10 from `20` to `160`, matching the generated dispatch units.
- `-Dmax_parallel_enum_dim` is an integer option (a multiple of 10, `0`
  disables parallel enumeration) instead of a fixed choice list, and the
  generated per-dimension translation units follow it.
- The parallel enumerator runs on an internal `std::jthread` pool that
  replaces the vendored thread pool. `get_threads()` and `set_threads()` keep
  their semantics (initial 1, `-1` → hardware threads, values below 1 → 1,
  lazy pool creation, exclusive enumeration sessions).
- `Enumeration` no longer takes the subtree/subtree-reset parameters (no
  production caller, only the removed external-enumeration protocol used
  them). Enumeration node counters are stored per level and exposed through a
  span; the fixed 1024-entry counter array and its per-call copy are gone.
- Enumeration scratch space is allocated per enumeration with the actual
  dimension instead of fixed 256×256 arrays; an `EnumerationBase` object no
  longer costs about 1 MiB regardless of dimension.
- The pruner's factorial and unit-ball-volume tables are thread-safe and
  bounds-checked (`std::call_once`), keep their values, and no longer leak a
  `PRUNER_MAX_N` macro into the public headers.
- Status traces (`log_stream()`) are silent by default; install a stream with
  `fplll::set_log_stream()` (the tools do this for verbose runs and for the
  pruner).
- LLL, HLLL and BKZ report the floating-point backend and precision they
  actually used, and LLL/HLLL set `guaranteed` when the proved variant reached
  the required precision.  BKZ and HKZ report `tours`/`nodes` and always set
  `guaranteed` to false because they do not provide a certificate.
- The GSO owns the consistency of its caches: row operations must run inside a
  `row_operations()` batch, and the read-only views returned by
  `get_mu_matrix()`/`get_r_matrix()`/`get_mu_exp()`/`get_r_exp()` are
  invalidated by the next row operation or GSO update.  The MPFR working
  precision can be passed to `GSOOptions`/`HouseholderOptions` instead of
  setting the global default before constructing the object.
- Reduction entry points validate their options and selected backend before
  modifying any basis or transformation matrix: a rejected configuration
  throws `fplll::Error` and leaves the caller's data untouched.  `BKZParam`
  additionally validates `delta` (in `(0.25, 1]`) and rejects SD-BKZ block
  sizes larger than the number of non-zero rows when the basis actually needs
  a reduction.  A non-zero `precision` now
  selects MPFR for BKZ as well (also with `FloatType::automatic`), instead of
  being silently ignored in favour of double precision.
- The opt-in test suite (`-Dtests=true`) is a single Catch2 v3 executable
  registered once per module tag, built on demand; `meson test -C build-tests lll`
  reruns one module.  A tests-enabled configuration also compiles the library
  with the internal test hooks, so it is kept in a separate build directory.
- Documentation is single-sourced: `README.md` (user-facing requirements,
  build, tools, library and citation), `CONTRIBUTING.md` (development build,
  layout, conventions), `CHANGELOG.md` and the Doxygen API reference.
- The BKZ strategy table remains installed under `share/fplll/strategies`
  (8.6 MB, 101 block sizes) and is selected explicitly, e.g.
  `-s default.json` or `load_strategies_json("default.json")`.
- The license texts (`LICENSE`, `LICENSES/LGPL-2.1-or-later.txt` and
  `LICENSES/MIT.txt`) are installed under `share/doc/fplll`. The tree is
  REUSE-compliant: files that cannot carry a header are covered by
  `REUSE.toml`; `reuse lint` checks the result.
- `meson dist` produces source archives; `--include-subprojects` embeds the
  wrapped dependencies in the archive, and `--no-tests` skips building and
  testing the archive.
- The pruner's factorial and ball-volume constants are compile-time tables for
  the `double` backend (5.5.0 shipped them as decimal-string literals parsed at
  first construction); the non-double backends fill their tables once on first
  use, with the `mpfr` backend converting the factorials directly from `mpz`
  (dd/qd keep the decimal-string path). The values are unchanged bit for bit.
  The generator that produced the ball-volume literals ships in
  `scripts/gen_pruner_ball_volumes.cpp` so the table can be reproduced.
- `PruningParams` is declared in the new lightweight installed header
  `<fplll/pruner/pruning_params.h>`; `<fplll/pruner/pruner.h>` re-exports it, so
  code that includes the pruner header keeps working unchanged.
- The floating-point backends are dispatched from one internal helper, so a new
  backend is a compile-time error until it is handled there, and the reduction
  entry points run their backend under the same guards.
- The command-line tools derive their enumeration parsing from one table per
  enum, and `--bkz-max-time` is stored as a `std::chrono::milliseconds` value
  from the moment it is parsed.
- Public and internal headers include what they use directly; the aggregate
  headers (`<fplll/fplll.h>`, `<fplll/nr/nr.h>`) own their re-exports and the
  tree is kept free of clangd include-cleaner diagnostics.  The Meson build
  and the source layout are documented in `CONTRIBUTING.md`.

### Removed

- The external enumeration plugin: `enumerate_ext.h`, `enumerate_ext_api.h`,
  `enumerate_ext.cpp`, the `extenum_*` global typedefs,
  `FPLLL_EXTENUM_MAX_EXTENUM_DIM`, `FPLLL_EXTENUM_FUNC`, and the
  `set/get_external_enumerator` API.
- The `long double` backend (`nr_FP_ld.inl`, `FP_NR<long double>`), the
  `FPLLL_WITH_LONG_DOUBLE` configuration and the `-f longdouble` tool option.
- The DPE backend (`nr/dpe.h`, `nr/nr_FP_dpe.inl`, `FT_DPE`, `-f dpe`,
  `FPLLL_WITH_DPE`) and the `Z_NR<double>` specialization (`nr/nr_Z_d.inl`,
  `FPLLL_WITH_ZDOUBLE`).
- The dedicated `long` reduction entry points (`FPLLL_DECLARE_LLL(long)`,
  `FPLLL_DECLARE_HLLL(long)`), `IntType`, the `-z` tool option and the
  `FPLLL_WITH_ZLONG` configuration macro.
- The `FPLLL_BEGIN_NAMESPACE`/`FPLLL_END_NAMESPACE` macros, the
  `using std::…` re-export block in `defs.h`, and `FPLLL_INFO`; code uses plain
  `namespace fplll` and qualified standard names.
- The `FPLLL_DECLARE_LLL`/`FPLLL_DECLARE_HLLL`/`FPLLL_DEFINE_LLL`/
  `FPLLL_DEFINE_HLLL` macros, the external `Wrapper` header, and the
  `FPLLL_DEFAULT_STRATEGY_PATH`/`FPLLL_DEFAULT_STRATEGY` fallbacks.
- The GSO dump feature listed above, `tools/plot_gso_dump.py`, the
  `llldiff` development helper, the raw-pointer `dump_mu_d`/`dump_r_d`
  overloads, and the unused `print_mu_r_g`/`get_max_gram`/`get_max_bstar`/
  `apply_transform`/`to_canonical` GSO surface.
- The dead `LLLFlags`/`BKZFlags`/`HKZFlags`/`SVPFlags`/`CVPFlags`/
  `PrunerFlags`/`MatGSOInterfaceFlags`/`MatHouseholderFlags` enums, the
  `FLOAT_TYPE_STR`/`INT_TYPE_STR`/`LLL_METHOD_STR`/`HLLL_METHOD_STR` lookup
  tables, and `enumxt`.
- `is_lll_reduced`-adjacent legacy glue, the `Cygwin`/`Sun`/`NAN` platform
  workarounds, and the `NOCOUNTS` enumeration build switch.
- The vendored thread pool (`fplll/io/thread_pool.hpp`).

### Fixed

- Invalid library input is rejected with `fplll::Error` instead of aborting or
  producing undefined results: out-of-range LLL/HLLL parameters, invalid BKZ
  block sizes and strategy tables.  Options are checked before the reduction
  writes its output matrices, so a rejected configuration cannot leave a
  half-updated transform or preprocessed basis behind.  The tools reject
  malformed command-line values and unknown output items with a diagnostic.
- `shortest_vectors()` fills `sol_coord`/`sol_dist` for every positive
  `max_sols` (including `max_sols == 1`); a successful status always comes with
  one result per output, and the outputs are emptied on failure.
- An empty basis is a successful no-op: LLL and HLLL short-circuit before the
  `log2(0)` iteration bound, the report uses `automatic` with precision 0, and
  the proved LLL reports `guaranteed = true` for the trivial run; BKZ and HKZ
  report `tours = nodes = 0` and `guaranteed = false`. The LLL iteration bound
  validates `delta`/`eta` before dividing by `log(delta)`.
- Printing a basis with no rows or no columns in the comma output format
  (`-of bk`) no longer indexes out of bounds: `[]` and `[[]]` print as `[]` and
  `[[]]` instead of crashing.
- The parallel enumeration session joins every accepted worker and rethrows the
  first error; a second enumeration waits instead of taking turns inside the
  shared pool, and reentrant calls from enumeration callbacks throw
  `std::logic_error` instead of deadlocking.
- Parallel enumeration installs round-to-nearest on the calling thread and on
  every worker, restores the caller's rounding mode when the evaluator throws,
  and rejects `max_indices` combined with dual enumeration before modifying any
  state.
- The enumeration node counter is 64-bit and no longer copied as a fixed
  1024-entry array on every call.
- `load_strategies_json()` validates the JSON schema strictly and reports the
  searched paths when the file is missing.
- HKZ no longer trips an internal debug check when the enumeration returns the
  basis vector already at the target position (HKZ enumerates with an exact
  radius, `delta = 1`); such a solution stays a no-op, and
  `-Ddebug_checks=true` builds now run the HKZ path.
- BKZ and HKZ with an explicit MPFR backend and `precision = 0` use the
  process default MPFR precision instead of failing, and HKZ validates its
  options before returning early for a trivial basis.
- The tools no longer print a doubled `fplll:` prefix when a library error
  reaches the top-level handler.
- Sequential enumeration restores the caller's floating-point rounding mode
  even when the evaluator throws; the previous paired save/restore calls
  leaked the mode on the exception path.
- In `-Ddebug_checks=true` builds the GSO mutating row operations abort unless
  they run inside a `row_operations()` batch, and the MPFR precision guards
  restore the previous process default in any destruction order, not only
  LIFO.
- `--bkz-max-time` rejects values above 1e9 seconds (about 31.7 years) with a
  clear error instead of silently saturating when it is converted to
  milliseconds; non-finite and negative values are rejected as well.
- `closest_vector()` validates the target dimension instead of reading out of
  bounds, and clears its output instead of returning intermediate Babai
  coordinates when the enumeration fails.
- The `fplll` tool rejects an explicit but empty CVP target with a diagnostic
  instead of silently running the SVP entry point.
- One-dimensional `closest_vector()` now succeeds: the enumeration radius of a
  one-row basis is no longer degenerate, so the closest lattice vector is found
  instead of reporting an enumeration failure.
- `Pruner` rejects dimensions below 2, where it previously indexed out of
  bounds.
- Verbose BKZ logging no longer leaves the shared `log_stream()` in a
  `fixed`/`precision` state after the run.
- The pruner validates its basis dimension before touching its tabulated
  constants, and its lazy table initialization is thread-safe.
- `vector_matrix_product()` bounds-checks its inputs; `zeros_first()` and
  `zeros_last()` have corrected documentation.
- The BKZ strategy loader no longer mutates its caller's strategy vector
  through a reference member.
- Command-line floating-point options are parsed with `std::from_chars`,
  which is locale-independent and rejects trailing characters; the option
  validation rejects non-finite and out-of-range values.
