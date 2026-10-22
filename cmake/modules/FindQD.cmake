find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(PC_QD QUIET IMPORTED_TARGET qd)
endif()

if(TARGET PkgConfig::PC_QD)
  set(QD_INCLUDE_DIRS "${PC_QD_INCLUDE_DIRS}")
  set(QD_FOUND TRUE)
else()
  find_path(QD_INCLUDE_DIR NAMES qd/dd_real.h)
  find_library(QD_LIBRARY NAMES qd)
  set(QD_INCLUDE_DIRS "${QD_INCLUDE_DIR}")
endif()

include(FindPackageHandleStandardArgs)
if(TARGET PkgConfig::PC_QD)
  find_package_handle_standard_args(QD REQUIRED_VARS QD_FOUND)
else()
  find_package_handle_standard_args(QD REQUIRED_VARS QD_LIBRARY QD_INCLUDE_DIR)
endif()

if(QD_FOUND AND NOT TARGET QD::QD)
  if(TARGET PkgConfig::PC_QD)
    add_library(QD::QD INTERFACE IMPORTED)
    set_target_properties(QD::QD PROPERTIES
      INTERFACE_LINK_LIBRARIES PkgConfig::PC_QD)
  else()
    add_library(QD::QD UNKNOWN IMPORTED)
    set_target_properties(QD::QD PROPERTIES
      IMPORTED_LOCATION "${QD_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${QD_INCLUDE_DIR}")
  endif()
endif()
mark_as_advanced(QD_INCLUDE_DIR QD_LIBRARY)
