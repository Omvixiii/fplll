cmake_minimum_required(VERSION 3.25)

# Also support running this script before configuring or building the project.
if(NOT DEFINED FPLLL_SOURCE_DIR)
  get_filename_component(FPLLL_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
endif()
if(NOT EXISTS "${FPLLL_SOURCE_DIR}/.clang-format")
  message(FATAL_ERROR "No .clang-format found under FPLLL_SOURCE_DIR=${FPLLL_SOURCE_DIR}")
endif()
find_program(CLANG_FORMAT_EXECUTABLE NAMES clang-format REQUIRED)

# Discover files at invocation time, including test helpers and public headers.
# This list does not control compilation, so new files need no reconfiguration.
set(sources)
foreach(directory fplll tests)
  foreach(extension cpp h hpp inl)
    file(GLOB_RECURSE files LIST_DIRECTORIES FALSE
      "${FPLLL_SOURCE_DIR}/${directory}/*.${extension}")
    list(APPEND sources ${files})
  endforeach()
endforeach()
# Preserve the upstream formatting of bundled third-party headers.
list(REMOVE_ITEM sources
  "${FPLLL_SOURCE_DIR}/fplll/io/json.hpp"
  "${FPLLL_SOURCE_DIR}/fplll/io/thread_pool.hpp")
list(SORT sources)
if(NOT sources)
  message(FATAL_ERROR "No C++ files found under FPLLL_SOURCE_DIR=${FPLLL_SOURCE_DIR}")
endif()

set(arguments --Werror --fail-on-incomplete-format)
if(FPLLL_FORMAT)
  list(APPEND arguments -i)
else()
  list(APPEND arguments --dry-run)
  message(STATUS "Checking C++ formatting; use the format target to apply fixes")
endif()
execute_process(
  COMMAND "${CLANG_FORMAT_EXECUTABLE}"
    "--style=file:${FPLLL_SOURCE_DIR}/.clang-format" ${arguments} ${sources}
  COMMAND_ERROR_IS_FATAL ANY)
