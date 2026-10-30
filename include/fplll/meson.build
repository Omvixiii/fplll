# SPDX-FileCopyrightText: 2026 Omvixiii
# SPDX-License-Identifier: LGPL-2.1-or-later

# Generate fplll_config.h under include/fplll so the build tree mirrors the
# installed include layout.
configure_file(
    input: '../../meson/fplll_config.h.in',
    output: 'fplll_config.h',
    configuration: {
        'FPLLL_MAJOR_VERSION': version_parts[0],
        'FPLLL_MINOR_VERSION': version_parts[1],
        'FPLLL_MICRO_VERSION': version_parts[2],
        'FPLLL_VERSION': meson.project_version(),
        'FPLLL_WITH_QD': qd_dep.found(),
        'FPLLL_DEBUG_CHECKS': debug_checks,
        'FPLLL_MAX_ENUM_DIM': max_enum_dim,
        'FPLLL_MAX_PARALLEL_ENUM_DIM': max_parallel_enum_dim,
    },
    install: true,
    install_dir: get_option('includedir') / 'fplll',
)
