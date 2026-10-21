# fplll

[![CMake build](https://github.com/fplll/fplll/actions/workflows/cmake.yml/badge.svg)](https://github.com/fplll/fplll/actions/workflows/cmake.yml)
[![Codecov](https://codecov.io/gh/fplll/fplll/branch/master/graph/badge.svg)](https://codecov.io/gh/fplll/fplll)

fplll is a library and command-line toolkit for lattice reduction. It provides
floating-point LLL, BKZ (including BKZ 2.0), Slide reduction, self-dual BKZ,
shortest-vector enumeration, and closest-vector enumeration. See the
[usage guide](doc/usage.md) for algorithm options and references.

## Build from source

Install CMake 3.20 or newer, Ninja, ccache, a C++ compiler, GMP, and MPFR.
The GCC debug preset uses C++23; release builds and the default CMake
configuration retain a C++11 compatibility baseline. QD is optional.

```sh
cmake --preset gcc-debug
cmake --build --preset gcc-debug
ctest --preset gcc-debug
```

The commands place executables in `build/gcc-debug/bin/DEBUG/` and keep
libraries, generated files, and test output under `build/gcc-debug/`.
Use `gcc-release` to check the C++11 release configuration. To install that
build into `_install/gcc-release/`:

```sh
cmake --preset gcc-release
cmake --build --preset gcc-release
ctest --preset gcc-release
cmake --install build/gcc-release
```

The [build guide](doc/building.md) covers dependencies, all presets, optional
features, installation, downstream CMake and pkg-config use, and source archives.

## Command-line tools

The build provides `fplll` for lattice reduction and `latticegen` for generating
example lattices. Matrix rows are basis vectors. For example:

```sh
build/gcc-debug/bin/DEBUG/latticegen r 10 1000 |
  build/gcc-debug/bin/DEBUG/fplll -a lll
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
