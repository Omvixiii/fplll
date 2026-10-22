# Keep test targets and CTest registrations beside their sources. CMake's
# CMakeFiles/ and CTestTestfile.cmake in this directory's binary counterpart
# are generated metadata, not source files.
add_executable(llldiff "${PROJECT_SOURCE_DIR}/fplll/llldiff.cpp")
target_link_libraries(llldiff PRIVATE fplll)

set(FPLLL_TESTS
  test_nr
  test_lll
  test_enum
  test_cvp
  test_svp
  test_bkz
  test_pruner
  test_gso
  test_lll_gram
  test_hlll
  test_svp_gram
  test_bkz_gram
  test_counter
  test_babai
  test_ceil)

foreach(test_name IN LISTS FPLLL_TESTS)
  add_executable(${test_name} "${test_name}.cpp")
  set(test_work_dir
    "${PROJECT_BINARY_DIR}/test-work/$<UPPER_CASE:$<CONFIG>>/${test_name}")
  file(GENERATE OUTPUT "${test_work_dir}/.keep" CONTENT "")
  target_compile_definitions(${test_name} PRIVATE
    "TESTDATADIR=\"${PROJECT_SOURCE_DIR}/\"")
  target_link_libraries(${test_name} PRIVATE fplll)
  add_test(NAME ${test_name} COMMAND ${test_name})
  set_tests_properties(${test_name} PROPERTIES
    WORKING_DIRECTORY "${test_work_dir}")
endforeach()

if(FPLLL_BUILD_ISOLATION_TESTS)
  set(isolation_work_dir
    "${PROJECT_BINARY_DIR}/test-work/$<UPPER_CASE:$<CONFIG>>/build-isolation")
  file(GENERATE OUTPUT "${isolation_work_dir}/.keep" CONTENT "")
  add_test(NAME test_build_isolation
    COMMAND "${CMAKE_COMMAND}"
      "-DFPLLL_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
      "-DFPLLL_TEST_ROOT=${isolation_work_dir}/sandbox"
      "-DFPLLL_TEST_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
      "-DFPLLL_TEST_CXX_LAUNCHER=${CMAKE_CXX_COMPILER_LAUNCHER}"
      "-DFPLLL_TEST_WITH_QD=${FPLLL_ISOLATION_WITH_QD}"
      "-DFPLLL_TEST_PREFIX_PATH=${CMAKE_PREFIX_PATH}"
      "-DFPLLL_PARENT_BINARY_DIR=${PROJECT_BINARY_DIR}"
      "-DFPLLL_PROJECT_VERSION=${PROJECT_VERSION}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/build_isolation.cmake")
  set_tests_properties(test_build_isolation PROPERTIES
    WORKING_DIRECTORY "${isolation_work_dir}"
    LABELS "build-system"
    TIMEOUT 1200
    RUN_SERIAL TRUE)
endif()
