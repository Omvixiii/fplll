test_names = [
  'test_nr',
  'test_lll',
  'test_enum',
  'test_cvp',
  'test_svp',
  'test_bkz',
  'test_pruner',
  'test_pruner_memory',
  'test_gso',
  'test_lll_gram',
  'test_hlll',
  'test_svp_gram',
  'test_bkz_gram',
  'test_counter',
  'test_babai',
  'test_ceil',
  'test_config',
  'test_public_macros',
  'test_regressions',
]

executable(
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

python3 = find_program('python3', required: false)
if python3.found()
  test(
    'cli',
    python3,
    args: [
      meson.current_source_dir() / 'test_cli.py',
      fplll_tool.full_path(),
      latticegen_tool.full_path(),
    ],
    workdir: test_workdir,
    timeout: 120,
  )
endif
