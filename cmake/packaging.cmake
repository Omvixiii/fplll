function(fplll_install_pkgconfig)
  # Installed headers inline GMP/MPFR calls, so those dependencies are part of
  # the public link interface and belong in Requires.
  set(FPLLL_PC_REQUIRES "mpfr gmp")
  set(FPLLL_PC_CFLAGS "")
  set(FPLLL_PC_LIBS "")
  set(FPLLL_PC_LIBS_PRIVATE "")
  if(CMAKE_USE_PTHREADS_INIT)
    set(FPLLL_PC_CFLAGS "-pthread")
    set(FPLLL_PC_LIBS "-pthread")
  endif()
  if(FPLLL_USE_MPIR)
    # MPIR has no pkg-config module; use explicit flags for the backend.
    set(FPLLL_PC_REQUIRES "")
    get_filename_component(mpfr_libdir "${MPFR_LIBRARY}" DIRECTORY)
    get_filename_component(integer_libdir "${MPIR_LIBRARY}" DIRECTORY)
    list(APPEND FPLLL_PC_CFLAGS
      "-I${MPIR_INCLUDE_DIR}" "-I${MPIR_GMP_INCLUDE_DIR}" "-I${MPFR_INCLUDE_DIR}")
    list(APPEND FPLLL_PC_LIBS
      "-L${mpfr_libdir}" -lmpfr "-L${integer_libdir}" -lmpir)
  endif()
  if(FPLLL_WITH_QD)
    # QD installs qd.pc both upstream and in distributions.
    list(PREPEND FPLLL_PC_REQUIRES qd)
  endif()
  if(FPLLL_EXTENUM_LIBRARY_PATH)
    list(APPEND FPLLL_PC_LIBS_PRIVATE "${FPLLL_EXTENUM_LIBRARY_PATH}")
  endif()
  if(FPLLL_ATOMIC_LIBRARY)
    list(APPEND FPLLL_PC_LIBS_PRIVATE "-l${FPLLL_ATOMIC_LIBRARY}")
  endif()
  string(JOIN " " FPLLL_PC_REQUIRES_TEXT ${FPLLL_PC_REQUIRES})
  string(JOIN " " FPLLL_PC_CFLAGS_TEXT ${FPLLL_PC_CFLAGS})
  string(JOIN " " FPLLL_PC_LIBS_TEXT ${FPLLL_PC_LIBS})
  string(JOIN " " FPLLL_PC_LIBS_PRIVATE_TEXT ${FPLLL_PC_LIBS_PRIVATE})

  # @ONLY preserves pkg-config's own ${variable} references.
  file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/fplll.pc" CONTENT [=[
prefix=@CMAKE_INSTALL_PREFIX@
exec_prefix=${prefix}
libdir=@CMAKE_INSTALL_FULL_LIBDIR@
includedir=@CMAKE_INSTALL_FULL_INCLUDEDIR@

Name: fplll
Description: lattice algorithms with floating-point computations
Version: @PROJECT_VERSION@
Requires: @FPLLL_PC_REQUIRES_TEXT@
Cflags: -I${includedir} @FPLLL_PC_CFLAGS_TEXT@
Libs: -L${libdir} -lfplll @FPLLL_PC_LIBS_TEXT@
Libs.private: @FPLLL_PC_LIBS_PRIVATE_TEXT@
]=] @ONLY)
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/fplll.pc"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
endfunction()

function(fplll_configure_packaging)
  # The historical root-level include is part of the Autotools install contract.
  file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/fplll.h" CONTENT [=[
// Compatibility entry point; prefer <fplll/fplll.h> in new code.
#include <fplll/fplll.h>
]=] @ONLY)
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/fplll.h"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

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

  fplll_install_pkgconfig()

  include(CMakePackageConfigHelpers)
  set(FPLLL_CMAKE_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/fplll")
  set(FPLLL_PACKAGE_DEPENDENCIES Threads)
  if(FPLLL_USE_MPIR)
    list(APPEND FPLLL_PACKAGE_DEPENDENCIES MPIR)
  else()
    list(APPEND FPLLL_PACKAGE_DEPENDENCIES GMP)
  endif()
  list(APPEND FPLLL_PACKAGE_DEPENDENCIES MPFR)
  if(FPLLL_WITH_QD)
    list(APPEND FPLLL_PACKAGE_DEPENDENCIES QD)
  endif()
  set(FPLLL_EXTENUM_PACKAGE_NAME "")
  if(FPLLL_HAS_EXTENUM)
    list(APPEND FPLLL_PACKAGE_DEPENDENCIES FplllExtEnum)
    get_filename_component(FPLLL_EXTENUM_PACKAGE_NAME "${FPLLL_EXTENUM_LIBRARY}" NAME)
    string(REGEX REPLACE "^lib" "" FPLLL_EXTENUM_PACKAGE_NAME "${FPLLL_EXTENUM_PACKAGE_NAME}")
    string(REGEX REPLACE "[.](so([.].*)?|a|dylib)$" "" FPLLL_EXTENUM_PACKAGE_NAME
      "${FPLLL_EXTENUM_PACKAGE_NAME}")
  endif()
  configure_package_config_file(
    "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/fplllConfig.cmake.in"
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
  install(DIRECTORY "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/modules/"
    DESTINATION "${FPLLL_CMAKE_INSTALL_DIR}/modules"
    FILES_MATCHING PATTERN "Find*.cmake")

  if(NOT PROJECT_IS_TOP_LEVEL)
    return()
  endif()

  set(FPLLL_INSTALL_MANIFEST "${CMAKE_CURRENT_BINARY_DIR}/install_manifest.txt")
  configure_file(
    "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake" @ONLY)
  add_custom_target(uninstall
    COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    VERBATIM)

  # Release archives are built entirely inside the binary directory.
  set(CPACK_PACKAGE_NAME fplll)
  set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
  set(CPACK_SOURCE_GENERATOR TGZ)
  set(CPACK_SOURCE_PACKAGE_FILE_NAME "fplll-${PROJECT_VERSION}")
  set(CPACK_SOURCE_INSTALLED_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR};/")
  set(CPACK_VERBATIM_VARIABLES YES)
  # CPACK_SOURCE_IGNORE_FILES matches absolute paths. Anchor the patterns to
  # this source tree so a checkout below a matching path is not excluded.
  string(REGEX REPLACE "([][+.*()^$?|\\\\])" "\\\\\\1"
    source_pattern "${CMAKE_CURRENT_SOURCE_DIR}")
  set(source_entry "^${source_pattern}/(.*/)?")
  set(CPACK_SOURCE_IGNORE_FILES
    "${source_entry}build/"
    "${source_entry}_install/"
    "${source_entry}[.]git($|/)"
    "${source_entry}[.]venv($|/)"
    "${source_entry}[.]vscode($|/)"
    "${source_entry}CMakeUserPresets[.]json$"
    "${source_entry}compile_commands[.]json$"
    "${source_entry}CMakeCache[.]txt$"
    "${source_entry}CMakeFiles/")
  include(CPack)
endfunction()
