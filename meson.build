project(
  'fplll',
  'cpp',
  version: '6.0.0',
  license: 'LGPL-2.1-or-later',
  license_files: 'LICENSE',
  meson_version: '>=1.11.0',
  default_options: [
    # C++23 is the project baseline.
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
strategy_dir = get_option('prefix') / get_option('datadir') / 'strategies'

cpp = meson.get_compiler('cpp')
fs = import('fs')

# --- Toolchain -------------------------------------------------------------
# warning_level=3 adds -Wall -Wextra -Wpedantic; pair it with the extra checks.
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
gmp_dep = dependency('gmp', version: '>=4.2', method: 'pkg-config')
mpfr_dep = dependency('mpfr', version: '>=2.3', method: 'pkg-config')

# Header-only JSON support: prefer a system copy,
# otherwise fetch the pinned wrap in subprojects/ during configuration.
nlohmann_json_dep = dependency(
  'nlohmann_json',
  version: '>=3.12.0',
  fallback: ['nlohmann_json', 'nlohmann_json_dep'],
  include_type: 'system',
)

public_deps = [gmp_dep, mpfr_dep, threads_dep]
private_deps = [nlohmann_json_dep]
pc_public_libs = [threads_dep]

# Homebrew's mpfr.pc lists -lgmp without gmp's -L flag.
if host_machine.system() == 'darwin'
  gmp_libdir = gmp_dep.get_variable(pkgconfig: 'libdir', default_value: '')
  if gmp_libdir != ''
    public_deps += declare_dependency(link_args: ['-L' + gmp_libdir])
  endif
endif

# QD ships a pkg-config file upstream; fall back to a plain lookup when a
# distribution only installs the library and headers.
qd_option = get_option('qd')
qd_dep = []
qd_via_pkgconfig = false
with_qd = false
if qd_option.allowed()
  qd_dep = dependency('qd', method: 'pkg-config', required: false)
  qd_via_pkgconfig = qd_dep.found()
  if not qd_dep.found()
    qd_library = cpp.find_library('qd', required: false)
    if qd_library.found() and cpp.has_header('qd/dd_real.h') and cpp.has_header('qd/qd_real.h')
      qd_dep = qd_library
    endif
  endif
  with_qd = qd_dep.found()
  if qd_option.enabled() and not with_qd
    error(
      'QD was requested (-Dqd=enabled) but neither qd.pc nor libqd with qd/dd_real.h was found',
    )
  endif
  if with_qd
    public_deps += qd_dep
    if not qd_via_pkgconfig
      # QD operations also appear in installed inline/template definitions.
      pc_public_libs += qd_dep
    endif
  endif
endif

# --- Optional external enumeration library --------------------------------
# An external enumerator is a library exporting a function with the
# extenum_fc_enumerate signature under the well-known name
# fplll_extenum_enumerate. Install it as a Meson dependency named
# "fplll_extenum" (a fplll_extenum.pc on PKG_CONFIG_PATH or a wrap) to make it
# the process-wide default; applications can still override the choice at run
# time with set_external_enumerator().
extenum_dep = dependency('fplll_extenum', required: false)
extenum_cpp_args = []
if extenum_dep.found()
  private_deps += extenum_dep
  extenum_cpp_args += ['-DFPLLL_EXTENUM_FUNC=fplll_extenum_enumerate']
endif

# --- Install paths ---------------------------------------------------------
# Tools resolve libdir relative to their own directory; an absolute libdir is
# used verbatim. Windows has no RPATH equivalent.
tool_install_rpath = ''
if host_machine.system() != 'windows'
  install_libdir = get_option('libdir')
  if fs.is_absolute(install_libdir)
    tool_install_rpath = install_libdir
  else
    libdir_from_bindir = fs.relative_to(install_libdir, get_option('bindir'))
    if host_machine.system() == 'darwin'
      tool_install_rpath = '@loader_path/' + libdir_from_bindir
    else
      tool_install_rpath = '$ORIGIN/' + libdir_from_bindir
    endif
  endif
endif

# --- Build targets ---------------------------------------------------------
# Public headers live in include/fplll/, implementation in src/; the build tree
# mirrors that split so <fplll/...> resolves through one -I entry.
public_inc = include_directories('include')
subdir('include/fplll')
subdir('src')
subdir('tools')

# --- Install ---------------------------------------------------------------

install_data(
  'strategies/default.json',
  install_dir: get_option('datadir') / 'strategies',
)

# The include tree is installed as-is; fplll_config.h comes from
# include/fplll/meson.build.
install_subdir(
  'include/fplll',
  install_dir: get_option('includedir'),
  exclude_files: ['meson.build'],
)
install_headers('include/fplll.h')

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
    'QD support': with_qd,
    'External enumeration': extenum_dep.found(),
    'Max enumeration dimension': max_enum_dim,
    'Max parallel enumeration dimension': max_parallel_enum_dim,
    'Tests': not meson.is_subproject() and get_option('tests'),
  },
  section: 'fplll',
)
