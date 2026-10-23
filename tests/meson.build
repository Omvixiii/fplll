test_names = [
  'test_nr',
  'test_lll',
  'test_enum',
  'test_cvp',
  'test_svp',
  'test_bkz',
  'test_pruner',
  'test_gso',
  'test_lll_gram',
  'test_hlll',
  'test_svp_gram',
  'test_bkz_gram',
  'test_counter',
  'test_babai',
  'test_ceil',
]

llldiff = executable(
  'llldiff',
  'llldiff.cpp',
  dependencies: fplll_dep,
  install: false,
)

# Fixtures are read-only inputs from the source tree; tests write only into
# this build directory.
test_workdir = meson.current_build_dir()
foreach name : test_names
  test_exe = executable(
    name,
    name + '.cpp',
    dependencies: fplll_dep,
    cpp_args: ['-DTESTDATADIR="' + meson.project_source_root() + '/"'],
    install: false,
  )
  test(name, test_exe, workdir: test_workdir, timeout: 300)
endforeach
