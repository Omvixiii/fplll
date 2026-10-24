# Generate fplll_config.h under a "fplll" directory so the build tree mirrors
# the installed include layout.
fplll_config_h = configure_file(
  input: '../../meson/fplll_config.h.in',
  output: 'fplll_config.h',
  configuration: {
    'FPLLL_MAJOR_VERSION': version_parts[0],
    'FPLLL_MINOR_VERSION': version_parts[1],
    'FPLLL_MICRO_VERSION': version_parts[2],
    'FPLLL_VERSION': meson.project_version(),
    'FPLLL_WITH_QD': qd_dep.found(),
    'FPLLL_MAX_ENUM_DIM': max_enum_dim,
    'FPLLL_MAX_PARALLEL_ENUM_DIM': max_parallel_enum_dim,
  },
)
install_data(fplll_config_h, install_dir: get_option('includedir') / 'fplll')
