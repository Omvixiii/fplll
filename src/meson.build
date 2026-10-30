# SPDX-FileCopyrightText: 2026 Omvixiii
# SPDX-License-Identifier: LGPL-2.1-or-later

# --- Library sources --------------------------------------------------------
library_sources = files(
    'enum/enumerate.cpp',
    'enum/enumerate_base.cpp',
    'enum/evaluator.cpp',
    'gso/gso.cpp',
    'gso/gso_gram.cpp',
    'gso/gso_interface.cpp',
    'gso/householder.cpp',
    'pruner/pruner.cpp',
    'reduction/bkz/bkz_driver.cpp',
    'reduction/bkz/bkz_kernel.cpp',
    'reduction/bkz/bkz_param.cpp',
    'reduction/hlll/hlll_driver.cpp',
    'reduction/hlll/hlll_kernel.cpp',
    'reduction/lll/lll_driver.cpp',
    'reduction/lll/lll_kernel.cpp',
    'reduction/wrapper.cpp',
    'svpcvp.cpp',
    'threadpool.cpp',
    'util.cpp',
)

# --- Generated parallel-enumeration translation units -----------------------
# The parallel enumerator is a template over the block size; each dimension
# class is a separate translation unit so the compiler only instantiates the
# dimensions the build enables.
if max_parallel_enum_dim > 0
    library_sources += files('enum/parallel/enumlib.cpp')
    foreach dimension : range(20, max_parallel_enum_dim + 1, 10)
        library_sources += configure_file(
            input: '../meson/enumlib_dim.cpp.in',
            output: 'enumlib_dim.@0@.cpp'.format(dimension),
            configuration: {'ENUMDIMENSION': dimension},
        )
    endforeach
endif

# The in-tree test hooks of the thread pool are compiled into the library only
# when the test suite is enabled, so the default build (and release packages
# produced from it) carries no test-only code.  The declarations stay private
# to src/detail/.
library_cpp_args = [
    '-DFPLLL_STRATEGIES_DIR="' + default_strategy_dir + '"',
]
if not meson.is_subproject() and get_option('tests')
    library_cpp_args += '-DFPLLL_TESTING'
endif

library_target = library(
    project_name,
    library_sources,
    include_directories: public_inc,
    dependencies: public_deps + private_deps,
    cpp_args: library_cpp_args,
    version: library_version,
    install: true,
)

library_dep = declare_dependency(
    link_with: library_target,
    include_directories: public_inc,
    dependencies: public_deps,
)
if get_option('default_library') == 'both'
    meson.override_dependency(project_name, library_dep.as_shared(), static: false)
    meson.override_dependency(project_name, library_dep.as_static(), static: true)
else
    meson.override_dependency(project_name, library_dep)
endif

# --- pkg-config metadata ----------------------------------------------------
# The public headers expose GMP, MPFR and QD types, so they are public
# requirements; thread flags are also needed by public inline code.
pkgconfig = import('pkgconfig')

# pkg-config describes the shared library; static-only builds describe the
# static one.  A both-libraries build exposes the shared flavour, matching the
# usual distribution layout.
pc_library = library_target
if get_option('default_library') == 'both'
    pc_library = library_target.get_shared_lib()
endif

pkgconfig.generate(
    pc_library,
    description: 'Lattice algorithms using floating-point arithmetic',
    url: 'https://github.com/Omvixiii/fplll',
    requires: [gmp_dep, mpfr_dep, qd_dep],
    libraries: threads_dep,
)
