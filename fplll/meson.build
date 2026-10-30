# First copy of the generated configuration header: build/fplll_config.h.
# This is the copy the root meson.build creates; see below for the second.
# The original sources include it as "fplll_config.h" (defs.h), as
# "../fplll_config.h" (enum/enumerate_base.h) and as
# <fplll/fplll_config.h> (enum-parallel/enumlib_dim.cpp), so the build
# produces both spellings instead of touching the sources.
#
# This copy in build/fplll serves the first two spellings; the copy in the
# build root produced by the top level serves the third one.
configure_file(
  input: '../meson/fplll_config.h.in',
  output: 'fplll_config.h',
  configuration: conf,
)

lib_sources = files(
  'fplll.cpp',
  'util.cpp',
  'enum/topenum.cpp',
  'enum/enumerate.cpp',
  'enum/enumerate_base.cpp',
  'enum/enumerate_ext.cpp',
  'enum/evaluator.cpp',
  'lll.cpp',
  'wrapper.cpp',
  'bkz.cpp',
  'bkz_param.cpp',
  'gso_interface.cpp',
  'gso_gram.cpp',
  'gso.cpp',
  'pruner/pruner.cpp',
  'householder.cpp',
  'hlll.cpp',
  'threadpool.cpp',
)

if max_parallel_enum_dim != 0
  lib_sources += files(
    'enum-parallel/enumlib.cpp',
    'enum-parallel/enumlib_dim.160.cpp',
    'enum-parallel/enumlib_dim.150.cpp',
    'enum-parallel/enumlib_dim.140.cpp',
    'enum-parallel/enumlib_dim.130.cpp',
    'enum-parallel/enumlib_dim.120.cpp',
    'enum-parallel/enumlib_dim.110.cpp',
    'enum-parallel/enumlib_dim.100.cpp',
    'enum-parallel/enumlib_dim.90.cpp',
    'enum-parallel/enumlib_dim.80.cpp',
    'enum-parallel/enumlib_dim.70.cpp',
    'enum-parallel/enumlib_dim.60.cpp',
    'enum-parallel/enumlib_dim.50.cpp',
    'enum-parallel/enumlib_dim.40.cpp',
    'enum-parallel/enumlib_dim.30.cpp',
    'enum-parallel/enumlib_dim.20.cpp',
  )
endif

lib_dependencies = [threads_dep, gmp_dep, mpfr_dep, qd_dep, extenum_dep]
library_link_args = []

libfplll = both_libraries(
  'fplll',
  lib_sources,
  include_directories: [root_inc, fplll_inc],
  dependencies: lib_dependencies,
  version: '9.0.0',
  soversion: '9',
  darwin_versions: ['10.0.0', '10.0.0'],
  install: true,
)

fplll_cli = executable(
  'fplll',
  'main.cpp',
  include_directories: [root_inc, fplll_inc],
  dependencies: lib_dependencies,
  link_with: libfplll,
  install: true,
)

latticegen_cli = executable(
  'latticegen',
  'latticegen.cpp',
  include_directories: [root_inc, fplll_inc],
  dependencies: lib_dependencies,
  link_with: libfplll,
  install: true,
)

llldiff_exe = executable(
  'llldiff',
  'llldiff.cpp',
  include_directories: [root_inc, fplll_inc],
  dependencies: lib_dependencies,
  link_with: libfplll,
  install: false,
)

# --- Installation ----------------------------------------------------------

includedir = get_option('includedir')

install_data(fplll_config_h, install_dir: includedir / 'fplll')

install_data(
  files(
    'defs.h',
    'fplll.h',
    'util.h',
    'svpcvp.h',
    'bkz.h',
    'lll.h',
    'gso_interface.h',
    'gso_gram.h',
    'gso.h',
    'wrapper.h',
    'bkz_param.h',
    'householder.h',
    'hlll.h',
    'threadpool.h',
  ),
  install_dir: includedir / 'fplll',
)

install_data(
  files(
    'nr/dpe.h',
    'nr/matrix.h',
    'nr/matrix.cpp',
    'nr/nr_FP_dd.inl',
    'nr/nr_FP_d.inl',
    'nr/nr_FP_dpe.inl',
    'nr/nr_FP.inl',
    'nr/nr_FP_ld.inl',
    'nr/nr_FP_misc.inl',
    'nr/nr_FP_mpfr.inl',
    'nr/nr_FP_qd.inl',
    'nr/nr.h',
    'nr/nr_rand.inl',
    'nr/nr_Z_d.inl',
    'nr/nr_Z.inl',
    'nr/nr_Z_l.inl',
    'nr/nr_Z_misc.inl',
    'nr/nr_Z_mpz.inl',
    'nr/numvect.h',
  ),
  install_dir: includedir / 'fplll' / 'nr',
)

install_data(
  files(
    'enum/evaluator.h',
    'enum/enumerate.h',
    'enum/enumerate_base.h',
    'enum/enumerate_ext.h',
    'enum/enumerate_ext_api.h',
  ),
  install_dir: includedir / 'fplll' / 'enum',
)

install_data(
  files(
    'pruner/pruner.h',
    'pruner/pruner_simplex.h',
  ),
  install_dir: includedir / 'fplll' / 'pruner',
)

install_data(
  files('io/thread_pool.hpp'),
  install_dir: includedir / 'fplll' / 'io',
)

# Autotools generates this forwarding header at install time.
install_data(
  '../meson/fplll.h',
  install_dir: includedir,
  rename: 'fplll.h',
)

# --- pkg-config ------------------------------------------------------------

pkg = import('pkgconfig')
pkg.generate(
  libfplll.get_shared_lib(),
  name: 'fplll',
  description: 'lattice algorithms with floating-point computations',
  version: meson.project_version(),
  requires: ['gmp >= 4.2', 'mpfr >= 2.3'],
  extra_cflags: ['-pthread'],
)
