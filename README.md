# fplll

[![CMake build](https://github.com/fplll/fplll/actions/workflows/cmake.yml/badge.svg)](https://github.com/fplll/fplll/actions/workflows/cmake.yml)
[![Codecov](https://codecov.io/gh/fplll/fplll/branch/master/graph/badge.svg)](https://codecov.io/gh/fplll/fplll)

fplll is a library and command-line toolkit for lattice reduction. It provides
floating-point LLL, BKZ (including BKZ 2.0), Slide reduction, self-dual BKZ,
shortest-vector enumeration, and closest-vector enumeration. See the
[usage guide](doc/usage.md) for algorithm options and references.

## Build from source

Install CMake 3.21 or newer, Ninja, ccache, a C++11-capable compiler, GMP, and
MPFR. QD is optional. All presets use Ninja and ccache; CMake discovers the
C++ compiler automatically.

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Use `release` for optimized builds or `relwithdebinfo` for optimized builds
with debug symbols. Use `asan` or `ubsan` for address or undefined behavior
checks with GCC or Clang and the corresponding sanitizer runtime. Both inherit
the Debug configuration.

Each preset keeps its build output under `build/<preset>/`,
with executables in `bin/<CONFIG>/` (for example, `build/debug/bin/DEBUG/`).
Installation defaults to `_install/<preset>/` inside the checkout:

```sh
cmake --preset release
cmake --build --preset release
ctest --preset release
cmake --install build/release
```

Put local overrides, such as compiler selection, dependency paths, and build
parallelism, in an untracked `CMakeUserPresets.json` that inherits the project
presets.

CMake determines development versions from Git tags automatically. Release CI
supplies the version through `-DFPLLL_VERSION=X.Y.Z`; generated source archives
retain it without Git. No version file needs to be edited before building.

The [build guide](doc/building.md) covers optional features, installation,
downstream CMake and pkg-config use, and source archives.

## Command-line tools

The build provides `fplll` for lattice reduction and `latticegen` for generating
example lattices. Matrix rows are basis vectors. For example:

```sh
build/debug/bin/DEBUG/latticegen r 10 1000 |
  build/debug/bin/DEBUG/fplll -a lll
```

### latticegen

See the [generator modes and seed options](doc/usage.md#latticegen).

### Reduction command

See the [algorithm, arithmetic, and output options](doc/usage.md#fplll-command)
and [worked examples](doc/usage.md#examples).

## Library and API documentation

See the [library usage notes](doc/usage.md#how-to-use-as-a-library) and
[API documentation](https://fplll.github.io/fplll/annotated.html). With Doxygen
installed when configuring CMake, the `doc` target generates local API
reference pages. Its configuration is explained in the
[build guide](doc/building.md#development-utilities).

## Citation

```bibtex
@unpublished{fplll,
  author = {The {FPLLL} development team},
  title = {{fplll}, a lattice reduction library, {Version}: 5.5.0},
  year = 2023,
  note = {Available at \url{https://github.com/fplll/fplll}},
  url = {https://github.com/fplll/fplll}
}
```

## License and project information

fplll is licensed under the [GNU Lesser General Public License, version 2.1 or
later](COPYING). The full text is retained as `COPYING`, the conventional GNU
license filename also referenced by source files.

Changes are recorded in [CHANGELOG.md](CHANGELOG.md); contributor guidance is
in [CONTRIBUTING.md](CONTRIBUTING.md). For bug reports and feature requests,
use [GitHub Issues](https://github.com/fplll/fplll/issues).

Historical credits and acknowledgments are in the
[usage guide](doc/usage.md#credit).
