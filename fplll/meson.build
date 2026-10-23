fplll_inc = include_directories('..')

# --- Public configuration header ------------------------------------------
configuration = configuration_data()
configuration.set('FPLLL_MAJOR_VERSION', version_parts[0])
configuration.set('FPLLL_MINOR_VERSION', version_parts[1])
configuration.set('FPLLL_MICRO_VERSION', version_parts[2])
configuration.set('FPLLL_VERSION', meson.project_version())
configuration.set('FPLLL_WITH_QD', with_qd)
configuration.set('FPLLL_MAX_ENUM_DIM', max_enum_dim)
configuration.set('FPLLL_MAX_PARALLEL_ENUM_DIM', max_parallel_enum_dim)
if with_extenum
  configuration.set('FPLLL_EXTENUM_DEFINE', '#define FPLLL_EXTENUM_FUNC ' + extenum_function)
else
  configuration.set('FPLLL_EXTENUM_DEFINE', '/* External enumeration is disabled. */')
endif
fplll_config_h = configure_file(
  input: '../meson/fplll_config.h.in',
  output: 'fplll_config.h',
  configuration: configuration,
)

# --- Generated parallel-enumeration translation units ---------------------
parallel_enum_sources = []
if max_parallel_enum_dim > 0
  parallel_enum_sources += files('enum-parallel/enumlib.cpp')
  foreach dimension : range(20, max_parallel_enum_dim + 1, 10)
    parallel_enum_sources += configure_file(
      input: '../meson/enumlib_dim.cpp.in',
      output: 'enumlib_dim.@0@.cpp'.format(dimension),
      configuration: {'ENUMDIMENSION': dimension},
    )
  endforeach
endif

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
) + parallel_enum_sources

# Only this translation unit needs rebuilding when the configured prefix changes.
strategy_define_args = [
  '-DFPLLL_DEFAULT_STRATEGY_PATH="' + strategy_dir + '"',
  '-DFPLLL_DEFAULT_STRATEGY="' + strategy_dir / 'default.json' + '"',
]
bkz_param_object = static_library(
  'fplll_bkz_param',
  'bkz_param.cpp',
  include_directories: fplll_inc,
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
  include_directories: fplll_inc,
  dependencies: library_deps,
  version: fplll_libversion,
  soversion: fplll_soversion,
  darwin_versions: fplll_darwin_versions,
  install: true,
)

fplll_dep = declare_dependency(
  link_with: fplll_lib,
  include_directories: fplll_inc,
  dependencies: public_deps,
)
if get_option('default_library') == 'both'
  meson.override_dependency('fplll', fplll_dep.as_shared(), static: false)
  meson.override_dependency('fplll', fplll_dep.as_static(), static: true)
else
  meson.override_dependency('fplll', fplll_dep)
endif

# --- Optional variant libraries for packagers -----------------------------
# These are never built by default and never installed. Only fpllld enables
# fplll's DEBUG macro, independently of the compiler buildtype.
variant_sources = fplll_sources + files('bkz_param.cpp')
fplllv_lib = library(
  'fplllv',
  variant_sources,
  include_directories: fplll_inc,
  dependencies: library_deps,
  cpp_args: strategy_define_args,
  version: fplll_libversion,
  soversion: fplll_soversion,
  darwin_versions: fplll_darwin_versions,
  build_by_default: false,
  install: false,
)
fpllld_lib = library(
  'fpllld',
  variant_sources,
  include_directories: fplll_inc,
  dependencies: library_deps,
  cpp_args: strategy_define_args + ['-DDEBUG'],
  version: fplll_libversion,
  soversion: fplll_soversion,
  darwin_versions: fplll_darwin_versions,
  build_by_default: false,
  install: false,
)
alias_target('variants', fplllv_lib, fpllld_lib)

# --- pkg-config metadata --------------------------------------------------
pkgconfig = import('pkgconfig')
pc_requires = [mpfr_dep, gmp_dep]
if with_qd and qd_via_pkgconfig
  pc_requires += qd_dep
endif
# Private link flags (external enumeration and dependency paths) are
# derived automatically from the library's dependency closure.
pkgconfig.generate(
  fplll_lib,
  filebase: 'fplll',
  name: 'fplll',
  description: 'Lattice algorithms using floating-point arithmetic',
  version: meson.project_version(),
  requires: pc_requires,
  libraries: pc_public_libs,
  install_dir: get_option('libdir') / 'pkgconfig',
)

# --- Public headers -------------------------------------------------------
install_headers(
  files(
    'bkz.h',
    'bkz_param.h',
    'defs.h',
    'fplll.h',
    'gso.h',
    'gso_gram.h',
    'gso_interface.h',
    'hlll.h',
    'householder.h',
    'lll.h',
    'svpcvp.h',
    'threadpool.h',
    'util.h',
    'wrapper.h',
  ),
  subdir: 'fplll',
)
install_headers(
  files(
    'enum/enumerate.h',
    'enum/enumerate_base.h',
    'enum/enumerate_ext.h',
    'enum/enumerate_ext_api.h',
    'enum/evaluator.h',
  ),
  subdir: 'fplll/enum',
)
install_headers('io/thread_pool.hpp', subdir: 'fplll/io')
install_headers(
  files(
    'nr/matrix.h',
    'nr/matrix.inl',
    'nr/nr.h',
    'nr/nr_FP.inl',
    'nr/nr_FP_d.inl',
    'nr/nr_FP_dd.inl',
    'nr/nr_FP_ld.inl',
    'nr/nr_FP_misc.inl',
    'nr/nr_FP_mpfr.inl',
    'nr/nr_FP_qd.inl',
    'nr/nr_Z.inl',
    'nr/nr_Z_l.inl',
    'nr/nr_Z_misc.inl',
    'nr/nr_Z_mpz.inl',
    'nr/nr_rand.inl',
    'nr/numvect.h',
  ),
  subdir: 'fplll/nr',
)
install_headers(
  files(
    'pruner/pruner.h',
    'pruner/pruner_simplex.inl',
  ),
  subdir: 'fplll/pruner',
)
install_data(fplll_config_h, install_dir: get_option('includedir') / 'fplll')
install_data('../meson/fplll.h', install_dir: get_option('includedir'))
