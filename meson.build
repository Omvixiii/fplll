# SPDX-FileCopyrightText: 2026 Omvixiii
# SPDX-License-Identifier: LGPL-2.1-or-later

project(
    'fplll',
    'cpp',
    version: '6.0.0',
    license: 'LGPL-2.1-or-later',
    license_files: [
        'LICENSE',
        'LICENSES/LGPL-2.1-or-later.txt',
        'LICENSES/MIT.txt',
    ],
    meson_version: '>=1.8.0',
    # C++23 is the project baseline.  Release builds define NDEBUG (as
    # distributions do): Meson keeps assertions in release by default, and the
    # libc++ container hardening that comes with it costs about 20% on LLL.
    default_options: ['cpp_std=c++23', 'warning_level=2', 'b_ndebug=if-release'],
)

# --- Project metadata and paths --------------------------------------------
project_name = meson.project_name()
# The product version and the libtool-style ABI version are independent.
library_version = '10.0.0'
version_parts = meson.project_version().split('.')
strategies_install_dir = get_option('datadir') / project_name / 'strategies'
default_strategy_dir = get_option('prefix') / strategies_install_dir

# --- Options ----------------------------------------------------------------
max_enum_dim = get_option('max_enum_dim')
max_parallel_enum_dim = get_option('max_parallel_enum_dim')
debug_checks = get_option('debug_checks')

if max_parallel_enum_dim != 0 and (
    max_parallel_enum_dim < 20
    or max_parallel_enum_dim % 10 != 0
)
    error('max_parallel_enum_dim must be 0 or a multiple of 10 from 20 to 160')
endif
if max_parallel_enum_dim >= max_enum_dim
    error('max_enum_dim must exceed max_parallel_enum_dim')
endif

# --- Dependencies -----------------------------------------------------------
# GMP, MPFR and threads are part of the public interface; nlohmann/json is a
# private dependency of the strategy-table loader.
threads_dep = dependency('threads')
gmp_dep = dependency('gmp', version: '>=4.2')
mpfr_dep = dependency('mpfr', version: '>=2.3')
qd_dep = dependency('qd', required: get_option('qd'))

nlohmann_json_dep = dependency(
    'nlohmann_json',
    version: '>=3.12.0',
    include_type: 'system',
)

public_deps = [gmp_dep, mpfr_dep, threads_dep, qd_dep]
private_deps = [nlohmann_json_dep]

# --- Build targets ----------------------------------------------------------
public_inc = include_directories('include')
subdir('include/fplll')
subdir('src')
subdir('tools')

# --- Install ----------------------------------------------------------------
install_data(
    'strategies/default.json',
    install_dir: strategies_install_dir,
    install_tag: 'runtime',
)
install_subdir(
    'include',
    install_dir: get_option('includedir'),
    strip_directory: true,
    exclude_files: ['fplll/meson.build'],
)

# The license metadata in project() reaches the dependency manifests only;
# Meson does not install those files, so install the texts themselves.
doc_install_dir = get_option('datadir') / 'doc' / project_name
install_data(
    'LICENSE',
    install_dir: doc_install_dir,
    install_tag: 'runtime',
)
install_data(
    'LICENSES/LGPL-2.1-or-later.txt',
    'LICENSES/MIT.txt',
    install_dir: doc_install_dir / 'LICENSES',
    install_tag: 'runtime',
)

# --- Tests and documentation ------------------------------------------------
# The test suite is opt-in.  When it is off, subdir('tests') is not entered, so
# the Catch2 dependency is never resolved and no Catch2 objects are built: the
# default build stays limited to the library and the tools.
if not meson.is_subproject()
    if get_option('tests')
        subdir('tests')
    endif
    subdir('doc')
endif

# --- Summary ----------------------------------------------------------------
summary(
    {
        'Build type': get_option('buildtype'),
        'Default library': get_option('default_library'),
        'Debug checks': debug_checks,
        'Install prefix': get_option('prefix'),
        'Strategies directory': default_strategy_dir,
        'QD support': qd_dep.found(),
        'Max enumeration dimension': max_enum_dim,
        'Max parallel enumeration dimension': max_parallel_enum_dim,
        'Tests': not meson.is_subproject() and get_option('tests'),
    },
)
