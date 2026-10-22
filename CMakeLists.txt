cmake_minimum_required(VERSION 3.20)

# Resolve aliases as well as literal paths, including use via add_subdirectory().
file(REAL_PATH "${CMAKE_CURRENT_SOURCE_DIR}" FPLLL_SOURCE_DIR_REAL)
file(REAL_PATH "${CMAKE_CURRENT_BINARY_DIR}" FPLLL_BINARY_DIR_REAL)
if(FPLLL_SOURCE_DIR_REAL STREQUAL FPLLL_BINARY_DIR_REAL)
  message(FATAL_ERROR
    "In-source builds are disabled. Configure with a CMake Preset or pass a separate -B binary directory.")
endif()

project(fplll VERSION 5.5.0 LANGUAGES CXX)
message(STATUS "fplll version: ${PROJECT_VERSION}")

# C++11 is the compatibility baseline. Presets and embedding projects may
# request a newer standard without this project silently overriding it.
if(NOT DEFINED CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 11)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)

# One layout for executables, tests and all library variants, including custom
# configurations and multi-config generators. Stay inside this project's binary
# directory even when embedded in a larger build.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY
  "${CMAKE_CURRENT_BINARY_DIR}/bin/$<UPPER_CASE:$<CONFIG>>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY
  "${CMAKE_CURRENT_BINARY_DIR}/lib/$<UPPER_CASE:$<CONFIG>>")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY
  "${CMAKE_CURRENT_BINARY_DIR}/lib/$<UPPER_CASE:$<CONFIG>>")

include(GNUInstallDirs)
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")
find_package(Threads REQUIRED)

option(FPLLL_USE_MPIR "Use MPIR instead of GMP" OFF)
option(FPLLL_WITH_QD "Enable quad-double support when libqd is available" OFF)
option(FPLLL_WITH_RECURSIVE_ENUM "Enable recursive enumeration" ON)
set(FPLLL_MPIR_ROOT "" CACHE PATH "MPIR installation prefix")
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

set(HAVE_LIBGMP 0)
set(HAVE_LIBMPIR 0)
if(FPLLL_USE_MPIR)
  find_package(MPIR REQUIRED)
  set(GMP_INCLUDE_DIR "${MPIR_GMP_INCLUDE_DIR}")
  set(FPLLL_INTEGER_TARGET MPIR::MPIR)
  set(FPLLL_INTEGER_LIBRARY_NAME mpir)
  set(HAVE_LIBMPIR 1)
else()
  find_package(GMP REQUIRED)
  set(FPLLL_INTEGER_TARGET GMP::GMP)
  set(FPLLL_INTEGER_LIBRARY_NAME gmp)
  set(HAVE_LIBGMP 1)
endif()

find_package(MPFR REQUIRED)
set(HAVE_LIBMPFR 1)

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
  set(CMAKE_REQUIRED_LIBRARIES "${FPLLL_EXTENUM_LIBRARY_PATH}")
  check_cxx_source_compiles(
    "#include <fplll/enum/enumerate_ext_api.h>
     extern extenum_fc_enumerate ${FPLLL_EXTENUM_FUNCTION};
     int main() { auto function = &${FPLLL_EXTENUM_FUNCTION}; (void)function; return 0; }"
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
  fplll/fplll.cpp
  fplll/util.cpp
  fplll/enum/topenum.cpp
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
  fplll/pruner/pruner_cost.cpp
  fplll/pruner/pruner_optimize.cpp
  fplll/pruner/pruner_optimize_tc.cpp
  fplll/pruner/pruner_optimize_tp.cpp
  fplll/pruner/pruner_prob.cpp
  fplll/pruner/pruner_util.cpp
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
      $<BUILD_INTERFACE:${FPLLL_CONFIG_INCLUDE_DIR}>
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
      $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
      $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/fplll>
    PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/fplll"
      "${CMAKE_CURRENT_SOURCE_DIR}/fplll/enum-parallel")
  target_compile_features(${target} PUBLIC cxx_std_11)
  target_link_libraries(${target} PUBLIC
    MPFR::MPFR
    "${FPLLL_INTEGER_TARGET}"
    Threads::Threads
    "${FPLLL_QD_TARGET}"
    "${FPLLL_EXTENUM_TARGET}"
    "${FPLLL_ATOMIC_LIBRARY}")
endfunction()

add_library(fplll_objects OBJECT ${FPLLL_LIBRARY_SOURCES})
set_target_properties(fplll_objects PROPERTIES POSITION_INDEPENDENT_CODE ON)
fplll_configure_library(fplll_objects)

add_library(fplll SHARED $<TARGET_OBJECTS:fplll_objects>)
add_library(fplll::fplll ALIAS fplll)
set_target_properties(fplll PROPERTIES VERSION "${PROJECT_VERSION}" SOVERSION 9)
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
set_target_properties(fplllv PROPERTIES VERSION "${PROJECT_VERSION}" SOVERSION 9)
fplll_configure_library(fplllv)

add_library(fplll_debug_objects OBJECT EXCLUDE_FROM_ALL ${FPLLL_LIBRARY_SOURCES})
set_target_properties(fplll_debug_objects PROPERTIES POSITION_INDEPENDENT_CODE ON)
fplll_configure_library(fplll_debug_objects)
target_compile_definitions(fplll_debug_objects PRIVATE DEBUG)

add_library(fpllld SHARED EXCLUDE_FROM_ALL $<TARGET_OBJECTS:fplll_debug_objects>)
set_target_properties(fpllld PROPERTIES VERSION "${PROJECT_VERSION}" SOVERSION 9)
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

add_executable(fplll_cli fplll/main.cpp)
set_target_properties(fplll_cli PROPERTIES
  OUTPUT_NAME fplll)
target_link_libraries(fplll_cli PRIVATE fplll)

add_executable(latticegen fplll/latticegen.cpp)
target_link_libraries(latticegen PRIVATE fplll)

# Installed tools must find fplll under a custom or relocated install prefix.
if(APPLE OR UNIX)
  if(IS_ABSOLUTE "${CMAKE_INSTALL_LIBDIR}")
    set(FPLLL_TOOL_INSTALL_RPATH "${CMAKE_INSTALL_LIBDIR}")
  else()
    file(RELATIVE_PATH FPLLL_BINDIR_TO_LIBDIR
      "${CMAKE_INSTALL_FULL_BINDIR}" "${CMAKE_INSTALL_FULL_LIBDIR}")
    if(APPLE)
      set(FPLLL_TOOL_INSTALL_RPATH "@loader_path/${FPLLL_BINDIR_TO_LIBDIR}")
    else()
      set(FPLLL_TOOL_INSTALL_RPATH "$ORIGIN/${FPLLL_BINDIR_TO_LIBDIR}")
    endif()
  endif()
  set_target_properties(fplll_cli latticegen PROPERTIES
    INSTALL_RPATH "${FPLLL_TOOL_INSTALL_RPATH}")
endif()

add_executable(fplll_dbg EXCLUDE_FROM_ALL fplll/main.cpp)
target_compile_definitions(fplll_dbg PRIVATE DEBUG)
target_link_libraries(fplll_dbg PRIVATE fpllld)

add_executable(latticegen_dbg EXCLUDE_FROM_ALL fplll/latticegen.cpp)
target_compile_definitions(latticegen_dbg PRIVATE DEBUG)
target_link_libraries(latticegen_dbg PRIVATE fpllld)
add_custom_target(dbg DEPENDS fplll_dbg latticegen_dbg)

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

include(cmake/packaging.cmake)
fplll_configure_packaging()
