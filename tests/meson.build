test_programs = {
  'test_nr': 'test_nr.cpp',
  'test_lll': 'test_lll.cpp',
  'test_enum': 'test_enum.cpp',
  'test_cvp': 'test_cvp.cpp',
  'test_svp': 'test_svp.cpp',
  'test_bkz': 'test_bkz.cpp',
  'test_pruner': 'test_pruner.cpp',
  'test_gso': 'test_gso.cpp',
  'test_lll_gram': 'test_lll_gram.cpp',
  'test_hlll': 'test_hlll.cpp',
  'test_svp_gram': 'test_svp_gram.cpp',
  'test_bkz_gram': 'test_bkz_gram.cpp',
  'test_counter': 'test_counter.cpp',
  'test_babai': 'test_babai.cpp',
  'test_ceil': 'test_ceil.cpp',
}

# The tests resolve their data files through TESTDATADIR, which the
# Autotools build points at the source root.
test_args = ['-DTESTDATADIR="' + meson.project_source_root() + '/"']

foreach name, source : test_programs
  test_exe = executable(
    name,
    source,
    include_directories: [root_inc, fplll_inc],
    dependencies: lib_dependencies,
    link_with: libfplll,
    cpp_args: test_args,
  )
  test(name, test_exe, timeout: 600)
endforeach
