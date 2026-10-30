# SPDX-FileCopyrightText: 2026 Omvixiii
# SPDX-License-Identifier: LGPL-2.1-or-later

# This directory is entered only when the tests option is enabled, so a default
# build never resolves Catch2.  The test programs are built on demand as well:
# `meson test` builds its prerequisites before running them.  Catch2 comes from
# a system package when available, otherwise from the pinned wrap.
catch2_dep = dependency(
    'catch2-with-main',
    version: '>=3.0.0',
    default_options: ['tests=false', 'install=false'],
)

# One executable keeps the build and link cost low; every module is then
# registered as its own test case filter, so `meson test -C build` still runs
# the modules independently.
test_sources = files(
    'cli/test_cli.cpp',
    'core/test_public_macros.cpp',
    'core/test_regressions.cpp',
    'core/test_threadpool.cpp',
    'enum/test_counter.cpp',
    'enum/test_enum.cpp',
    'enum/test_enum_bound.cpp',
    'gso/test_babai.cpp',
    'gso/test_gso.cpp',
    'nr/test_ceil.cpp',
    'nr/test_nr.cpp',
    'nr/test_rng.cpp',
    'pruner/test_pruner.cpp',
    'pruner/test_pruner_memory.cpp',
    'reduction/test_bkz.cpp',
    'reduction/test_bkz_gram.cpp',
    'reduction/test_config.cpp',
    'reduction/test_hlll.cpp',
    'reduction/test_lll.cpp',
    'reduction/test_lll_gram.cpp',
    'test_cvp.cpp',
    'test_svp.cpp',
    'test_svp_gram.cpp',
)

# The command-line tools are spawned by the cli test case, so their paths are
# compiled in: Catch2 consumes the process command line.
test_exe = executable(
    project_name + '_tests',
    test_sources,
    dependencies: [library_dep, catch2_dep],
    cpp_args: [
        '-DFPLLL_TESTING',
        '-DFPLLL_TOOL="' + cli_tool.full_path() + '"',
        '-DLATTICEGEN_TOOL="' + latticegen_tool.full_path() + '"',
        '-DTESTDATADIR="' + meson.project_source_root() + '/"',
    ],
    build_by_default: false,
)

# Fixtures are read-only inputs from the source tree; tests write only into
# this build directory.
test_workdir = meson.current_build_dir()

# Catch2 3.9 and newer randomise the test case order by default.  Keep the
# suite deterministic and in declaration order.
test_args = ['--order', 'decl']

# Register one test per module tag so a failure names the module and a module
# can be rerun on its own: `meson test -C build lll`.
module_tags = {
    'lll': '[lll]',
    'hlll': '[hlll]',
    'bkz': '[bkz]',
    'gso': '[gso]',
    'babai': '[babai]',
    'enum': '[enum]',
    'nr': '[nr]',
    'rng': '[rng]',
    'pruner': '[pruner]',
    'svp': '[svp]',
    'cvp': '[cvp]',
    'config': '[config]',
    'threadpool': '[threadpool]',
    'public-macros': '[public-macros]',
    'regression': '[regression]',
}

# The strategy-table test and the cli test write fixed file names into the
# shared test directory; keep them out of the parallel batch.
serial_tests = ['config']
foreach name, tag : module_tags
    test(
        name,
        test_exe,
        args: test_args + [tag],
        workdir: test_workdir,
        timeout: 300,
        is_parallel: name not in serial_tests,
    )
endforeach

# The cli test spawns the built tools, so they must exist before it runs.
test(
    'cli',
    test_exe,
    args: test_args + ['[cli]'],
    workdir: test_workdir,
    timeout: 120,
    is_parallel: false,
    depends: [cli_tool, latticegen_tool],
)
