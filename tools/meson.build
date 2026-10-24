fplll_tool = executable(
  'fplll',
  'main.cpp',
  dependencies: fplll_dep,
  install: true,
)

latticegen_tool = executable(
  'latticegen',
  'latticegen.cpp',
  dependencies: fplll_dep,
  install: true,
)

# Debug variants reuse the same sources but link the DEBUG-enabled library.
fplll_dbg = executable(
  'fplll_dbg',
  'main.cpp',
  include_directories: public_inc,
  dependencies: public_deps,
  link_with: fpllld_lib,
  cpp_args: ['-DDEBUG'],
  build_by_default: false,
  install: false,
)
latticegen_dbg = executable(
  'latticegen_dbg',
  'latticegen.cpp',
  include_directories: public_inc,
  dependencies: public_deps,
  link_with: fpllld_lib,
  cpp_args: ['-DDEBUG'],
  build_by_default: false,
  install: false,
)
alias_target('dbg', fplll_dbg, latticegen_dbg)
