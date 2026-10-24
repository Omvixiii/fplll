project(
  'fplll',
  'cpp',
  version: '6.0.0',
  license: 'LGPL-2.1-or-later',
  license_files: 'LICENSE',
  meson_version: '>=1.11.0',
  default_options: [
    # Installers get a release build; developers pass --buildtype=debugoptimized.
    'buildtype=release',
    'cpp_std=c++23',
    'default_library=both',
  ],
)

# --- Versions and paths ----------------------------------------------------
# The product version and the libtool-style ABI version are independent.
fplll_soversion = '10'
fplll_libversion = '10.0.0'
fplll_darwin_versions = ['11.0.0', '11.0.0']
version_parts = meson.project_version().split('.')
strategies_install_dir = get_option('datadir') / 'strategies'
default_strategy_dir = get_option('prefix') / strategies_install_dir

# --- Toolchain -------------------------------------------------------------
# warning_level=3 adds -Wall -Wextra -Wpedantic; pair it with the extra checks.
cpp = meson.get_compiler('cpp')
if get_option('warning_level') in ['3', 'everything']
  add_project_arguments(
    cpp.get_supported_arguments(['-Wshadow', '-Wnon-virtual-dtor', '-Woverloaded-virtual']),
    language: 'cpp',
  )
endif

# --- Options ---------------------------------------------------------------
max_enum_dim = get_option('max_enum_dim')
max_parallel_enum_dim = get_option('max_parallel_enum_dim').to_int()

if max_parallel_enum_dim >= max_enum_dim
  error('max_enum_dim must exceed max_parallel_enum_dim')
endif

# --- Dependencies ----------------------------------------------------------
threads_dep = dependency('threads')
gmp_dep = dependency('gmp', version: '>=4.2')
mpfr_dep = dependency('mpfr', version: '>=2.3')
qd_dep = dependency('qd', required: get_option('qd'))

nlohmann_json_dep = dependency(
  'nlohmann_json',
  version: '>=3.12.0',
  fallback: ['nlohmann_json', 'nlohmann_json_dep'],
  include_type: 'system',
)

extenum_dep = dependency('fplll_extenum', required: false)

public_deps = [gmp_dep, mpfr_dep, threads_dep]
private_deps = [nlohmann_json_dep]

if qd_dep.found()
  public_deps += qd_dep
endif

# Some mpfr.pc files add -lgmp without gmp's library directory; gmp.pc has it.
gmp_libdir = gmp_dep.get_variable(pkgconfig: 'libdir', default_value: '')
if gmp_libdir != ''
  public_deps += declare_dependency(link_args: ['-L' + gmp_libdir])
endif

extenum_cpp_args = []
if extenum_dep.found()
  private_deps += extenum_dep
  extenum_cpp_args += '-DFPLLL_EXTENUM_FUNC=fplll_extenum_enumerate'
endif

# --- Build targets ---------------------------------------------------------
public_inc = include_directories('include')
subdir('include/fplll')
subdir('src')
subdir('tools')

# --- Install ---------------------------------------------------------------
install_data('strategies/default.json', install_dir: strategies_install_dir)
install_subdir(
  'include',
  install_dir: get_option('includedir'),
  strip_directory: true,
  exclude_files: ['fplll/meson.build'],
)

# --- Tests and documentation ----------------------------------------------
if not meson.is_subproject()
  if get_option('tests')
    subdir('tests')
  endif
  subdir('doc')
endif

# --- Summary ---------------------------------------------------------------
summary(
  {
    'Build type': get_option('buildtype'),
    'Default library': get_option('default_library'),
    'Install prefix': get_option('prefix'),
    'nlohmann/json': nlohmann_json_dep.version(),
    'QD support': qd_dep.found(),
    'External enumeration': extenum_dep.found(),
    'Max enumeration dimension': max_enum_dim,
    'Max parallel enumeration dimension': max_parallel_enum_dim,
    'Tests': not meson.is_subproject() and get_option('tests'),
  },
  section: 'fplll',
)
