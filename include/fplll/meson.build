# Public configuration header. Generating it inside a source subdirectory
# called "fplll" makes the build tree mirror the installed include layout:
# build/include/fplll/fplll_config.h is reachable as <fplll/fplll_config.h>
# through the single -I<build>/include entry (configure_file cannot create
# path segments in its output name).
configuration = configuration_data()
configuration.set('FPLLL_MAJOR_VERSION', version_parts[0])
configuration.set('FPLLL_MINOR_VERSION', version_parts[1])
configuration.set('FPLLL_MICRO_VERSION', version_parts[2])
configuration.set('FPLLL_VERSION', meson.project_version())
configuration.set('FPLLL_WITH_QD', with_qd)
configuration.set('FPLLL_MAX_ENUM_DIM', max_enum_dim)
configuration.set('FPLLL_MAX_PARALLEL_ENUM_DIM', max_parallel_enum_dim)
fplll_config_h = configure_file(
  input: '../../meson/fplll_config.h.in',
  output: 'fplll_config.h',
  configuration: configuration,
)
install_data(fplll_config_h, install_dir: get_option('includedir') / 'fplll')
