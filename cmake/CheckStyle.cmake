cmake_minimum_required(VERSION 3.20)
find_program(CLANG_FORMAT_EXECUTABLE NAMES clang-format REQUIRED)
file(GLOB_RECURSE sources
  "${FPLLL_SOURCE_DIR}/fplll/*.cpp"
  "${FPLLL_SOURCE_DIR}/fplll/*.h"
  "${FPLLL_SOURCE_DIR}/fplll/*.inl"
  "${FPLLL_SOURCE_DIR}/tests/*.cpp")
if(FPLLL_FORMAT)
  set(arguments -i)
else()
  set(arguments --dry-run --Werror)
endif()
execute_process(COMMAND "${CLANG_FORMAT_EXECUTABLE}" --style=file ${arguments} ${sources}
  RESULT_VARIABLE result)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "Formatting failed; use the format target to apply clang-format")
endif()
