function(fplll_install_pkgconfig)
  file(RELATIVE_PATH FPLLL_PC_PREFIX_RELATIVE
    "${CMAKE_INSTALL_FULL_LIBDIR}/pkgconfig" "${CMAKE_INSTALL_PREFIX}")
  set(FPLLL_PC_LIBDIR "${CMAKE_INSTALL_LIBDIR}")
  if(NOT IS_ABSOLUTE "${FPLLL_PC_LIBDIR}")
    set(FPLLL_PC_LIBDIR "\${prefix}/${FPLLL_PC_LIBDIR}")
  endif()
  set(FPLLL_PC_INCLUDEDIR "${CMAKE_INSTALL_INCLUDEDIR}")
  if(NOT IS_ABSOLUTE "${FPLLL_PC_INCLUDEDIR}")
    set(FPLLL_PC_INCLUDEDIR "\${prefix}/${FPLLL_PC_INCLUDEDIR}")
  endif()
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
  if(FPLLL_ATOMIC_LIBRARY)
    list(APPEND FPLLL_PC_LIBS "-l${FPLLL_ATOMIC_LIBRARY}")
  endif()
  string(JOIN " " FPLLL_PC_LIBS ${FPLLL_PC_LIBS})
  string(JOIN " " FPLLL_PC_CFLAGS ${FPLLL_PC_CFLAGS})

  # @ONLY preserves pkg-config's own ${variable} references.
  file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/fplll.pc" CONTENT [=[
prefix=${pcfiledir}/@FPLLL_PC_PREFIX_RELATIVE@
exec_prefix=${prefix}
libdir=@FPLLL_PC_LIBDIR@
includedir=@FPLLL_PC_INCLUDEDIR@

Name: fplll
Description: lattice algorithms with floating-point computations
Version: @PROJECT_VERSION@
Cflags: -I${includedir} -I${includedir}/fplll @FPLLL_PC_CFLAGS@ -pthread
Libs: -L${libdir} -lfplll @FPLLL_PC_LIBS@
]=] @ONLY)
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/fplll.pc"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
endfunction()

function(fplll_configure_packaging)
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

  # Release archives are built entirely inside the binary directory.
  set(CPACK_PACKAGE_NAME fplll)
  set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
  set(CPACK_SOURCE_GENERATOR TGZ)
  set(CPACK_SOURCE_PACKAGE_FILE_NAME "fplll-${PROJECT_VERSION}")
  set(CPACK_SOURCE_INSTALLED_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR};/")
  set(CPACK_VERBATIM_VARIABLES YES)
  set(CPACK_SOURCE_IGNORE_FILES
    "${CMAKE_CURRENT_BINARY_DIR}/"
    "/build/"
    "/_install/"
    "/.git/"
    "/.venv/"
    "/.vscode/"
    "/notes/"
    "/CMakeUserPresets.json$"
    "/compile_commands.json$"
    "/CMakeCache.txt$"
    "/CMakeFiles/")
  include(CPack)
endfunction()
