# Command-line programs live beside the library they link, not inside fplll/.
# The debug variants reuse the same sources and link the DEBUG-enabled library.

add_executable(fplll_cli main.cpp)
set_target_properties(fplll_cli PROPERTIES
  OUTPUT_NAME fplll)
target_link_libraries(fplll_cli PRIVATE fplll)

add_executable(latticegen latticegen.cpp)
target_link_libraries(latticegen PRIVATE fplll)

# Installed tools find the library relative to the configured final prefix.
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

add_executable(fplll_dbg EXCLUDE_FROM_ALL main.cpp)
target_compile_definitions(fplll_dbg PRIVATE DEBUG)
target_link_libraries(fplll_dbg PRIVATE fpllld)

add_executable(latticegen_dbg EXCLUDE_FROM_ALL latticegen.cpp)
target_compile_definitions(latticegen_dbg PRIVATE DEBUG)
target_link_libraries(latticegen_dbg PRIVATE fpllld)

# Only a top-level build owns the aggregate convenience target.
if(PROJECT_IS_TOP_LEVEL)
  add_custom_target(dbg DEPENDS fplll_dbg latticegen_dbg)
endif()
