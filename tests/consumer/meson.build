project(
    'fplll-consumer',
    'cpp',
    default_options: ['cpp_std=c++23'],
)

# pkg-config is the downstream interface; point PKG_CONFIG_PATH at the
# installed prefix. pkg-config does not carry a C++ standard, so the consumer
# requests C++23 itself via default_options above.
foreach kind : ['shared', 'static']
  fplll_dep = dependency('fplll', method: 'pkg-config', static: kind == 'static')
  consumer = executable('consumer-' + kind, 'main.cpp', dependencies: fplll_dep)
  test(kind, consumer)
endforeach
