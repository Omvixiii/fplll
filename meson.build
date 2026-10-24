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
gmp_dep = dependency('gmp', version: '>=4.2')
mpfr_dep = dependency('mpfr', version: '>=2.3')

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

# Homebrew's mpfr.pc omits "Requires: gmp" and gmp's -L; repair the link only
# when a probe fails, using gmp.pc's libdir (link_args keeps it out of fplll.pc).
gmp_link_args = []
if not cpp.links('int main(void) { return 0; }', dependencies: [mpfr_dep])
  gmp_pc_libdir = gmp_dep.get_variable(pkgconfig: 'libdir', default_value: '')
  if gmp_pc_libdir != ''
    gmp_link_args = ['-L' + gmp_pc_libdir]
  endif
endif

# --- Optional QD support ---------------------------------------------------
# Prefer qd.pc; fall back to a plain library lookup for installs without it.
# The fallback cannot export QD include paths downstream (see src/meson.build).
qd_option = get_option('qd')
qd_dep = []
qd_via_pkgconfig = false
with_qd = false
if qd_option.allowed()
  qd_dep = dependency('qd', method: 'pkg-config', required: false)
  qd_via_pkgconfig = qd_dep.found()
  if not qd_dep.found()
    qd_dep = cpp.find_library(
      'qd',
      has_headers: ['qd/dd_real.h', 'qd/qd_real.h'],
      required: false,
    )
  endif
  with_qd = qd_dep.found()
  if qd_option.enabled() and not with_qd
    error(
      'QD was requested (-Dqd=enabled) but neither qd.pc nor libqd with '
      + 'qd/dd_real.h and qd/qd_real.h was found',
    )
  endif
  if with_qd
    public_deps += qd_dep
  endif
endif

# --- Optional external enumeration library --------------------------------
# An external enumeration library exports a function with the
# extenum_fc_enumerate signature under the well-known name
# fplll_extenum_enumerate. There is no project option: provide the library as
# a Meson dependency named "fplll_extenum" (a fplll_extenum.pc on
# PKG_CONFIG_PATH, or a wrap) to make it the build-time default. Without it the
# built-in enumerator is used. Applications can still override the choice at
# run time with set_external_enumerator().
extenum_dep = dependency('fplll_extenum', required: false)
extenum_cpp_args = []
if extenum_dep.found()
  private_deps += extenum_dep
  extenum_cpp_args += ['-DFPLLL_EXTENUM_FUNC=fplll_extenum_enumerate']
endif

# --- Install RPATH ---------------------------------------------------------
# RPATH is a deployment policy, not dependency discovery. auto: relative RPATH
# for non-/usr prefixes and non-static builds; enabled/disabled force it.
# Derive the path from bindir/libdir so lib64/multiarch/absolute layouts work.
tool_install_rpath = ''
rpath_summary = 'disabled'
rpath_option = get_option('rpath')
rpath_enabled = false
if host_machine.system() != 'windows' and rpath_option.allowed()
  rpath_enabled = rpath_option.enabled() or (
    rpath_option.auto()
    and get_option('prefix') != '/usr'
    and get_option('default_library') != 'static'
  )
endif
if rpath_enabled
  install_libdir = get_option('libdir')
  install_bindir = get_option('bindir')
  if fs.is_absolute(install_libdir)
    rpath_target = install_libdir
  elif fs.is_absolute(install_bindir)
    # Relative libdir with absolute bindir: use the absolute location.
    rpath_target = get_option('prefix') / install_libdir
  else
    libdir_from_bindir = fs.relative_to(install_libdir, install_bindir)
    if libdir_from_bindir == '.'
      if host_machine.system() == 'darwin'
        rpath_target = '@loader_path'
      else
        rpath_target = '$ORIGIN'
      endif
    elif host_machine.system() == 'darwin'
      rpath_target = '@loader_path/' + libdir_from_bindir
    else
      rpath_target = '$ORIGIN/' + libdir_from_bindir
    endif
  endif
  tool_install_rpath = rpath_target
  rpath_summary = rpath_target
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
    'Install RPATH': rpath_summary,
    'External enumeration': extenum_dep.found(),
    'Max enumeration dimension': max_enum_dim,
    'Max parallel enumeration dimension': max_parallel_enum_dim,
    'Tests': not meson.is_subproject() and get_option('tests'),
  },
  section: 'fplll',
)
