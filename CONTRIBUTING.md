# Contributing to fplll

fplll welcomes contributions. We encourage users to fix bugs, improve the documentation, write tests and to enhance the code. We encourage researchers to contribute implementations of their algorithms to fplll. In the following we are trying to give some guidance on how to contribute effectively.

## Communication

Communication in the fplll project happens mainly on [GitHub](https://github.com/fplll/fplll/issues) and on our [mailing list](https://groups.google.com/forum/#!forum/fplll-devel).

## Setup

### Fork on GitHub

Login/signup on GitHub and fork fplll from [https://github.com/fplll/fplll](https://github.com/fplll/fplll).

### Clone your fork locally

Now clone your git repo where my-github-name is your account name on GitHub:

```sh
git clone git@github.com:my-github-name/fplll.git
```

Install Meson 1.8 or newer, Ninja, a C++23-capable compiler, GMP and MPFR
development packages, and pkg-config. Install Meson with
`uv tool install meson` or `pipx install meson`. nlohmann/json 3.12 or newer
is required as well; a system package is optional because Meson downloads
the pinned wrap during the first `meson setup` when none is found, which
needs network access once. If a macOS compiler cannot find SDK headers or
libc++, set `export SDKROOT="$(xcrun --show-sdk-path)"` before configuring.
Configure, build, and run the tests:

```sh
meson setup build --buildtype=debugoptimized
meson compile -C build
meson test -C build --print-errorlogs
```

`build/` is the canonical developer build directory, and its
`build/compile_commands.json` feeds clangd and clang-tidy; see
[editor and tooling setup](doc/building.md#editor-and-tooling-setup). Use a
separate `build-<name>/` directory for every other configuration; see the
[build guide](doc/building.md) for options, diagnostics, and output locations.

For an optimized validation build:

```sh
meson setup build-release --buildtype=release
meson compile -C build-release
meson test -C build-release --print-errorlogs
```

A strict-warnings configuration is available as
`-Dwarning_level=3 -Dwerror=true`. It is advisory: it is expected to report
pre-existing diagnostics until the core warning baseline is cleaned up, so do
not use it as a hard submission gate yet.

Changes to installation, packaging, or the build system should also pass
`meson dist -C build-release`; `meson dist` requires a clean Git checkout, so
commit locally before running it. See
[source packages](doc/building.md#source-packages).

## Reporting Bugs

Bug should be filed at [https://github.com/fplll/fplll/issues](https://github.com/fplll/fplll/issues). Alternatively, feel free to contact [https://groups.google.com/forum/#!forum/fplll-devel](https://groups.google.com/forum/#!forum/fplll-devel). The former method is definitely preferred though.

## Setting up topic branches and generating pull requests

While it's handy to provide useful code snippets in an issue, it is better for to submit pull requests.

In Git it is best to isolate each topic or feature into a "topic branch". While individual commits allow you control over how small individual changes are made to the code, branches are a great way to group a set of commits all related to one feature together, or to isolate different efforts when you might be working on multiple topics at the same time.

While it takes some experience to get the right feel about how to break up commits, a topic branch should be limited in scope to a single ``issue`` as submitted to an issue tracker.

Also since GitHub pegs and syncs a pull request to a specific branch, it is the **only** way that you can submit more than one fix at a time. If you submit a pull from your ``master`` branch, you can't make any more commits to your ``master`` branch without those getting added to the pull.

To create a topic branch, its easiest to use the convenient ``-b`` argument to ``git checkout``:

```sh
git checkout -b fix-broken-thing
```

You should use a verbose enough name for your branch so it is clear what it is about. Now you can commit your changes and regularly merge in the upstream develop as described below.

When you are ready to generate a pull request, either for preliminary review, or for consideration of merging into the project you must first push your local topic branch back up to GitHub:

```sh
git push origin fix-broken-thing
```

Now when you go to your fork on GitHub, you will see this branch listed under the "Source" tab where it says "Switch Branches". Go ahead and select your topic branch from this list, and then click the "Pull request" button.

Here you can add a comment about your branch. If this in response to a submitted issue, it is good to put a link to that issue in this initial comment. The maintainers will be notified of your pull request and it will be reviewed (see below for best practices). Note that you can continue to add commits to your topic branch (and push them up to GitHub) either if you see something that needs changing, or in response to a reviewer's comments. If a reviewer asks for changes, you do not need to close the pull and reissue it after making changes. Just make the changes locally, push them to GitHub, then add a comment to the discussion section of the pull request.

## Pull upstream changes into your fork regularly

It is important that you pull upstream changes from master into your fork on a regular basis. Nothing is worse than putting in a days of hard work into a pull request only to have it rejected because it has diverged too far from master.

To pull in upstream changes::

```sh
git remote add upstream https://github.com/fplll/fplll.git
git fetch upstream master
```

Check the log to be sure that you actually want the changes, before merging::

```sh
git log upstream/master
```

Then merge the changes that you fetched::

```sh
git merge upstream/master
```

For more info, see <http://help.github.com/fork-a-repo/>

## How to get your pull request accepted

We want your submission. But we also want to provide a stable experience for our users. In the following, we give some guidelines for contributing.

### Run the tests

Before you submit a pull request, run the test suite:

```sh
meson compile -C build
meson test -C build --print-errorlogs
```

Use an optimized build directory to validate release behaviour. Changes to
installation, packaging, or the build system should also pass `meson dist`; see
the [build guide](doc/building.md#source-packages).

### If you add code please add tests

Code that isn’t tested is broken.

Also, keep your tests simple. Complex tests end up requiring their own tests. We would rather see duplicated assertions across test methods then cunning utility methods that magically determine which assertions are needed at a particular stage. Remember: Explicit is better than implicit.

Finally, the nature of fplll means that sometimes it is hard to properly test the behaviour of a change quickly. Running BKZ for several minutes takes way too long for a test. In this case, we should at least test that a particular piece of code compiles and runs.

Fixtures under `tests/lattices/` and `strategies/` are read-only inputs. Tests
must write only into their build-tree working directory, `build/tests/`.

### Keep your pull requests limited to a single issue

Pull requests should be as small/atomic as possible.

### Coding Conventions

The project requires C++23 for the library, installed headers, command-line
tools and tests. Compiler modes older than C++23 are not supported.

Please keep your code as clean and straightforward as possible. Code is written for the consumption by compilers and for the consumption by human beings. By making code clear and easy to understand, others can build on it and fix issues should they arise.

Our naming convention is close to Python's [naming convention](https://www.python.org/dev/peps/pep-0008/). Classes are in ``CamelCase``. Functions, methods, parameters and local variables in ``lower_case`` . Curly braces go on the next line and we [prefer explicit curly braces](https://nakedsecurity.sophos.com/2014/02/24/anatomy-of-a-goto-fail-apples-ssl-bug-explained-plus-an-unofficial-patch/), e.g.

```c++
if (foo)
{
  do_something_good();
}
```

instead of:

```c++
if (foo)
  do_something_bad();
```

Furthermore, the pixel shortage is over. We want to see:

- `package` instead of `pkg`
- `grid` instead of `g`
- `my_function_that_does_things` instead of `mftdt`

### Formatting

The repository's `.clang-format` is the authoritative formatting
configuration; run `clang-format` directly on the files you change:

```sh
clang-format --style=file -i path/to/file.cpp
clang-format --style=file --dry-run --Werror path/to/file.cpp  # check only
```

Keep formatting changes separate from functional changes. The bundled
third-party header `src/fplll/io/thread_pool.hpp` follows its upstream formatting
and must not be reformatted; nlohmann/json is external and untouched by these
rules.

In order to improve readability, some situations might require manual formatting. Clang-format [includes a comment trigger](https://clang.llvm.org/docs/ClangFormatStyleOptions.html#disabling-formatting-on-a-piece-of-code) to _locally_ disable the formatting. The code between a comment `// clang-format off` or `/* clang-format off */` up to a comment `// clang-format on` or `/* clang-format on */` will not be formatted.

```c++
int formatted_code;
// clang-format off
    void    unformatted_code  ;
// clang-format on
void formatted_code_again;
```

### Faster compilation

Meson rebuilds incrementally. Install `ccache` or `sccache` and Meson picks it
up automatically the next time a build directory is configured; no compiler
wrapper is needed. Keep one build directory per configuration (developer,
release, sanitizers) instead of reconfiguring one directory back and forth, and
use `-Dtests=false` in a throwaway directory for quick compile-only checks.
Lowering `-Dmax_parallel_enum_dim` (for example to `20`) compiles fewer
enumeration specializations; this changes the configured capability, so keep
the default `120` for release validation. See the
[build guide](doc/building.md#build-options) for all options.

### Attribution

Please add yourself to the historical credits in [usage.md](doc/usage.md) if you make a non-trivial contribution. You may also claim copyright in the copyright headers of files you change.

## Documentation

With Doxygen found at configure time, build the API documentation:

```sh
meson compile -C build doc
```

HTML output is written to `build/doc/html/`. Library sources, the README,
changelog, and usage, build, and contribution guides are documentation inputs;
build configuration headers and local notes are excluded. Meson generates the
Doxygen configuration, and heading links use GitHub-style anchors so they work
in both the repository and generated pages. Review the generated documentation
before publishing to the separate `gh-pages` checkout. Never generate
documentation into the source tree.
