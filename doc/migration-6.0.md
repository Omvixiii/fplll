# Migrating from fplll 5.x to 6.0

fplll 6.0 is a major release.  It requires a modern toolchain, removes several
historical backends and changes parts of the public API.  This guide lists the
changes that need action when upgrading.

## Toolchain and soname

- C++23 is required for the library, installed headers, command-line tools,
  tests and benchmarks.  The validated floor is GCC 14, Clang 18
  (libstdc++ 14 or libc++ 18) and AppleClang 16.
- The shared-library SONAME is now 10.  Rebuild all consumers against the
  installed `fplll::fplll` or `fplll::fplll_static` target.
- `find_package(fplll 6.0 CONFIG REQUIRED)` is the supported CMake lookup.

## Removed backends and build options

- **MPIR** is gone.  GMP is the only integer backend; `FPLLL_USE_MPIR` and
  `FPLLL_MPIR_ROOT` no longer exist.
- **DPE** is gone.  `FT_DPE`, `-f dpe`, `FPLLL_WITH_DPE` and the DPE headers
  were removed.  Heuristic LLL now uses `double` with GSO row-exponent
  tracking and falls back to MPFR when more precision is needed.  The default
  wrapper path is unchanged.
- **`ZZ_mat<double>` (`ZT_DOUBLE`, `-z double`)** was removed.  Use `long`
  (`-z long`) or `mpz` (`-z mpz`).
- The **recursive-enumeration switch** (`FPLLL_WITH_RECURSIVE_ENUM`) was
  removed; recursive enumeration is always enabled.
- The unused `enum/topenum` enumerator and the `fplll/fplll.cpp` shim were
  removed; internal implementation-include fragments are named `.inl`.

## Public API changes

- `FloatType`, `IntType`, `LLLMethod`, `SVPMethod`, `CVPMethod` and
  `PrunerMetric` are now `enum class`.  Their enumerators are re-exported with
  `using enum`, so names such as `FT_DOUBLE` and `LM_PROVED` keep working, but
  they no longer implicitly convert to integers.  Use `static_cast` or
  `fplll::enum_index` where an integer is required.
- Status-returning functions are marked `[[nodiscard]]`.  Check the returned
  status instead of discarding it.
- The library is silent by default.  Install a sink with
  `fplll::set_log_stream(std::ostream *)` to receive verbose diagnostics;
  `fplll::set_log_stream(nullptr)` restores the default.
- `util.h` no longer provides the POSIX-based `cputime()` helper, and the
  public headers no longer include POSIX headers.  Internal CPU timing is based
  on `clock_gettime`/`std::clock`.
- Random generation uses `std::mt19937_64` with a deterministic default seed.
  `RandGen::init_with_seed` seeds both the GMP state and the integer engine;
  `RandGen::init_with_time` uses `std::random_device` and a high-resolution
  clock.  `RandGenInt` is thread-safe.
- Command-line numeric options and `Z_NR<long>`/`FP_NR<double>` string parsing
  use `std::from_chars` and reject malformed values instead of silently
  accepting them.
- Pruner contexts are computed at first use with GMP/MPFR instead of being
  parsed from generated 4,100-line tables; numeric behaviour and
  `PRUNER_MAX_N` are unchanged.
- `get_red_status_str` and the status/method/type string tables are backed by
  `std::string_view` arrays with corrected sizes.

## Migration checklist

1. Update the compiler and build flags to C++23.
2. Replace removed CMake options and CLI switches (`FPLLL_USE_MPIR`,
   `-f dpe`, `-z double`, `FPLLL_WITH_RECURSIVE_ENUM`).
3. Rebuild against the new soname; update `find_package(fplll 6.0)`.
4. Install a log stream if your application relied on verbose output.
5. Update code that relied on implicit enum-to-int conversions or ignored
   `[[nodiscard]]` statuses.
6. Re-run your numerical quality and performance tests; the heuristic default
   now uses double and is substantially faster at the same quality.

Historical release notes are in [CHANGELOG.md](../CHANGELOG.md).
