# Microbenchmarks for the library hot paths.
#
# Built only when FPLLL_BUILD_BENCHMARKS=ON. The executable prints one JSON
# object per measured case so external scripts can compare runs across
# revisions without parsing free-form text.

add_executable(fplll_bench bench.cpp)
target_link_libraries(fplll_bench PRIVATE fplll)
