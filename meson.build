project(
  'fplll', 'cpp',
  version: '6.0.0',
  license: 'LGPL-2.1-or-later',
  license_files: 'LICENSE',
  meson_version: '>=1.8.0',
  default_options: [
    'cpp_std=c++23',
    'default_library=both',
  ],
)

# The product version and the libtool-style ABI version are independent.
fplll_soversion = '10'
fplll_libversion = '10.0.0'
fplll_darwin_versions = ['11.0.0', '11.0.0']
strategy_dir = get_option('prefix') / get_option('datadir') / 'fplll' / 'strategies'
version_parts = meson.project_version().split('.')

cpp = meson.get_compiler('cpp')
fs = import('fs')

# C++23 is the project baseline. Reject an explicit downgrade early instead of
# letting the compiler fail deep inside the headers.
if get_option('cpp_std') in [
  'none', 'c++98', 'gnu++98', 'c++03', 'gnu++03', 'c++11', 'gnu++11',
  'c++14', 'gnu++14', 'c++17', 'gnu++17', 'c++20', 'gnu++20',
]
  error('fplll requires C++23 or newer; got cpp_std=' + get_option('cpp_std'))
endif

# warning_level=3 adds -Wall -Wextra -Wpedantic; the project has always paired
# those with the extra checks below when strict warnings are requested.
if get_option('warning_level') in ['3', 'everything']
  add_project_arguments(
    cpp.get_supported_arguments(['-Wshadow', '-Wnon-virtual-dtor', '-Woverloaded-virtual']),
    language: 'cpp',
  )
endif

# --- Option validation -----------------------------------------------------
max_enum_dim = get_option('max_enum_dim')
if max_enum_dim < 1
  error('max_enum_dim must be a positive integer')
endif
# Enumeration node counters are fixed-size arrays of this length.
if max_enum_dim > 1024
  error('max_enum_dim must not exceed 1024')
endif
max_parallel_enum_dim = get_option('max_parallel_enum_dim').to_int()
if max_parallel_enum_dim >= max_enum_dim
  error('max_enum_dim must exceed max_parallel_enum_dim')
endif

# --- Dependencies ----------------------------------------------------------
threads_dep = dependency('threads')
gmp_dep = dependency('gmp', version: '>=4.2', method: 'pkg-config')
mpfr_dep = dependency('mpfr', version: '>=2.3', method: 'pkg-config')

# nlohmann/json parses the BKZ strategy files. It is header-only and no longer
# vendored: a compatible system installation is preferred, and otherwise the
# pinned wrap in subprojects/ is fetched during configuration.
nlohmann_json_dep = dependency(
  'nlohmann_json',
  version: '>=3.12.0',
  fallback: ['nlohmann_json', 'nlohmann_json_dep'],
  include_type: 'system',
)

public_deps = [gmp_dep, mpfr_dep, threads_dep]
private_deps = [nlohmann_json_dep]
pc_public_libs = [threads_dep]

# Homebrew's mpfr.pc lists -lgmp without gmp's -L flag. Keep the gmp library
# directory on the link line on macOS so that -lgmp resolves.
if host_machine.system() == 'darwin'
  gmp_libdir = gmp_dep.get_variable(pkgconfig: 'libdir', default_value: '')
  if gmp_libdir != ''
    public_deps += declare_dependency(link_args: ['-L' + gmp_libdir])
  endif
endif

# QD ships a pkg-config file upstream, but several distributions only install
# the library and headers; fall back to a plain compiler/linker lookup then.
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
    error('QD was requested (-Dqd=enabled) but neither qd.pc nor libqd with qd/dd_real.h was found')
  endif
endif
if with_qd
  public_deps += qd_dep
  if not qd_via_pkgconfig
    # QD operations also appear in installed inline/template definitions.
    pc_public_libs += qd_dep
  endif
endif

# std::atomic needs libatomic only on some targets (for example 32-bit x86).
# Probe first so the installed pkg-config metadata stays minimal.
atomic_dep = []
atomic_source = '''#include <atomic>
#include <cstdint>
int main() {
  std::atomic<int64_t> value{};
  return static_cast<int>(value.fetch_add(1));
}
'''
if not cpp.links(atomic_source, name: 'std::atomic without libatomic')
  atomic_dep = cpp.find_library('atomic', required: false)
  if not atomic_dep.found() or not cpp.links(
      atomic_source, dependencies: atomic_dep, name: 'std::atomic with libatomic')
    error('cannot link std::atomic with or without libatomic')
  endif
  public_deps += atomic_dep
  pc_public_libs += atomic_dep
endif

# --- Optional external enumeration library --------------------------------
extenum_library = get_option('extenum_library')
extenum_function = get_option('extenum_function')
with_extenum = extenum_library != ''
extenum_dep = []
if with_extenum
  if extenum_function == ''
    extenum_function = fs.name(extenum_library).split('.')[0]
    if extenum_function.startswith('lib')
      extenum_function = extenum_function.substring(3)
    endif
  endif
  if extenum_function == '' or extenum_function != extenum_function.underscorify() or extenum_function.substring(0, 1) in ['0', '1', '2', '3', '4', '5', '6', '7', '8', '9']
    error('extenum_function must be a C++ identifier')
  endif
  if fs.is_absolute(extenum_library) or extenum_library.contains('/')
    extenum_library = meson.project_source_root() / extenum_library
    if not fs.is_file(extenum_library)
      error('external enumeration library not found: ' + extenum_library)
    endif
    extenum_dep = [declare_dependency(link_args: [extenum_library])]
  else
    hint_dirs = []
    if get_option('extenum_dir') != ''
      hint_dirs += meson.project_source_root() / get_option('extenum_dir')
    endif
    extenum_dep = [cpp.find_library(extenum_library, dirs: hint_dirs, required: true)]
  endif
  extenum_test = '''#include <fplll/enum/enumerate_ext_api.h>
extern extenum_fc_enumerate @0@;
int main() {
  extenum_fc_enumerate * volatile function = &@0@;
  return function == nullptr;
}
'''.format(extenum_function)
  if not cpp.links(
      extenum_test,
      args: ['-I' + meson.project_source_root(), '-I' + meson.project_build_root()],
      dependencies: extenum_dep + public_deps,
      name: 'external enumeration symbol ' + extenum_function)
    error('external enumeration symbol ' + extenum_function + ' was not found')
  endif
  private_deps += extenum_dep
endif

# Installed tools find the library relative to the configured final prefix.
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

subdir('src/fplll')
subdir('tools')

# Meson rewrites Darwin install names to absolute paths. Preserve the relative
# lookup encoded by install_rpath so staged installations remain runnable.
if host_machine.system() == 'darwin' and get_option('default_library') != 'static'
  installed_shared_lib = fplll_lib
  if get_option('default_library') == 'both'
    installed_shared_lib = fplll_lib.get_shared_lib()
  endif
  meson.add_install_script(
    import('python').find_installation(),
    files('meson/fix_darwin_install.py'),
    get_option('libdir'), get_option('bindir'), fs.name(installed_shared_lib.full_path()),
    find_program('install_name_tool').full_path(), find_program('codesign').full_path(),
    install_tag: 'runtime',
  )
endif

install_data(
  'strategies/default.json',
  install_dir: get_option('datadir') / 'fplll' / 'strategies',
)

if not meson.is_subproject()
  if get_option('tests')
    subdir('tests')
  endif
  subdir('doc')
endif

summary({
  'Build type': get_option('buildtype'),
  'Default library': get_option('default_library'),
  'Install prefix': get_option('prefix'),
  'nlohmann/json': nlohmann_json_dep.version(),
  'QD support': with_qd,
  'External enumeration': with_extenum,
  'Max enumeration dimension': max_enum_dim,
  'Max parallel enumeration dimension': max_parallel_enum_dim,
  'Tests': not meson.is_subproject() and get_option('tests'),
}, section: 'fplll')
