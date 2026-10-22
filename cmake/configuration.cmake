function(fplll_generate_configuration)
  set(FPLLL_CONFIG_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/fplll")
  configure_file(
    "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/fplll_config.h.in"
    "${FPLLL_CONFIG_INCLUDE_DIR}/fplll_config.h"
    @ONLY)

  set(parallel_enum_sources)
  if(FPLLL_MAX_PARALLEL_ENUM_DIM GREATER 0)
    list(APPEND parallel_enum_sources fplll/enum-parallel/enumlib.cpp)
    foreach(dimension RANGE 20 ${FPLLL_MAX_PARALLEL_ENUM_DIM} 10)
      set(enum_source
        "${CMAKE_CURRENT_BINARY_DIR}/generated/enum-parallel/enumlib_dim.${dimension}.cpp")
      file(CONFIGURE OUTPUT "${enum_source}" CONTENT [=[
// Generated in the binary directory; do not edit.
#define ENUMDIMENSION @dimension@
#include <fplll/enum-parallel/enumlib_dim.cpp>
]=] @ONLY)
      list(APPEND parallel_enum_sources "${enum_source}")
    endforeach()
  endif()

  set(FPLLL_CONFIG_INCLUDE_DIR "${FPLLL_CONFIG_INCLUDE_DIR}" PARENT_SCOPE)
  set(FPLLL_PARALLEL_ENUM_SOURCES "${parallel_enum_sources}" PARENT_SCOPE)
endfunction()
