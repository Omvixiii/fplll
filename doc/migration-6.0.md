# Migrating from fplll 5.x to 6.0

fplll 6.0 is a major release. It requires a modern toolchain and build system,
removes several historical backends and build options, and changes parts of the
public API. This guide lists the changes that need action when upgrading.

## Toolchain and library version

- C++23 is required for the library, installed headers, command-line tools
  and tests. The validated floor is GCC 14, Clang 18 (libstdc++ 14 or
  libc++ 18) and AppleClang 16.
- The shared-library SONAME is now 10; the library version is 10.0.0 and the
  macOS compatibility/current version is 11.0.0. Rebuild all consumers against
  the installed library.
- The build system is Meson 1.11 or newer with Ninja. The intermediate CMake
  interface is removed; downstream projects discover fplll through pkg-config
  or Meson's `dependency('fplll')` instead of a CMake package config.

## Build system: Autotools -> CMake -> Meson

The upstream build system history is:

| Stage | Build system |
| --- | --- |
| fplll 5.x | Autotools |
| 6.0 development (intermediate) | CMake |
| fplll 6.0 | Meson |

The CMake interface was an intermediate port during 6.0 development. It is
**historical and removed**; the items below are listed only to support
upgrades and must not be used for new work.

### Removed CMake interface (historical)

The following CMake items no longer exist in fplll 6.0:

- CMake presets, workflows and preset names (`debug`, `release`, `asan`,
  `ubsan`, `warnings`, `tsan`).
- CMake cache options (`-DFPLLL_*`, `BUILD_TESTING`, `CMAKE_BUILD_TYPE`,
  `CMAKE_INSTALL_PREFIX`, `BUILD_SHARED_LIBS`, ...).
- The installed CMake package config and targets:
  `find_package(fplll 6.0 CONFIG REQUIRED)`, `fplll::fplll` and
  `fplll::fplll_static`.
- CPack source archives and the `distcheck` workflow.
- CMake-based formatting and lint scripts, and the CMake-generated Doxygen
  configuration.

Downstream CMake projects must switch to pkg-config, for example:

```cmake
find_package(PkgConfig REQUIRED)
pkg_check_modules(FPLLL REQUIRED IMPORTED_TARGET gmp mpfr fplll)
target_link_libraries(my_program PRIVATE PkgConfig::FPLLL)
```

A Meson consumer is direct:

```meson
project('consumer', 'cpp', default_options: ['cpp_std=c++23'])
executable('consumer', 'main.cpp', dependencies: dependency('fplll'))
```

pkg-config does not carry the C++ standard: consumers that compile fplll
headers directly must select C++23 themselves.

### Old CMake options and equivalent Meson options (historical)

| Removed CMake option | Meson 6.0 equivalent |
| --- | --- |
| `-DBUILD_TESTING=OFF` | `-Dtests=false` |
| `-DFPLLL_WITH_QD=ON` | `-Dqd=enabled` (or `auto`) |
| `-DFPLLL_MAX_ENUM_DIM=N` | `-Dmax_enum_dim=N` |
| `-DFPLLL_MAX_PARALLEL_ENUM_DIM=N` | `-Dmax_parallel_enum_dim=N` |
| `-DFPLLL_EXTENUM_DIR/LIBRARY/FUNCTION=...` | no option; provide the `fplll_extenum` dependency (pkg-config or wrap) whose library exports `fplll_extenum_enumerate` |
| `-DFPLLL_WARNINGS=ON`, `-DFPLLL_WARNINGS_AS_ERRORS=ON` | `-Dwarning_level=3 -Dwerror=true` |
| `-DCMAKE_BUILD_TYPE=Release` | `--buildtype=release` |
| `-DCMAKE_INSTALL_PREFIX=...` | `--prefix ...` |
| `-DBUILD_SHARED_LIBS=ON` | `--default-library=shared` (default `both`) |
| `-DCMAKE_CXX_COMPILER_LAUNCHER=ccache` | automatic: Meson uses ccache or sccache when installed |
| `-DCMAKE_CXX_FLAGS=...` | `-Dcpp_args=...` |
| `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` | always written in the build directory (`build/compile_commands.json` for the developer build) |
| `find_package(fplll CONFIG)` | pkg-config or `dependency('fplll')` |
| `cmake --build ... --target package_source`, `distcheck` | `meson dist` |
| `cmake --build ... --target format`, `lint` | removed with the Meson port; run `clang-format` (and `clang-tidy`) directly |
| `cmake --build ... --target doc` | `meson compile -C build doc` |
| `cmake --install ...` | `meson install -C build` |
| `cmake --build ... --target uninstall` | `ninja -C build uninstall` |

The removed presets map to Meson options in separate build directories:
`debug` to `--buildtype=debug`, `release` to `--buildtype=release`, `asan` to
`-Db_sanitize=address,undefined -Db_lundef=false`, and `warnings` to
`-Dwarning_level=3 -Dwerror=true` (advisory until the pre-existing warning
baseline is cleaned up).

See the [build guide](building.md) for the full Meson command set.

## Removed backends and build options

- **MPIR** is gone. GMP is the only integer backend; `FPLLL_USE_MPIR` and
  `FPLLL_MPIR_ROOT` no longer exist.
- **DPE** is gone. `FT_DPE`, `-f dpe`, `FPLLL_WITH_DPE` and the DPE headers
  were removed. Heuristic LLL now uses `double` with GSO row-exponent tracking
  and falls back to MPFR when more precision is needed. The default wrapper
  path is unchanged.
- **`ZZ_mat<double>` (`ZT_DOUBLE`, `-z double`)** was removed. Use `long`
  (`-z long`) or `mpz` (`-z mpz`).
- The **recursive-enumeration switch** (`FPLLL_WITH_RECURSIVE_ENUM`) was
  removed; recursive enumeration is always enabled.
- The unused `enum/topenum` enumerator and the `fplll/fplll.cpp` shim were
  removed; internal implementation-include fragments are named `.inl`.

## Public API changes

- `FloatType`, `IntType`, `LLLMethod`, `SVPMethod`, `CVPMethod` and
  `PrunerMetric` are now `enum class`. Their enumerators are re-exported with
  `using enum`, so names such as `FT_DOUBLE` and `LM_PROVED` keep working, but
  they no longer implicitly convert to integers. Use `static_cast` or
  `fplll::enum_index` where an integer is required.
- Status-returning functions are marked `[[nodiscard]]`. Check the returned
  status instead of discarding it.
- The library is silent by default. Install a sink with
  `fplll::set_log_stream(std::ostream *)` to receive verbose diagnostics;
  `fplll::set_log_stream(nullptr)` restores the default.
- `util.h` no longer provides the POSIX-based `cputime()` helper, and the
  public headers no longer include POSIX headers. Internal CPU timing is based
  on `clock_gettime`/`std::clock`.
- Random generation uses `std::mt19937_64` with a deterministic default seed.
  `RandGen::init_with_seed` seeds both the GMP state and the integer engine;
  `RandGen::init_with_time` uses `std::random_device` and a high-resolution
  clock. `RandGenInt` is thread-safe.
- Command-line numeric options and `Z_NR<long>`/`FP_NR<double>` string parsing
  use `std::from_chars` and reject malformed values instead of silently
  accepting them. Double overflow and underflow retain their IEEE infinity
  and signed-zero results through a locale-independent MPFR fallback.
- Pruner contexts are computed at first use with GMP/MPFR instead of being
  parsed from generated 4,100-line tables; numeric behaviour and
  `PRUNER_MAX_N` are unchanged.
- `get_red_status_str` and the status/method/type string tables are backed by
  `std::string_view` arrays with corrected sizes.
- `strategy_full_path` returns `std::optional<std::string>`;
  `load_strategies_json` and `BKZReduction::dump_gso` accept `std::string_view`
  arguments.

## Migration checklist

1. Move to Meson 1.11 or newer with Ninja and a C++23 compiler; replace the
   removed CMake options using the mapping above.
2. Replace removed command-line switches (`-f dpe`, `-z double`).
3. Rebuild against SONAME 10 and switch downstream discovery to pkg-config or
   `dependency('fplll')`; set C++23 for consumers that compile directly.
4. Install a log stream if your application relied on verbose output.
5. Update code that relied on implicit enum-to-int conversions or ignored
   `[[nodiscard]]` statuses.
6. Re-run your numerical quality and performance tests; the heuristic default
   now uses double and is substantially faster at the same quality.

Historical release notes are in [CHANGELOG.md](../CHANGELOG.md).
