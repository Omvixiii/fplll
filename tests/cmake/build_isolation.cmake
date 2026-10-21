cmake_minimum_required(VERSION 3.20)

# CTest gives this test its own binary-tree sandbox. Never operate on the
# checkout itself: even the intentionally rejected in-source configure may
# leave CMake's initial cache behind.
foreach(required FPLLL_SOURCE_DIR FPLLL_TEST_ROOT FPLLL_TEST_CXX_COMPILER
    FPLLL_PARENT_BINARY_DIR FPLLL_VERSION)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "${required} is required")
  endif()
endforeach()

# A previous run may have cached an older source layout (for example, the
# installed-consumer fixture's location). Always start this regression from a
# clean binary-tree sandbox, and reject paths outside CTest's work directory.
set(test_work_root "${FPLLL_PARENT_BINARY_DIR}/test-work")
cmake_path(IS_PREFIX test_work_root "${FPLLL_TEST_ROOT}" NORMALIZE in_test_work_tree)
if(NOT in_test_work_tree OR NOT FPLLL_TEST_ROOT MATCHES "/sandbox$")
  message(FATAL_ERROR "FPLLL_TEST_ROOT must be a sandbox under ${test_work_root}")
endif()
file(REMOVE_RECURSE "${FPLLL_TEST_ROOT}")
file(MAKE_DIRECTORY "${FPLLL_TEST_ROOT}")
set(source "${FPLLL_TEST_ROOT}/source")
set(binary "${FPLLL_TEST_ROOT}/binary")
set(prefix "${FPLLL_TEST_ROOT}/install")

function(run_step name)
  execute_process(COMMAND ${ARGN}
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
  file(WRITE "${FPLLL_TEST_ROOT}/${name}.log" "${output}\n${error}")
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "${name} failed (${result}):\n${output}\n${error}")
  endif()
endfunction()

function(snapshot result)
  file(GLOB_RECURSE paths LIST_DIRECTORIES FALSE RELATIVE "${source}" "${source}/*")
  set(hashes "")
  foreach(path IN LISTS paths)
    file(SHA256 "${source}/${path}" hash)
    string(APPEND hashes "${path}: ${hash}\n")
  endforeach()
  set(${result} "${hashes}" PARENT_SCOPE)
endfunction()

# The source distribution is the same artifact that users build on release.
run_step(package "${CMAKE_COMMAND}" --build "${FPLLL_PARENT_BINARY_DIR}"
  --target package_source)
set(archive "${FPLLL_PARENT_BINARY_DIR}/fplll-${FPLLL_VERSION}.tar.gz")
file(ARCHIVE_EXTRACT INPUT "${archive}" DESTINATION "${FPLLL_TEST_ROOT}/unpacked")
file(RENAME "${FPLLL_TEST_ROOT}/unpacked/fplll-${FPLLL_VERSION}" "${source}")
foreach(forbidden m4 configure.ac autogen.sh Makefile.am fplll/Makefile.am
    tests/Makefile.am fplll/fplll_config.h.in notes .git build _install)
  if(EXISTS "${source}/${forbidden}")
    message(FATAL_ERROR "Source archive contains forbidden entry: ${forbidden}")
  endif()
endforeach()

# Reject both the direct and symlink spellings of an in-source binary path.
set(rejected "${FPLLL_TEST_ROOT}/in-source")
file(MAKE_DIRECTORY "${rejected}")
configure_file("${source}/CMakeLists.txt" "${rejected}/CMakeLists.txt" COPYONLY)
execute_process(COMMAND "${CMAKE_COMMAND}" -S "${rejected}" -B "${rejected}"
  RESULT_VARIABLE rejected_result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(rejected_result EQUAL 0 OR NOT "${output}${error}" MATCHES "In-source builds are disabled")
  message(FATAL_ERROR "An in-source build was not rejected by the project guard")
endif()
file(CREATE_LINK "${rejected}" "${FPLLL_TEST_ROOT}/source-alias" SYMBOLIC RESULT link_result)
if(link_result STREQUAL "0")
  execute_process(COMMAND "${CMAKE_COMMAND}" -S "${rejected}"
    -B "${FPLLL_TEST_ROOT}/source-alias"
    RESULT_VARIABLE rejected_result OUTPUT_VARIABLE output ERROR_VARIABLE error)
  if(rejected_result EQUAL 0 OR NOT "${output}${error}" MATCHES "In-source builds are disabled")
    message(FATAL_ERROR "An in-source build through a symlink was not rejected")
  endif()
endif()

# Simulate a stale configuration header left by a different build system.
# Every real target must use the binary-tree header instead.
file(WRITE "${source}/fplll/fplll_config.h"
  "#error source fplll_config.h must never be included\n")
snapshot(before)
file(WRITE "${FPLLL_TEST_ROOT}/dependencies.cmake"
  "set(CMAKE_PREFIX_PATH [==[${FPLLL_TEST_PREFIX_PATH}]==] CACHE STRING \"\")\n")
run_step(configure "${CMAKE_COMMAND}" -S "${source}" -B "${binary}" -G Ninja
  -C "${FPLLL_TEST_ROOT}/dependencies.cmake"
  "-DCMAKE_CXX_COMPILER=${FPLLL_TEST_CXX_COMPILER}"
  "-DCMAKE_CXX_COMPILER_LAUNCHER=${FPLLL_TEST_CXX_LAUNCHER}"
  "-DCMAKE_INSTALL_PREFIX=${prefix}" -DCMAKE_INSTALL_LIBDIR=lib
  -DCMAKE_BUILD_TYPE=Debug -DFPLLL_MAX_PARALLEL_ENUM_DIM=20
  "-DFPLLL_WITH_QD=${FPLLL_TEST_WITH_QD}"
  -DBUILD_TESTING=ON -DFPLLL_BUILD_ISOLATION_TESTS=OFF)
run_step(build "${CMAKE_COMMAND}" --build "${binary}" --parallel 2)
run_step(variants "${CMAKE_COMMAND}" --build "${binary}" --parallel 2
  --target dbg fplllv fplllv_static fpllld_static)
run_step(test "${CMAKE_CTEST_COMMAND}" --test-dir "${binary}" --output-on-failure)
run_step(install "${CMAKE_COMMAND}" --install "${binary}")
file(WRITE "${FPLLL_TEST_ROOT}/consumer-prefix.cmake"
  "set(CMAKE_PREFIX_PATH [==[${prefix};${FPLLL_TEST_PREFIX_PATH}]==] CACHE STRING \"\" FORCE)\n")
run_step(consumer-configure "${CMAKE_COMMAND}"
  -S "${source}/tests/cmake/consumer" -B "${FPLLL_TEST_ROOT}/consumer" -G Ninja
  -C "${FPLLL_TEST_ROOT}/consumer-prefix.cmake"
  "-DCMAKE_CXX_COMPILER=${FPLLL_TEST_CXX_COMPILER}"
  "-DCMAKE_PREFIX_PATH=${prefix};${FPLLL_TEST_PREFIX_PATH}")
run_step(consumer-build "${CMAKE_COMMAND}"
  --build "${FPLLL_TEST_ROOT}/consumer" --parallel 2)
foreach(kind shared static)
  run_step("consumer-${kind}" "${FPLLL_TEST_ROOT}/consumer/fplll_consumer_${kind}")
endforeach()
snapshot(after)
if(NOT before STREQUAL after)
  message(FATAL_ERROR "Configure/build/test/install modified the source directory")
endif()
if(NOT EXISTS "${binary}/generated/enum-parallel/enumlib_dim.20.cpp")
  message(FATAL_ERROR "Enumeration source was not generated in the binary directory")
endif()
if(NOT EXISTS "${binary}/test-work/DEBUG/test_bkz/bkz-gso-dump.json")
  message(FATAL_ERROR "BKZ test output is missing from its binary-tree working directory")
endif()
