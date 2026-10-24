# --- Library sources ------------------------------------------------------
fplll_sources = files(
  'bkz.cpp',
  'gso.cpp',
  'gso_gram.cpp',
  'gso_interface.cpp',
  'hlll.cpp',
  'householder.cpp',
  'lll.cpp',
  'svpcvp.cpp',
  'threadpool.cpp',
  'util.cpp',
  'wrapper.cpp',
  'enum/enumerate.cpp',
  'enum/enumerate_base.cpp',
  'enum/enumerate_ext.cpp',
  'enum/evaluator.cpp',
  'pruner/pruner.cpp',
)

# --- Generated parallel-enumeration translation units ---------------------
if max_parallel_enum_dim > 0
  fplll_sources += files('enum-parallel/enumlib.cpp')
  foreach dimension : range(20, max_parallel_enum_dim + 1, 10)
    fplll_sources += configure_file(
      input: '../meson/enumlib_dim.cpp.in',
      output: 'enumlib_dim.@0@.cpp'.format(dimension),
      configuration: {'ENUMDIMENSION': dimension},
    )
  endforeach
endif

# Only this translation unit needs rebuilding when the configured prefix changes.
strategy_define_args = [
  '-DFPLLL_DEFAULT_STRATEGY_PATH="' + default_strategy_dir + '"',
  '-DFPLLL_DEFAULT_STRATEGY="' + default_strategy_dir / 'default.json' + '"',
]
bkz_param_object = static_library(
  'fplll_bkz_param',
  'bkz_param.cpp',
  include_directories: public_inc,
  dependencies: public_deps + private_deps,
  cpp_args: strategy_define_args,
  pic: true,
  build_by_default: false,
  install: false,
)

library_deps = public_deps + private_deps
fplll_lib = library(
  'fplll',
  fplll_sources,
  objects: bkz_param_object.extract_all_objects(recursive: true),
  include_directories: public_inc,
  dependencies: library_deps,
  cpp_args: extenum_cpp_args,
  version: fplll_libversion,
  soversion: fplll_soversion,
  darwin_versions: fplll_darwin_versions,
  install: true,
)

fplll_dep = declare_dependency(
  link_with: fplll_lib,
  include_directories: public_inc,
  dependencies: public_deps,
)
if get_option('default_library') == 'both'
  meson.override_dependency('fplll', fplll_dep.as_shared(), static: false)
  meson.override_dependency('fplll', fplll_dep.as_static(), static: true)
else
  meson.override_dependency('fplll', fplll_dep)
endif

# --- Optional variant libraries for packagers -----------------------------
variant_sources = fplll_sources + files('bkz_param.cpp')
fplllv_lib = library(
  'fplllv',
  variant_sources,
  include_directories: public_inc,
  dependencies: library_deps,
  cpp_args: strategy_define_args + extenum_cpp_args,
  version: fplll_libversion,
  soversion: fplll_soversion,
  darwin_versions: fplll_darwin_versions,
  build_by_default: false,
  install: false,
)
fpllld_lib = library(
  'fpllld',
  variant_sources,
  include_directories: public_inc,
  dependencies: library_deps,
  cpp_args: strategy_define_args + ['-DDEBUG'] + extenum_cpp_args,
  version: fplll_libversion,
  soversion: fplll_soversion,
  darwin_versions: fplll_darwin_versions,
  build_by_default: false,
  install: false,
)
alias_target('variants', fplllv_lib, fpllld_lib)

# --- pkg-config metadata --------------------------------------------------
# Public headers expose GMP, MPFR and QD types, so they are public requirements.
pkgconfig = import('pkgconfig')
pc_requires = [mpfr_dep, gmp_dep]
if qd_dep.found()
  pc_requires += qd_dep
endif
pkgconfig.generate(
  fplll_lib,
  filebase: 'fplll',
  name: 'fplll',
  description: 'Lattice algorithms using floating-point arithmetic',
  version: meson.project_version(),
  requires: pc_requires,
  libraries: [threads_dep],
  install_dir: get_option('libdir') / 'pkgconfig',
)
