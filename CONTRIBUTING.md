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

Install CMake 3.25 or newer, Ninja, a C++11-capable compiler, GMP, and MPFR.
Configure, build, and test with the shared workflow:

```sh
cmake --workflow --preset debug
```

For the edit/build loop, use `cmake --build --preset debug`, optionally selecting
a target with `--target` or adding `--verbose` for compiler commands. CTest alone
does not build changed code; run the workflow before submitting a change. See
the [build guide](doc/building.md) for options, diagnostics, and output locations.

## Reporting Bugs

Bug should be filed at [https://github.com/fplll/fplll/issues](https://github.com/fplll/fplll/issues). Alternatively, feel free to contact  [https://groups.google.com/forum/#!forum/fplll-devel](https://groups.google.com/forum/#!forum/fplll-devel). The former method is definitely preferred though.

## Setting up topic branches and generating pull requests

While it's handy to provide useful code snippets in an issue, it is better for to submit pull requests.

In Git it is best to isolate each topic or feature into a "topic branch". While individual commits allow you control over how small individual changes are made to the code, branches are a great way to group a set of commits all related to one feature together, or to isolate different efforts when you might be working on multiple topics at the same time.

While it takes some experience to get the right feel about how to break up commits, a topic branch should be limited in scope to a single ``issue`` as submitted to an issue tracker.

Also since GitHub pegs and syncs a pull request to a specific branch, it is the **only** way that you can submit more than one fix at a time. If you submit a pull from your ``master`` branch, you can't make any more commits to your ``master`` branch without those getting added to the pull.

To create a topic branch, its easiest to use the convenient ``-b`` argument to ``git checkout``

```sh
git checkout -b fix-broken-thing
```

You should use a verbose enough name for your branch so it is clear what it is about. Now you can commit your changes and regularly merge in the upstream develop as described below.

When you are ready to generate a pull request, either for preliminary review, or for consideration of merging into the project you must first push your local topic branch back up to GitHub

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

Before you submit a pull request, configure, build, and run the tests:

```sh
cmake --workflow --preset debug
```

Use the `release` configure, build, and test presets to validate an optimized
build. Changes to installation, packaging, or the build system should also pass
`cmake --workflow --preset distcheck`; see the [build guide](doc/building.md#source-packages).

### If you add code please add tests

Code that isn’t tested is broken.

Also, keep your tests simple. Complex tests end up requiring their own tests. We would rather see duplicated assertions across test methods then cunning utility methods that magically determine which assertions are needed at a particular stage. Remember: Explicit is better than implicit.

Finally, the nature of fplll means that sometimes it is hard to properly test the behaviour of a change quickly. Running BKZ for several minutes takes way too long for a test. In this case, we should at least test that a particular piece of code compiles and runs.

### Keep your pull requests limited to a single issue

Pull requests should be as small/atomic as possible.

### Coding Conventions

The public library and default builds remain compatible with C++11. A newer
compiler mode used locally must not introduce requirements beyond that baseline.

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

The repository's `.clang-format` is the authoritative formatting
configuration. Keep formatting changes separate from functional changes.

### Enforcement of the coding convention

The coding convention applies throughout the project. Automatic formatting can
be performed with:

```sh
cmake --build --preset debug --target format
```

This requires clang-format. The `lint` target checks formatting without modifying
files. You can also run `cmake -P cmake/lints.cmake`
without configuring the project. Both commands include test headers and inline
implementation files, and exclude bundled third-party headers.

In order to improve readability, some situations might require manual formatting. Clang-format [includes a comment trigger](http://llvm.org/releases/6.0.0/tools/clang/docs/https://releases.llvm.org/6.0.0/tools/clang/docs/ClangFormatStyleOptions.html#disabling-formatting-on-a-piece-of-code) to _locally_ disable the formatting. The code between a comment `// clang-format off` or `/* clang-format off */` up to a comment `// clang-format on` or `/* clang-format on */` will not be formatted.

```c++
int formatted_code;
// clang-format off
    void    unformatted_code  ;
// clang-format on
void formatted_code_again;
```

### Faster compilation

The shared and static libraries reuse the same compiled OBJECT library. Build
only the target you are changing during development, then run the workflow for
full validation. To enable an installed ccache, configure with
`-DCMAKE_CXX_COMPILER_LAUNCHER=ccache`; shared presets do not require it.
Debug variant objects are compiled only when a debug variant target is requested.
These variants enable fplll's `DEBUG` macro and are distinct from the Debug
build type. Reducing the parallel enumeration dimension is an optional local
tradeoff, not the release default; see the [build guide](doc/building.md#build-options-and-targets).

### Attribution

Please add yourself to the historical credits in [usage.md](doc/usage.md) if you
make a non-trivial contribution. You may also claim copyright in the copyright
headers of files you change.

## Documentation

With Doxygen 1.9.8 or newer installed, configure a preset and build the `doc` target:

```sh
cmake --preset debug
cmake --build --preset debug --target doc
```

HTML output is written to `build/debug/doc/html/`. Library sources, the
README, changelog, and usage, build, and contribution guides are documentation
inputs; build configuration headers and local notes are excluded.
The [build guide](doc/building.md#development-utilities) describes the generated
configuration, optional Graphviz diagrams, and formula rendering.
Review the generated documentation before publishing to the separate `gh-pages`
checkout. Never generate documentation into the source tree.
