cmake_minimum_required(VERSION 3.20)

file(REAL_PATH "${CMAKE_CURRENT_SOURCE_DIR}" source_real)
file(REAL_PATH "${CMAKE_CURRENT_BINARY_DIR}" binary_real)
if(source_real STREQUAL binary_real)
  message(FATAL_ERROR "In-source builds are disabled. Use a separate -B binary directory.")
endif()

project(fplll_install_consumer LANGUAGES CXX)

find_package(fplll 5.5 CONFIG REQUIRED)

add_executable(fplll_consumer_shared consumer.cpp)
target_link_libraries(fplll_consumer_shared PRIVATE fplll::fplll)

add_executable(fplll_consumer_static consumer.cpp)
target_link_libraries(fplll_consumer_static PRIVATE fplll::fplll_static)
