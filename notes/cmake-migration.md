# Autotools to CMake feature map

The CMake build is an independent build system. It does not run or include Autoconf, Automake, Libtool, or `m4/` macros. Both build systems remain in the source tree while the migration is reviewed.

| Existing Autotools behavior | CMake implementation | Status |
| --- | --- | --- |
| GMP/MPIR, MPFR, pthread and atomic detection in `configure.ac` | Imported dependency targets and compile/link checks in `CMakeLists.txt`; `FPLLL_USE_MPIR`, `FPLLL_MPIR_ROOT` | Implemented; MPIR is unavailable in the local environment |
| Optional QD support | `FPLLL_WITH_QD`, `QD::QD`, generated `FPLLL_WITH_QD` macro | Implemented; QD preset build and tests passed |
| Recursive enumeration, max enum dimensions, parallel enum sources | `FPLLL_WITH_RECURSIVE_ENUM`, `FPLLL_MAX_ENUM_DIM`, `FPLLL_MAX_PARALLEL_ENUM_DIM` | Implemented; parallel sources are selected from the configured maximum |
| External enumeration integration | `FPLLL_EXTENUM_DIR`, `FPLLL_EXTENUM_LIBRARY`, `FPLLL_EXTENUM_FUNCTION` and configure-time link check | Implemented; external library is unavailable locally |
| `fplll_config.h` and `config.h` generation | `cmake/fplll_config.h.in`, `cmake/config.h.in`, `configure_file()` under the Binary Directory | Implemented |
| `libfplll`, `fplll`, `latticegen`, check helper `llldiff` | Shared/static library targets, CLI targets and test-only `llldiff` | Implemented |
| Optional `libfplllv`, `libfpllld`, `fplll_dbg`, `latticegen_dbg`, `dbg` target | Opt-in variant/debug targets using shared OBJECT libraries | Implemented and locally built |
| 15 `make check` tests and `TESTDATADIR` | CTest targets with source fixture paths and per-test Binary Directory working folders | Implemented; GCC and Clang each passed 15/15 |
| Libtool version, public headers, strategy data, pkg-config and downstream linking | `VERSION 9.0.0`, `SOVERSION 9`, install rules, `fplll.pc`, exported CMake package | Implemented; shared/static consumers passed |
| Compiler and build configuration | CMake Presets for GCC/Clang, Debug/Release, feature variants, sanitizers and coverage; CMake/Conda CI | Implemented |

## Build layout

`CMakePresets.json` fixes each Ninja binary directory under `build/<preset>` and each install prefix under `_install/<preset>`. `CMakeLists.txt` rejects in-source builds. Generated configuration headers and umbrella header stay in the binary directory; executables, libraries, tests and test-generated files use managed binary-tree directories. The BKZ dump test writes into its CTest working directory.

The common library object files are compiled once and linked into both shared and static libraries. Debug objects are built only when a debug variant target is requested.

## Local verification limits

The local host does not have MPIR, an external enumeration library, or Conda. Those optional dependency/package paths are represented in CMake/CI but were not locally exercised. The historical Autotools reference uses a separate worktree at the Phase 1 source commit; its generated files remain inside that worktree.
