# fplll

fplll is a library and command-line toolkit for lattice reduction. It provides
floating-point LLL, BKZ (including BKZ 2.0), Slide reduction, self-dual BKZ,
shortest-vector enumeration, and closest-vector enumeration. See the
[usage guide](doc/usage.md) for algorithm options and references.

## Build from source

Install CMake 3.25 or newer, Ninja, a C++23-capable compiler, GMP 4.2 or
newer, and MPFR 2.3 or newer. Choose the final installation prefix when
configuring, then build, test, and install:

```sh
cmake -S . -B build/install -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build/install --parallel
ctest --test-dir build/install --output-on-failure --no-tests=error
cmake --install build/install
```

The checked-in presets configure Ninja builds and run the Debug workflow, which
configures, builds, and runs the tests in order:

```sh
cmake --workflow --preset debug
```

Executables are in `build/debug/bin/`. Use the `release` configure, build, and
test presets for an optimized build; the optional `ubsan` preset supports
diagnosis. No user preset or ccache installation is required.

The [build guide](doc/building.md) covers optional features, installation,
downstream CMake and pkg-config use, and source archives.

## Command-line tools

The build provides `fplll` for lattice reduction and `latticegen` for generating
example lattices. Matrix rows are basis vectors. For example:

```sh
build/debug/bin/latticegen r 10 1000 |
  build/debug/bin/fplll -a lll
```

### latticegen

See the [generator modes and seed options](doc/usage.md#latticegen).

### Reduction command

See the [algorithm, arithmetic, and output options](doc/usage.md#fplll-command)
and [worked examples](doc/usage.md#examples).

## Library and API documentation

See the [library usage notes](doc/usage.md#how-to-use-as-a-library) and
[API documentation](https://fplll.github.io/fplll/annotated.html). With Doxygen
1.9.8 or newer installed when configuring CMake, the `doc` target generates
local API reference pages. Its configuration is explained in the
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
later](LICENSE). The full license text is available in `LICENSE`.

Changes are recorded in [CHANGELOG.md](CHANGELOG.md); contributor guidance is
in [CONTRIBUTING.md](CONTRIBUTING.md). For bug reports and feature requests,
use [GitHub Issues](https://github.com/fplll/fplll/issues).

Historical credits and acknowledgments are in the
[usage guide](doc/usage.md#credit).
