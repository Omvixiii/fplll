cmake_minimum_required(VERSION 3.20)

project(fplll VERSION 5.5.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)

include(GNUInstallDirs)
find_package(Threads REQUIRED)
find_path(GMP_INCLUDE_DIR NAMES gmp.h)
find_library(GMP_LIBRARY NAMES gmp)
if(NOT GMP_INCLUDE_DIR OR NOT GMP_LIBRARY)
  message(FATAL_ERROR "GMP headers and library are required")
endif()

find_path(MPFR_INCLUDE_DIR NAMES mpfr.h)
find_library(MPFR_LIBRARY NAMES mpfr)
if(NOT MPFR_INCLUDE_DIR OR NOT MPFR_LIBRARY)
  message(FATAL_ERROR "MPFR headers and library are required")
endif()

add_library(GMP::GMP UNKNOWN IMPORTED)
set_target_properties(GMP::GMP PROPERTIES
  IMPORTED_LOCATION "${GMP_LIBRARY}"
  INTERFACE_INCLUDE_DIRECTORIES "${GMP_INCLUDE_DIR}")

add_library(MPFR::MPFR UNKNOWN IMPORTED)
set_target_properties(MPFR::MPFR PROPERTIES
  IMPORTED_LOCATION "${MPFR_LIBRARY}"
  INTERFACE_INCLUDE_DIRECTORIES "${MPFR_INCLUDE_DIR}"
  INTERFACE_LINK_LIBRARIES GMP::GMP)

set(FPLLL_CONFIG_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/fplll")
file(MAKE_DIRECTORY "${FPLLL_CONFIG_INCLUDE_DIR}")
configure_file(
  cmake/fplll_config.h.in
  "${FPLLL_CONFIG_INCLUDE_DIR}/fplll_config.h"
  @ONLY)
configure_file(cmake/config.h.in "${CMAKE_CURRENT_BINARY_DIR}/config.h" @ONLY)

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
  fplll/enum-parallel/enumlib.cpp
  fplll/enum-parallel/enumlib_dim.20.cpp
  fplll/enum-parallel/enumlib_dim.30.cpp
  fplll/enum-parallel/enumlib_dim.40.cpp
  fplll/enum-parallel/enumlib_dim.50.cpp
  fplll/enum-parallel/enumlib_dim.60.cpp
  fplll/enum-parallel/enumlib_dim.70.cpp
  fplll/enum-parallel/enumlib_dim.80.cpp
  fplll/enum-parallel/enumlib_dim.90.cpp
  fplll/enum-parallel/enumlib_dim.100.cpp
  fplll/enum-parallel/enumlib_dim.110.cpp
  fplll/enum-parallel/enumlib_dim.120.cpp
  fplll/enum-parallel/enumlib_dim.130.cpp
  fplll/enum-parallel/enumlib_dim.140.cpp
  fplll/enum-parallel/enumlib_dim.150.cpp
  fplll/enum-parallel/enumlib_dim.160.cpp)

add_library(fplll SHARED ${FPLLL_LIBRARY_SOURCES})
add_library(fplll::fplll ALIAS fplll)
set_target_properties(fplll PROPERTIES
  VERSION 9.0.0
  SOVERSION 9)

target_include_directories(fplll
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    $<BUILD_INTERFACE:${FPLLL_CONFIG_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/fplll")
set(FPLLL_DEFAULT_STRATEGY_PATH
  "${CMAKE_INSTALL_FULL_DATADIR}/fplll/strategies")
target_compile_definitions(fplll PRIVATE
  "FPLLL_DEFAULT_STRATEGY_PATH=\"${FPLLL_DEFAULT_STRATEGY_PATH}\""
  "FPLLL_DEFAULT_STRATEGY=\"${FPLLL_DEFAULT_STRATEGY_PATH}/default.json\"")
target_link_libraries(fplll PUBLIC MPFR::MPFR GMP::GMP Threads::Threads)

add_executable(fplll_cli fplll/main.cpp)
set_target_properties(fplll_cli PROPERTIES
  OUTPUT_NAME fplll
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
target_include_directories(fplll_cli PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_BINARY_DIR}"
  "${FPLLL_CONFIG_INCLUDE_DIR}")
target_link_libraries(fplll_cli PRIVATE fplll)

add_executable(latticegen fplll/latticegen.cpp)
set_target_properties(latticegen PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
target_include_directories(latticegen PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_BINARY_DIR}"
  "${FPLLL_CONFIG_INCLUDE_DIR}")
target_link_libraries(latticegen PRIVATE fplll)

add_executable(llldiff fplll/llldiff.cpp)
set_target_properties(llldiff PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
target_include_directories(llldiff PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_BINARY_DIR}"
  "${FPLLL_CONFIG_INCLUDE_DIR}")
target_link_libraries(llldiff PRIVATE fplll)

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fplll.h"
  "#include <fplll/fplll.h>\n")

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

install(TARGETS fplll fplll_cli latticegen
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
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
  if(QD_INCLUDE_DIR)
    list(APPEND FPLLL_PC_CFLAGS "-I${QD_INCLUDE_DIR}")
  endif()
endif()
if(FPLLL_EXTENUM_LIBRARY_PATH)
  list(APPEND FPLLL_PC_LIBS "${FPLLL_EXTENUM_LIBRARY_PATH}")
endif()
string(JOIN " " FPLLL_PC_LIBS ${FPLLL_PC_LIBS})
string(JOIN " " FPLLL_PC_CFLAGS ${FPLLL_PC_CFLAGS})
configure_file(cmake/fplll.pc.in "${CMAKE_CURRENT_BINARY_DIR}/fplll.pc" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/fplll.pc"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
