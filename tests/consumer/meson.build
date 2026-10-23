project(
    'fplll-consumer',
    'cpp',
    default_options: ['cpp_std=c++23'],
)

# pkg-config is the downstream interface; CI points PKG_CONFIG_PATH at the
# installed prefix. pkg-config does not carry a C++ standard, so the consumer
# requests C++23 itself via default_options above.
fplll_dep = dependency('fplll', required: true)

executable(
    'consumer',
    'main.cpp',
    dependencies: fplll_dep,
    install: false,
)
