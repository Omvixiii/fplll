# Keep test targets and CTest registrations beside their sources. CMake's
# CMakeFiles/ and CTestTestfile.cmake in this directory's binary counterpart
# are generated metadata, not source files.
add_executable(llldiff llldiff.cpp)
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
    WORKING_DIRECTORY "${test_work_dir}"
    TIMEOUT 300)
endforeach()
