cmake_minimum_required(VERSION 3.25)
project(fplll_consumer LANGUAGES CXX)

# The consumer intentionally does not set a C++ standard: it must inherit the
# standard requirement exported by the fplll target.
find_package(fplll CONFIG REQUIRED)

add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE fplll::fplll)
