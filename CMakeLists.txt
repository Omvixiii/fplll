cmake_minimum_required(VERSION 3.25)

# Resolve aliases as well as literal paths, including use via add_subdirectory().
file(REAL_PATH "${CMAKE_CURRENT_SOURCE_DIR}" FPLLL_SOURCE_DIR_REAL)
file(REAL_PATH "${CMAKE_CURRENT_BINARY_DIR}" FPLLL_BINARY_DIR_REAL)
if(FPLLL_SOURCE_DIR_REAL STREQUAL FPLLL_BINARY_DIR_REAL)
  message(FATAL_ERROR
    "In-source builds are disabled. Configure with a CMake Preset or pass a separate -B binary directory.")
endif()

project(fplll VERSION 5.5.0 LANGUAGES CXX)

# C++23 is the project baseline. Embedders may request a newer standard; the
# target-level feature below rejects older ones with a clear configure error.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)

# Product version and the Libtool 9:0:0 ABI are independent.
set(FPLLL_ABI_VERSION 9.0.0)
set(FPLLL_SOVERSION 9)
set(FPLLL_MACHO_VERSION 10.0.0)

# Multi-config generators append their own configuration directory.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib")

include(GNUInstallDirs)
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")
find_package(Threads REQUIRED)

# Optional strict warnings for project sources. Vendored headers are included
# through normal include paths and are not part of the warning policy.
if(FPLLL_WARNINGS)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor
      -Woverloaded-virtual)
  endif()
  if(FPLLL_WARNINGS_AS_ERRORS)
    add_compile_options(-Werror)
  endif()
endif()

option(FPLLL_WITH_QD "Enable quad-double support when libqd is available" OFF)
option(FPLLL_WARNINGS "Enable the project's strict warnings" OFF)
option(FPLLL_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(FPLLL_BUILD_BENCHMARKS "Build the microbenchmark executable" OFF)
set(FPLLL_MAX_ENUM_DIM 256 CACHE STRING "Maximum supported enumeration dimension")
set(FPLLL_MAX_PARALLEL_ENUM_DIM 120 CACHE STRING
  "Maximum parallel enumeration dimension (0 disables parallel enumeration)")
set(FPLLL_EXTENUM_DIR "" CACHE PATH "External enumeration library prefix")
set(FPLLL_EXTENUM_LIBRARY "" CACHE STRING "External enumeration library name or path")
set(FPLLL_EXTENUM_FUNCTION "" CACHE STRING "External enumeration function symbol")

if(NOT FPLLL_MAX_ENUM_DIM MATCHES "^[1-9][0-9]*$")
  message(FATAL_ERROR "FPLLL_MAX_ENUM_DIM must be a positive integer")
endif()
if(NOT FPLLL_MAX_PARALLEL_ENUM_DIM MATCHES
    "^(0|20|30|40|50|60|70|80|90|100|110|120|130|140|150|160)$")
  message(FATAL_ERROR
    "FPLLL_MAX_PARALLEL_ENUM_DIM must be 0 or a multiple of 10 from 20 to 160")
endif()
if(FPLLL_MAX_PARALLEL_ENUM_DIM GREATER_EQUAL FPLLL_MAX_ENUM_DIM)
  message(FATAL_ERROR "FPLLL_MAX_ENUM_DIM must exceed FPLLL_MAX_PARALLEL_ENUM_DIM")
endif()

find_package(GMP REQUIRED)
set(FPLLL_INTEGER_TARGET GMP::GMP)
set(FPLLL_INTEGER_LIBRARY "${GMP_LIBRARY}")
set(FPLLL_INTEGER_HEADER gmp.h)

find_package(MPFR REQUIRED)

# find_path()/find_library() cannot tell whether headers and libraries belong to
# the same working installation, and version macros are only checked once here.
include(CheckCXXSourceCompiles)
include(CMakePushCheckState)
set(_FPLLL_INTEGER_VERSION_CHECK
"#if __GNU_MP_VERSION * 10000 + __GNU_MP_VERSION_MINOR * 100 + __GNU_MP_VERSION_PATCHLEVEL < 40200
#error \"GMP >= 4.2 is required\"
#endif")
cmake_push_check_state(RESET)
set(CMAKE_REQUIRED_INCLUDES "${GMP_INCLUDE_DIR}" "${MPFR_INCLUDE_DIR}")
set(CMAKE_REQUIRED_LIBRARIES "${MPFR_LIBRARY}" "${FPLLL_INTEGER_LIBRARY}")
check_cxx_source_compiles("
#include <${FPLLL_INTEGER_HEADER}>
#include <mpfr.h>
${_FPLLL_INTEGER_VERSION_CHECK}
#ifdef MPFR_VERSION_MAJOR
#if MPFR_VERSION_MAJOR * 100 + MPFR_VERSION_MINOR < 203
#error \"MPFR >= 2.3 is required\"
#endif
#endif
int main() {
  mpz_t z;
  mpz_init(z);
  mpz_clear(z);
  mpfr_t f;
  mpfr_init2(f, 53);
  mpfr_clear(f);
  return 0;
}" FPLLL_NUMERIC_DEPS_LINK)
cmake_pop_check_state()
unset(_FPLLL_INTEGER_VERSION_CHECK)
if(NOT FPLLL_NUMERIC_DEPS_LINK)
  message(FATAL_ERROR
    "GMP/MPFR headers and libraries are not a matching working installation. "
    "See CMakeError.log, fix the dependency paths, and reconfigure with "
    "-U FPLLL_NUMERIC_DEPS_LINK to run the check again.")
endif()

set(FPLLL_QD_TARGET "")
set(FPLLL_QD_INCLUDE_DIRS "")
if(FPLLL_WITH_QD)
  find_package(QD REQUIRED)
  set(FPLLL_QD_TARGET QD::QD)
  set(FPLLL_QD_INCLUDE_DIRS "${QD_INCLUDE_DIRS}")
endif()

set(FPLLL_EXTENUM_LIBRARY_PATH "")
set(FPLLL_EXTENUM_TARGET "")
set(FPLLL_HAS_EXTENUM OFF)
if(FPLLL_EXTENUM_LIBRARY)
  if(NOT FPLLL_EXTENUM_FUNCTION)
    set(FPLLL_EXTENUM_FUNCTION "${FPLLL_EXTENUM_LIBRARY}")
  endif()
  if(NOT FPLLL_EXTENUM_FUNCTION MATCHES "^[A-Za-z_][A-Za-z0-9_]*$")
    message(FATAL_ERROR "FPLLL_EXTENUM_FUNCTION must be a C++ identifier")
  endif()
  set(FplllExtEnum_NAME "${FPLLL_EXTENUM_LIBRARY}")
  find_package(FplllExtEnum REQUIRED)
  set(FPLLL_EXTENUM_LIBRARY_PATH "${FplllExtEnum_LIBRARY}")

  include(CheckCXXSourceCompiles)
  set(CMAKE_REQUIRED_INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}")
  set(CMAKE_REQUIRED_LIBRARIES "${FPLLL_EXTENUM_LIBRARY_PATH}"
    MPFR::MPFR "${FPLLL_INTEGER_TARGET}" "${FPLLL_QD_TARGET}" Threads::Threads)
  check_cxx_source_compiles(
    "#include <fplll/enum/enumerate_ext_api.h>
     extern extenum_fc_enumerate ${FPLLL_EXTENUM_FUNCTION};
     int main() { extenum_fc_enumerate * volatile function = &${FPLLL_EXTENUM_FUNCTION}; return function == nullptr; }"
    FPLLL_EXTENUM_LINKS)
  unset(CMAKE_REQUIRED_INCLUDES)
  unset(CMAKE_REQUIRED_LIBRARIES)
  if(NOT FPLLL_EXTENUM_LINKS)
    message(FATAL_ERROR "External enumeration symbol ${FPLLL_EXTENUM_FUNCTION} was not found")
  endif()
  set(FPLLL_EXTENUM_TARGET FPLLL::ExtEnum)
  set(FPLLL_EXTENUM_FUNC "${FPLLL_EXTENUM_FUNCTION}")
  set(FPLLL_HAS_EXTENUM ON)
endif()

include(CheckCXXSourceCompiles)
set(_FPLLL_ATOMIC_TEST_SOURCE [=[
  #include <atomic>
  #include <cstdint>
  int main() {
    std::atomic<int64_t> value{};
    return static_cast<int>(value.fetch_add(1));
  }
]=])
check_cxx_source_compiles("${_FPLLL_ATOMIC_TEST_SOURCE}" FPLLL_ATOMIC_LINKS_WITHOUT_LIB)
set(FPLLL_ATOMIC_LIBRARY "")
if(NOT FPLLL_ATOMIC_LINKS_WITHOUT_LIB)
  set(CMAKE_REQUIRED_LIBRARIES atomic)
  check_cxx_source_compiles("${_FPLLL_ATOMIC_TEST_SOURCE}" FPLLL_ATOMIC_LINKS_WITH_LIB)
  unset(CMAKE_REQUIRED_LIBRARIES)
  if(NOT FPLLL_ATOMIC_LINKS_WITH_LIB)
    message(FATAL_ERROR "Could not link std::atomic with or without libatomic")
  endif()
  set(FPLLL_ATOMIC_LIBRARY atomic)
endif()

include(cmake/configuration.cmake)
fplll_generate_configuration()

set(FPLLL_LIBRARY_SOURCES
  fplll/svpcvp.cpp
  fplll/util.cpp
  fplll/enum/enumerate.cpp
  fplll/enum/enumerate_base.cpp
  fplll/enum/enumerate_ext.cpp
  fplll/enum/evaluator.cpp
  fplll/lll.cpp
  fplll/wrapper.cpp
  fplll/bkz.cpp
  fplll/bkz_param.cpp
  fplll/gso_interface.cpp
  fplll/gso_gram.cpp
  fplll/gso.cpp
  fplll/pruner/pruner.cpp
  fplll/householder.cpp
  fplll/hlll.cpp
  fplll/threadpool.cpp
  ${FPLLL_PARALLEL_ENUM_SOURCES})

set(FPLLL_DEFAULT_STRATEGY_PATH
  "${CMAKE_INSTALL_FULL_DATADIR}/fplll/strategies")
# Only bkz_param.cpp reads these values. An install-prefix change should not
# recompile every library translation unit.
set_property(SOURCE fplll/bkz_param.cpp APPEND PROPERTY COMPILE_DEFINITIONS
  "FPLLL_DEFAULT_STRATEGY_PATH=\"${FPLLL_DEFAULT_STRATEGY_PATH}\""
  "FPLLL_DEFAULT_STRATEGY=\"${FPLLL_DEFAULT_STRATEGY_PATH}/default.json\"")
function(fplll_configure_library target)
  target_include_directories(${target}
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
      $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
  target_compile_features(${target} PUBLIC cxx_std_23)
  target_link_libraries(${target} PRIVATE "${FPLLL_EXTENUM_TARGET}")
  target_link_libraries(${target} PUBLIC
    MPFR::MPFR
    "${FPLLL_INTEGER_TARGET}"
    Threads::Threads
    "${FPLLL_QD_TARGET}"
    "${FPLLL_ATOMIC_LIBRARY}")
endfunction()

add_library(fplll_objects OBJECT ${FPLLL_LIBRARY_SOURCES})
set_target_properties(fplll_objects PROPERTIES POSITION_INDEPENDENT_CODE ON)
fplll_configure_library(fplll_objects)

add_library(fplll SHARED $<TARGET_OBJECTS:fplll_objects>)
add_library(fplll::fplll ALIAS fplll)
set_target_properties(fplll PROPERTIES
  VERSION "${FPLLL_ABI_VERSION}"
  SOVERSION "${FPLLL_SOVERSION}"
  MACHO_COMPATIBILITY_VERSION "${FPLLL_MACHO_VERSION}"
  MACHO_CURRENT_VERSION "${FPLLL_MACHO_VERSION}")
fplll_configure_library(fplll)

add_library(fplll_static STATIC $<TARGET_OBJECTS:fplll_objects>)
add_library(fplll::fplll_static ALIAS fplll_static)
set_target_properties(fplll_static PROPERTIES
  OUTPUT_NAME fplll
  EXPORT_NAME fplll_static
  POSITION_INDEPENDENT_CODE ON)
fplll_configure_library(fplll_static)

# Optional variant/debug targets for downstream packagers and developers.
add_library(fplllv SHARED EXCLUDE_FROM_ALL $<TARGET_OBJECTS:fplll_objects>)
set_target_properties(fplllv PROPERTIES
  VERSION "${FPLLL_ABI_VERSION}"
  SOVERSION "${FPLLL_SOVERSION}"
  MACHO_COMPATIBILITY_VERSION "${FPLLL_MACHO_VERSION}"
  MACHO_CURRENT_VERSION "${FPLLL_MACHO_VERSION}")
fplll_configure_library(fplllv)

add_library(fplll_debug_objects OBJECT EXCLUDE_FROM_ALL ${FPLLL_LIBRARY_SOURCES})
set_target_properties(fplll_debug_objects PROPERTIES POSITION_INDEPENDENT_CODE ON)
fplll_configure_library(fplll_debug_objects)
target_compile_definitions(fplll_debug_objects PRIVATE DEBUG)

add_library(fpllld SHARED EXCLUDE_FROM_ALL $<TARGET_OBJECTS:fplll_debug_objects>)
set_target_properties(fpllld PROPERTIES
  VERSION "${FPLLL_ABI_VERSION}"
  SOVERSION "${FPLLL_SOVERSION}"
  MACHO_COMPATIBILITY_VERSION "${FPLLL_MACHO_VERSION}"
  MACHO_CURRENT_VERSION "${FPLLL_MACHO_VERSION}")
fplll_configure_library(fpllld)
target_compile_definitions(fpllld PUBLIC DEBUG)

add_library(fpllld_static STATIC EXCLUDE_FROM_ALL $<TARGET_OBJECTS:fplll_debug_objects>)
set_target_properties(fpllld_static PROPERTIES
  OUTPUT_NAME fpllld
  EXPORT_NAME fpllld_static
  POSITION_INDEPENDENT_CODE ON)
fplll_configure_library(fpllld_static)
target_compile_definitions(fpllld_static PUBLIC DEBUG)

add_library(fplllv_static STATIC EXCLUDE_FROM_ALL $<TARGET_OBJECTS:fplll_objects>)
set_target_properties(fplllv_static PROPERTIES
  OUTPUT_NAME fplllv
  EXPORT_NAME fplllv_static
  POSITION_INDEPENDENT_CODE ON)
fplll_configure_library(fplllv_static)

# Command-line programs; tools/ also owns their debug variants and the dbg target.
add_subdirectory(tools)

if(PROJECT_IS_TOP_LEVEL)
  include(CTest)
  if(BUILD_TESTING)
    add_subdirectory(tests)
  endif()

  add_custom_target(lint
    COMMAND "${CMAKE_COMMAND}" "-DFPLLL_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/lints.cmake"
    USES_TERMINAL
    VERBATIM)
  add_custom_target(format
    COMMAND "${CMAKE_COMMAND}" "-DFPLLL_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
      -DFPLLL_FORMAT=ON -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/lints.cmake"
    USES_TERMINAL
    VERBATIM)

  include(cmake/documentation.cmake)
  fplll_add_documentation()

  if(FPLLL_BUILD_BENCHMARKS)
    add_subdirectory(bench)
  endif()

  # Full release lifecycle check; the script reads the parent configuration
  # from this build tree's CMakeCache.txt.
  add_custom_target(distcheck
    COMMAND "${CMAKE_COMMAND}"
      "-DFPLLL_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}"
      "-DFPLLL_GENERATOR=${CMAKE_GENERATOR}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/distcheck.cmake"
    USES_TERMINAL
    VERBATIM)
endif()

include(cmake/packaging.cmake)
fplll_configure_packaging()
