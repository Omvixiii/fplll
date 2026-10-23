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
max_parallel_enum_dim = get_option('max_parallel_enum_dim').to_int()
if max_parallel_enum_dim >= max_enum_dim
  error('max_enum_dim must exceed max_parallel_enum_dim')
endif

# --- Dependencies ----------------------------------------------------------
threads_dep = dependency('threads')
gmp_dep = dependency('gmp', version: '>=4.2')
mpfr_dep = dependency('mpfr', version: '>=2.3')

public_deps = [gmp_dep, mpfr_dep, threads_dep]
private_deps = []

# Homebrew's mpfr.pc lists -lgmp without gmp's -L flag. Keep the gmp library
# directory on the link line on macOS so that -lgmp resolves.
if host_machine.system() == 'darwin'
  gmp_libdir = gmp_dep.get_variable(pkgconfig: 'libdir', default_value: '')
  if gmp_libdir != ''
    public_deps += declare_dependency(link_args: ['-L' + gmp_libdir])
  endif
endif

qd_dep = dependency('qd', required: get_option('qd'))
with_qd = qd_dep.found()
if with_qd
  public_deps += qd_dep
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
  if atomic_dep.length() == 0
    error('cannot link std::atomic with or without libatomic')
  endif
  private_deps += atomic_dep
endif

# --- Optional external enumeration library --------------------------------
extenum_library = get_option('extenum_library')
extenum_function = get_option('extenum_function')
with_extenum = extenum_library != ''
extenum_dep = []
if with_extenum
  if extenum_function == ''
    extenum_function = fs.name(extenum_library)
    if extenum_function.startswith('lib')
      extenum_function = extenum_function.substring(4)
    endif
    foreach suffix : ['.so', '.dylib', '.a']
      if extenum_function.endswith(suffix)
        extenum_function = extenum_function.substring(0, extenum_function.length() - suffix.length())
      endif
    endforeach
  endif
  if fs.is_absolute(extenum_library) or extenum_library.contains('/')
    extenum_dep = [declare_dependency(link_args: [extenum_library])]
  else
    hint_dirs = []
    if get_option('extenum_dir') != ''
      hint_dirs += get_option('extenum_dir')
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

subdir('fplll')
subdir('tools')

install_data(
  'strategies/default.json',
  install_dir: get_option('datadir') / 'fplll' / 'strategies',
)

if not meson.is_subproject()
  if get_option('tests')
    subdir('tests')
  endif
  if get_option('benchmarks')
    subdir('bench')
  endif
  subdir('doc')

  python3 = find_program('python3', required: false)
  clang_format = find_program('clang-format', required: false)
  if python3.found() and clang_format.found()
    format_script = files('scripts/format.py')
    run_target('format', command: [python3, format_script, clang_format, '--format'])
    run_target('lint', command: [python3, format_script, clang_format, '--check'])
  else
    message('clang-format or python3 not found; format and lint targets are unavailable')
  endif
endif
