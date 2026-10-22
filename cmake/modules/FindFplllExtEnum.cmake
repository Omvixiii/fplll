if(IS_ABSOLUTE "${FplllExtEnum_NAME}" AND EXISTS "${FplllExtEnum_NAME}")
  set(FplllExtEnum_LIBRARY "${FplllExtEnum_NAME}")
else()
  find_library(FplllExtEnum_LIBRARY NAMES "${FplllExtEnum_NAME}"
    HINTS "${FPLLL_EXTENUM_DIR}" PATH_SUFFIXES lib lib64)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(FplllExtEnum REQUIRED_VARS FplllExtEnum_LIBRARY)

if(FplllExtEnum_FOUND AND NOT TARGET FPLLL::ExtEnum)
  add_library(FPLLL::ExtEnum UNKNOWN IMPORTED)
  set_target_properties(FPLLL::ExtEnum PROPERTIES
    IMPORTED_LOCATION "${FplllExtEnum_LIBRARY}")
endif()
mark_as_advanced(FplllExtEnum_LIBRARY)
