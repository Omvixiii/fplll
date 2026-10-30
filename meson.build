project(
  'fplll',
  'cpp',
  version: '5.5.0',
  license: 'LGPL-2.1-or-later',
  license_files: 'COPYING',
  meson_version: '>=1.3.0',
  default_options: [
    'cpp_std=c++11',
    'warning_level=1',
  ],
)

# --- Options ---------------------------------------------------------------

max_enum_dim = get_option('max_enum_dim')
max_parallel_enum_dim = get_option('max_parallel_enum_dim')

if max_parallel_enum_dim != 0 and (
    max_parallel_enum_dim < 20 or max_parallel_enum_dim % 10 != 0)
  error('max_parallel_enum_dim must be 0 or a multiple of 10 from 20 to 160')
endif
if max_parallel_enum_dim >= max_enum_dim
  error('max_enum_dim must exceed max_parallel_enum_dim')
endif

# --- Dependencies ----------------------------------------------------------

threads_dep = dependency('threads')
gmp_dep = dependency('gmp', version: '>=4.2')
mpfr_dep = dependency('mpfr', version: '>=2.3')
qd_dep = dependency('qd', required: get_option('qd'))

extenum_dep = dependency('', required: false)
if get_option('extenum_lib') != ''
  extenum_dep = declare_dependency(
    link_args: [
      '-L' + get_option('extenum_dir'),
      '-l' + get_option('extenum_lib'),
    ],
  )
endif

# --- Installation paths ----------------------------------------------------

project_name = meson.project_name()
strategy_dir = get_option('prefix') / get_option('datadir') / project_name / 'strategies'

add_project_arguments(
  '-DFPLLL_DEFAULT_STRATEGY_PATH="' + strategy_dir + '"',
  '-DFPLLL_DEFAULT_STRATEGY="' + (strategy_dir / 'default.json') + '"',
  language: 'cpp',
)

# --- Configuration headers -------------------------------------------------

version = meson.project_version().split('.')

conf = configuration_data()
conf.set('FPLLL_MAJOR_VERSION', version[0])
conf.set('FPLLL_MINOR_VERSION', version[1])
conf.set('FPLLL_MICRO_VERSION', version[2])
conf.set('FPLLL_VERSION', meson.project_version())
conf.set('FPLLL_VERSION_INFO', '')
conf.set('FPLLL_MAX_ENUM_DIM', max_enum_dim)
conf.set('FPLLL_MAX_PARALLEL_ENUM_DIM', max_parallel_enum_dim)
conf.set('FPLLL_WITH_QD', qd_dep.found())
conf.set('FPLLL_WITH_RECURSIVE_ENUM', get_option('recursive_enum'))
conf.set('HAVE_LIBGMP', not get_option('mpir'))
conf.set('HAVE_LIBMPIR', get_option('mpir'))

if get_option('extenum_lib') != ''
  extenum_func = get_option('extenum_func')
  if extenum_func == ''
    extenum_func = get_option('extenum_lib')
  endif
  conf.set('FPLLL_EXTENUM_FUNC', extenum_func)
else
  conf.set('FPLLL_EXTENUM_FUNC', false)
endif

# fplll_config.h is generated twice: the copy in the build root serves the
# <fplll/fplll_config.h> spelling, the copy in build/fplll serves the
# "fplll_config.h" and "../fplll_config.h" spellings used by the original
# sources.
fplll_config_h = configure_file(
  input: 'meson/fplll_config.h.in',
  output: 'fplll_config.h',
  configuration: conf,
)

# The command line tools include ../config.h for the VERSION string.
version_conf = configuration_data()
version_conf.set('VERSION', meson.project_version())
configure_file(
  input: 'meson/config.h.in',
  output: 'config.h',
  configuration: version_conf,
)

root_inc = include_directories('.')
fplll_inc = include_directories('fplll')

subdir('fplll')
subdir('tests')

# --- Data files ------------------------------------------------------------

install_data(
  'strategies/default.json',
  install_dir: get_option('datadir') / project_name / 'strategies',
)
