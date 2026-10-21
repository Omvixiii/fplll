cmake_minimum_required(VERSION 3.20)

# Resolve aliases as well as literal paths, including use via add_subdirectory().
file(REAL_PATH "${CMAKE_CURRENT_SOURCE_DIR}" FPLLL_SOURCE_DIR_REAL)
file(REAL_PATH "${CMAKE_CURRENT_BINARY_DIR}" FPLLL_BINARY_DIR_REAL)
if(FPLLL_SOURCE_DIR_REAL STREQUAL FPLLL_BINARY_DIR_REAL)
  message(FATAL_ERROR
    "In-source builds are disabled. Configure with a CMake Preset or pass a separate -B binary directory.")
endif()

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" FPLLL_VERSION)
string(STRIP "${FPLLL_VERSION}" FPLLL_VERSION)
if(NOT FPLLL_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
  message(FATAL_ERROR "VERSION must contain a three-component version, got: ${FPLLL_VERSION}")
endif()

project(fplll VERSION "${FPLLL_VERSION}" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
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
find_package(Threads REQUIRED)
find_package(PkgConfig QUIET)

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

if(FPLLL_USE_MPIR)
  find_path(MPIR_INCLUDE_DIR NAMES mpir.h HINTS "${FPLLL_MPIR_ROOT}" PATH_SUFFIXES include)
  find_library(MPIR_LIBRARY NAMES mpir HINTS "${FPLLL_MPIR_ROOT}" PATH_SUFFIXES lib lib64)
  find_path(GMP_INCLUDE_DIR NAMES gmp.h
    HINTS "${FPLLL_MPIR_ROOT}" "${MPIR_INCLUDE_DIR}" PATH_SUFFIXES include)
  if(NOT MPIR_INCLUDE_DIR OR NOT MPIR_LIBRARY OR NOT GMP_INCLUDE_DIR)
    message(FATAL_ERROR "FPLLL_USE_MPIR requires the MPIR headers and library")
  endif()
  add_library(MPIR::MPIR UNKNOWN IMPORTED)
  set_target_properties(MPIR::MPIR PROPERTIES
    IMPORTED_LOCATION "${MPIR_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${MPIR_INCLUDE_DIR};${GMP_INCLUDE_DIR}")
  set(FPLLL_INTEGER_TARGET MPIR::MPIR)
  set(FPLLL_INTEGER_LIBRARY_NAME mpir)
  set(HAVE_LIBMPIR 1)
else()
  find_path(GMP_INCLUDE_DIR NAMES gmp.h)
  find_library(GMP_LIBRARY NAMES gmp)
  if(NOT GMP_INCLUDE_DIR OR NOT GMP_LIBRARY)
    message(FATAL_ERROR "GMP headers and library are required")
  endif()
  add_library(GMP::GMP UNKNOWN IMPORTED)
  set_target_properties(GMP::GMP PROPERTIES
    IMPORTED_LOCATION "${GMP_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${GMP_INCLUDE_DIR}")
  set(FPLLL_INTEGER_TARGET GMP::GMP)
  set(FPLLL_INTEGER_LIBRARY_NAME gmp)
  set(HAVE_LIBGMP 1)
endif()

find_path(MPFR_INCLUDE_DIR NAMES mpfr.h)
find_library(MPFR_LIBRARY NAMES mpfr)
if(NOT MPFR_INCLUDE_DIR OR NOT MPFR_LIBRARY)
  message(FATAL_ERROR "MPFR headers and library are required")
endif()
set(HAVE_LIBMPFR 1)
add_library(MPFR::MPFR UNKNOWN IMPORTED)
set_target_properties(MPFR::MPFR PROPERTIES
  IMPORTED_LOCATION "${MPFR_LIBRARY}"
  INTERFACE_INCLUDE_DIRECTORIES "${MPFR_INCLUDE_DIR}"
  INTERFACE_LINK_LIBRARIES "${FPLLL_INTEGER_TARGET}")

set(FPLLL_QD_TARGET "")
set(FPLLL_QD_INCLUDE_DIRS "")
if(FPLLL_WITH_QD)
  if(PkgConfig_FOUND)
    pkg_check_modules(QD QUIET IMPORTED_TARGET qd)
  endif()
  if(TARGET PkgConfig::QD)
    add_library(QD::QD INTERFACE IMPORTED)
    set_target_properties(QD::QD PROPERTIES
      INTERFACE_LINK_LIBRARIES PkgConfig::QD)
    set(FPLLL_QD_TARGET QD::QD)
    set(FPLLL_QD_INCLUDE_DIRS "${QD_INCLUDE_DIRS}")
  else()
    find_path(QD_INCLUDE_DIR NAMES qd/dd_real.h)
    find_library(QD_LIBRARY NAMES qd)
    if(NOT QD_INCLUDE_DIR OR NOT QD_LIBRARY)
      message(FATAL_ERROR "FPLLL_WITH_QD requires libqd and qd/dd_real.h")
    endif()
    add_library(QD::QD UNKNOWN IMPORTED)
    set_target_properties(QD::QD PROPERTIES
      IMPORTED_LOCATION "${QD_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${QD_INCLUDE_DIR}")
    set(FPLLL_QD_TARGET QD::QD)
    set(FPLLL_QD_INCLUDE_DIRS "${QD_INCLUDE_DIR}")
  endif()
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
  if(IS_ABSOLUTE "${FPLLL_EXTENUM_LIBRARY}" AND EXISTS "${FPLLL_EXTENUM_LIBRARY}")
    set(FPLLL_EXTENUM_LIBRARY_PATH "${FPLLL_EXTENUM_LIBRARY}")
  else()
    find_library(FPLLL_EXTENUM_LIBRARY_PATH NAMES "${FPLLL_EXTENUM_LIBRARY}"
      HINTS "${FPLLL_EXTENUM_DIR}" PATH_SUFFIXES lib lib64)
  endif()
  if(NOT FPLLL_EXTENUM_LIBRARY_PATH)
    message(FATAL_ERROR "Could not find external enumeration library ${FPLLL_EXTENUM_LIBRARY}")
  endif()

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
  add_library(FPLLL::ExtEnum UNKNOWN IMPORTED)
  set_target_properties(FPLLL::ExtEnum PROPERTIES
    IMPORTED_LOCATION "${FPLLL_EXTENUM_LIBRARY_PATH}")
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

set(FPLLL_CONFIG_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/fplll")
file(MAKE_DIRECTORY "${FPLLL_CONFIG_INCLUDE_DIR}")
configure_file(
  cmake/fplll_config.h.in
  "${FPLLL_CONFIG_INCLUDE_DIR}/fplll_config.h"
  @ONLY)
configure_file(cmake/config.h.in "${CMAKE_CURRENT_BINARY_DIR}/config.h" @ONLY)

set(FPLLL_PARALLEL_ENUM_SOURCES)
if(FPLLL_MAX_PARALLEL_ENUM_DIM GREATER 0)
  list(APPEND FPLLL_PARALLEL_ENUM_SOURCES fplll/enum-parallel/enumlib.cpp)
  foreach(dimension RANGE 20 ${FPLLL_MAX_PARALLEL_ENUM_DIM} 10)
    set(enum_source
      "${CMAKE_CURRENT_BINARY_DIR}/generated/enum-parallel/enumlib_dim.${dimension}.cpp")
    configure_file(cmake/enumlib_dim.cpp.in "${enum_source}" @ONLY)
    list(APPEND FPLLL_PARALLEL_ENUM_SOURCES "${enum_source}")
  endforeach()
endif()

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
  target_compile_definitions(${target} PRIVATE
    "FPLLL_DEFAULT_STRATEGY_PATH=\"${FPLLL_DEFAULT_STRATEGY_PATH}\""
    "FPLLL_DEFAULT_STRATEGY=\"${FPLLL_DEFAULT_STRATEGY_PATH}/default.json\"")
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
set_target_properties(fplll PROPERTIES VERSION 9.0.0 SOVERSION 9)
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
set_target_properties(fplllv PROPERTIES VERSION 9.0.0 SOVERSION 9)
fplll_configure_library(fplllv)

add_library(fplll_debug_objects OBJECT EXCLUDE_FROM_ALL ${FPLLL_LIBRARY_SOURCES})
set_target_properties(fplll_debug_objects PROPERTIES POSITION_INDEPENDENT_CODE ON)
fplll_configure_library(fplll_debug_objects)
target_compile_definitions(fplll_debug_objects PRIVATE DEBUG)

add_library(fpllld SHARED EXCLUDE_FROM_ALL $<TARGET_OBJECTS:fplll_debug_objects>)
set_target_properties(fpllld PROPERTIES VERSION 9.0.0 SOVERSION 9)
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

add_executable(fplll_dbg EXCLUDE_FROM_ALL fplll/main.cpp)
target_compile_definitions(fplll_dbg PRIVATE DEBUG)
target_link_libraries(fplll_dbg PRIVATE fpllld)

add_executable(latticegen_dbg EXCLUDE_FROM_ALL fplll/latticegen.cpp)
target_compile_definitions(latticegen_dbg PRIVATE DEBUG)
target_link_libraries(latticegen_dbg PRIVATE fpllld)
add_custom_target(dbg DEPENDS fplll_dbg latticegen_dbg)

include(CTest)
option(FPLLL_BUILD_ISOLATION_TESTS
  "Enable the slower standalone build/install isolation regression test" OFF)
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()

configure_file(cmake/fplll.h.in "${CMAKE_CURRENT_BINARY_DIR}/fplll.h" COPYONLY)

set(FPLLL_PUBLIC_HEADERS
  fplll/defs.h
  fplll/fplll.h
  fplll/util.h
  fplll/svpcvp.h
  fplll/bkz.h
  fplll/lll.h
  fplll/gso_interface.h
  fplll/gso_gram.h
  fplll/gso.h
  fplll/wrapper.h
  fplll/bkz_param.h
  fplll/householder.h
  fplll/hlll.h
  fplll/threadpool.h)
install(FILES ${FPLLL_PUBLIC_HEADERS}
  "${FPLLL_CONFIG_INCLUDE_DIR}/fplll_config.h"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/fplll")
install(FILES
  fplll/nr/dpe.h
  fplll/nr/matrix.h
  fplll/nr/matrix.cpp
  fplll/nr/nr_FP_dd.inl
  fplll/nr/nr_FP_d.inl
  fplll/nr/nr_FP_dpe.inl
  fplll/nr/nr_FP.inl
  fplll/nr/nr_FP_ld.inl
  fplll/nr/nr_FP_misc.inl
  fplll/nr/nr_FP_mpfr.inl
  fplll/nr/nr_FP_qd.inl
  fplll/nr/nr.h
  fplll/nr/nr_rand.inl
  fplll/nr/nr_Z_d.inl
  fplll/nr/nr_Z.inl
  fplll/nr/nr_Z_l.inl
  fplll/nr/nr_Z_misc.inl
  fplll/nr/nr_Z_mpz.inl
  fplll/nr/numvect.h
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/fplll/nr")
install(FILES
  fplll/enum/evaluator.h
  fplll/enum/enumerate.h
  fplll/enum/enumerate_base.h
  fplll/enum/enumerate_ext.h
  fplll/enum/enumerate_ext_api.h
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/fplll/enum")
install(FILES
  fplll/pruner/pruner.h
  fplll/pruner/pruner_simplex.h
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/fplll/pruner")
install(FILES fplll/io/thread_pool.hpp
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/fplll/io")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/fplll.h"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

install(TARGETS fplll fplll_static EXPORT fplllTargets
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(TARGETS fplll_cli latticegen
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(EXPORT fplllTargets
  NAMESPACE fplll::
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/fplll")
install(FILES strategies/default.json
  DESTINATION "${CMAKE_INSTALL_DATADIR}/fplll/strategies")

file(RELATIVE_PATH FPLLL_PC_PREFIX_RELATIVE
  "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/pkgconfig"
  "${CMAKE_INSTALL_PREFIX}")
set(FPLLL_PC_LIBS "-l${FPLLL_INTEGER_LIBRARY_NAME}" "-lmpfr" "-pthread")
set(FPLLL_PC_CFLAGS "-I${GMP_INCLUDE_DIR}" "-I${MPFR_INCLUDE_DIR}")
if(FPLLL_USE_MPIR)
  list(APPEND FPLLL_PC_CFLAGS "-I${MPIR_INCLUDE_DIR}")
endif()
if(FPLLL_WITH_QD)
  list(APPEND FPLLL_PC_LIBS -lqd)
  foreach(qd_include_dir IN LISTS FPLLL_QD_INCLUDE_DIRS)
    list(APPEND FPLLL_PC_CFLAGS "-I${qd_include_dir}")
  endforeach()
endif()
if(FPLLL_EXTENUM_LIBRARY_PATH)
  list(APPEND FPLLL_PC_LIBS "${FPLLL_EXTENUM_LIBRARY_PATH}")
endif()
string(JOIN " " FPLLL_PC_LIBS ${FPLLL_PC_LIBS})
string(JOIN " " FPLLL_PC_CFLAGS ${FPLLL_PC_CFLAGS})
configure_file(cmake/fplll.pc.in "${CMAKE_CURRENT_BINARY_DIR}/fplll.pc" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/fplll.pc"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

include(CMakePackageConfigHelpers)
set(FPLLL_CMAKE_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/fplll")
configure_package_config_file(
  cmake/fplllConfig.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/fplllConfig.cmake"
  INSTALL_DESTINATION "${FPLLL_CMAKE_INSTALL_DIR}")
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/fplllConfigVersion.cmake"
  VERSION "${PROJECT_VERSION}"
  COMPATIBILITY SameMajorVersion)
install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/fplllConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/fplllConfigVersion.cmake"
  DESTINATION "${FPLLL_CMAKE_INSTALL_DIR}")

add_custom_target(check-style
  COMMAND "${CMAKE_COMMAND}" "-DFPLLL_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckStyle.cmake"
  VERBATIM)
add_custom_target(format
  COMMAND "${CMAKE_COMMAND}" "-DFPLLL_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
    -DFPLLL_FORMAT=ON -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckStyle.cmake"
  VERBATIM)

find_package(Doxygen QUIET)
if(DOXYGEN_FOUND)
  # Override the historical Doxyfile's relative paths without editing sources.
  configure_file(cmake/Doxyfile.in "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile" @ONLY)
  add_custom_target(doc
    COMMAND "${DOXYGEN_EXECUTABLE}" "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    VERBATIM)
endif()

# Release archives are built entirely inside the binary directory.
set(CPACK_PACKAGE_NAME fplll)
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_SOURCE_GENERATOR TGZ)
set(CPACK_SOURCE_PACKAGE_FILE_NAME "fplll-${PROJECT_VERSION}")
set(CPACK_VERBATIM_VARIABLES YES)
string(REGEX REPLACE "([][+.*()^$?|\\\\])" "\\\\\\1"
  FPLLL_SOURCE_REGEX "${CMAKE_CURRENT_SOURCE_DIR}")
string(REGEX REPLACE "([][+.*()^$?|\\\\])" "\\\\\\1"
  FPLLL_BINARY_REGEX "${CMAKE_CURRENT_BINARY_DIR}")
set(CPACK_SOURCE_IGNORE_FILES
  "^${FPLLL_BINARY_REGEX}(/|$)"
  "^${FPLLL_SOURCE_REGEX}/(build|_install|notes|[.]git|[.]cache|[.]vscode|[.]agents|[.]codex)(/|$)"
  "^${FPLLL_SOURCE_REGEX}/CMakeUserPresets[.]json$"
  "^${FPLLL_SOURCE_REGEX}/CMakeCache[.]txt$"
  "^${FPLLL_SOURCE_REGEX}/CMakeFiles/"
  "^${FPLLL_SOURCE_REGEX}/compile_commands[.]json$"
  "^${FPLLL_SOURCE_REGEX}/doc/html/"
  "[.]swp$" "~$")
include(CPack)
