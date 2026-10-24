# fplll

fplll is a library and command-line toolkit for lattice reduction. It provides
floating-point LLL, BKZ (including BKZ 2.0), Slide reduction, self-dual BKZ,
shortest-vector enumeration, and closest-vector enumeration. See the
[usage guide](doc/usage.md) for algorithm options and references.

## Install

fplll is built with Meson and Ninja; Meson is the only build interface. Install
Meson 1.11 or newer, Ninja, a C++23-capable compiler, GMP 4.2 or newer and
MPFR 2.3 or newer, then from the repository root:

```sh
meson setup build
meson install -C build
```

The project defaults to the `release` build type, and the default prefix is
`/usr/local`; on Linux and macOS, installing there may require administrator
rights. Choose a different prefix when configuring, for example
`meson setup build --prefix "$HOME/.local"`. `meson install` builds the project
first; run `meson test -C build --print-errorlogs` beforehand to validate it.

Meson is available with `uv tool install meson` or `pipx install meson`; QD is
optional (`-Dqd=enabled`), and so is an external enumeration library, which is
discovered as the Meson dependency `fplll_extenum` (see
[external enumeration](doc/building.md#external-enumeration)). If a macOS
compiler cannot find SDK headers or libc++, set
`export SDKROOT="$(xcrun --show-sdk-path)"` before configuring. The
[build guide](doc/building.md) covers options, additional configurations,
sanitizers, coverage, source archives, and downstream use through pkg-config or
Meson's `dependency('fplll')`. For development builds, tests and tooling, see
[CONTRIBUTING.md](CONTRIBUTING.md).

## Command-line tools

The build provides `fplll` for lattice reduction and `latticegen` for generating
example lattices; after installation both are on `PATH`, and in a build tree
they are `build/tools/fplll` and `build/tools/latticegen`. Matrix rows are basis
vectors. For example:

```sh
latticegen r 10 1000 | fplll -a lll
```

### latticegen

See the [generator modes and seed options](doc/usage.md#latticegen).

### Reduction command

See the [algorithm, arithmetic, and output options](doc/usage.md#fplll-command)
and [worked examples](doc/usage.md#examples).

## Library and API documentation

See the [library usage notes](doc/usage.md#how-to-use-as-a-library) and
[API documentation](https://fplll.github.io/fplll/annotated.html). With
Doxygen installed when configuring Meson, `meson compile -C build doc`
generates local API reference pages under `build/doc/html/`; see
[api documentation](doc/building.md#api-documentation) in the build guide.

## Citation

```bibtex
@unpublished{fplll,
  author = {The {FPLLL} development team},
  title = {{fplll}, a lattice reduction library, {Version}: 6.0.0},
  year = 2026,
  note = {Available at \url{https://github.com/fplll/fplll}},
  url = {https://github.com/fplll/fplll}
}
```

## License and project information

fplll is licensed under the [GNU Lesser General Public License, version 2.1 or
later](LICENSE). The full license text is available in `LICENSE`.

Upgrading from 5.x is described in the [6.0 migration guide](doc/migration-6.0.md).
Changes are recorded in [CHANGELOG.md](CHANGELOG.md); contributor guidance is
in [CONTRIBUTING.md](CONTRIBUTING.md). For bug reports and feature requests,
use [GitHub Issues](https://github.com/fplll/fplll/issues).

Historical credits and acknowledgments are in the
[usage guide](doc/usage.md#credit).
