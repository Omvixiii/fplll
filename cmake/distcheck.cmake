cmake_minimum_required(VERSION 3.25)

# Validates a source archive end to end: unpack read-only, build out of tree,
# test, install, verify installed metadata, uninstall, stage with DESTDIR,
# clean and rebuild. Invoked by the project's distcheck target.

get_filename_component(FPLLL_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
if(NOT FPLLL_BINARY_DIR OR NOT EXISTS "${FPLLL_BINARY_DIR}/CMakeCache.txt"
   OR NOT EXISTS "${FPLLL_BINARY_DIR}/CPackSourceConfig.cmake")
  message(FATAL_ERROR "Run this script through the distcheck target of a configured build tree")
endif()

load_cache("${FPLLL_BINARY_DIR}" READ_WITH_PREFIX parent_
  CMAKE_CXX_COMPILER CMAKE_CXX_STANDARD CMAKE_CXX_FLAGS CMAKE_BUILD_TYPE
  CMAKE_PREFIX_PATH CMAKE_INSTALL_LIBDIR
  CMAKE_OSX_SYSROOT CMAKE_OSX_ARCHITECTURES CMAKE_OSX_DEPLOYMENT_TARGET
  FPLLL_USE_MPIR FPLLL_WITH_QD FPLLL_WITH_RECURSIVE_ENUM
  FPLLL_MAX_ENUM_DIM FPLLL_MAX_PARALLEL_ENUM_DIM
  GMP_LIBRARY GMP_INCLUDE_DIR MPIR_LIBRARY MPIR_INCLUDE_DIR MPIR_GMP_INCLUDE_DIR
  MPFR_LIBRARY MPFR_INCLUDE_DIR QD_LIBRARY QD_INCLUDE_DIR
  FPLLL_EXTENUM_LIBRARY FPLLL_EXTENUM_FUNCTION)
if(NOT parent_CMAKE_BUILD_TYPE)
  message(FATAL_ERROR "distcheck requires a single-configuration build; use the project presets")
endif()
unset(ENV{DESTDIR})

set(check_dir "${FPLLL_BINARY_DIR}/distcheck")
set(work "${check_dir}/work")
set(logs "${check_dir}/logs")
if(EXISTS "${work}")
  file(CHMOD_RECURSE "${work}" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
endif()
file(REMOVE_RECURSE "${work}" "${logs}")
file(MAKE_DIRECTORY "${work}" "${logs}")
file(REMOVE "${check_dir}/passed.txt")

function(distcheck_step name directory)
  cmake_parse_arguments(PARSE_ARGV 2 STEP "" "INPUT;OUTPUT_VARIABLE" "COMMAND")
  set(log "${logs}/${name}.log")
  set(command_text "")
  foreach(argument IN LISTS STEP_COMMAND)
    string(REPLACE "'" "'\\''" argument "${argument}")
    string(APPEND command_text " '${argument}'")
  endforeach()
  set(input_option)
  if(STEP_INPUT)
    string(REPLACE ";" "\\;" input_file "${STEP_INPUT}")
    set(input_option INPUT_FILE "${input_file}")
    string(REPLACE "'" "'\\''" input_text "${STEP_INPUT}")
    string(APPEND command_text " < '${input_text}'")
  endif()
  file(WRITE "${log}" "Directory: ${directory}\nCommand:${command_text}\n\n")
  message(STATUS "distcheck: ${name} (${log})")
  execute_process(COMMAND ${STEP_COMMAND} WORKING_DIRECTORY "${directory}"
    ${input_option} RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
  file(APPEND "${log}" "${output}${error}")
  if(NOT "${result}" STREQUAL "0")
    set(tail "${output}${error}")
    string(LENGTH "${tail}" length)
    if(length GREATER 6000)
      math(EXPR start "${length} - 6000")
      string(SUBSTRING "${tail}" ${start} -1 tail)
    endif()
    message(FATAL_ERROR
      "distcheck stage '${name}' failed (${result}).\n${tail}\n"
      "Command:${command_text}\nDirectory: ${directory}\n"
      "Log: ${log}\nThe failed work tree is retained at ${work}")
  endif()
  if(STEP_OUTPUT_VARIABLE)
    set(${STEP_OUTPUT_VARIABLE} "${output}" PARENT_SCOPE)
  endif()
endfunction()

function(source_digest directory output)
  file(GLOB_RECURSE files LIST_DIRECTORIES TRUE RELATIVE "${directory}" "${directory}/*")
  list(SORT files)
  set(contents "")
  foreach(file IN LISTS files)
    if(IS_SYMLINK "${directory}/${file}")
      file(READ_SYMLINK "${directory}/${file}" target)
      string(APPEND contents "link ${file}: ${target}\n")
    elseif(IS_DIRECTORY "${directory}/${file}")
      string(APPEND contents "directory ${file}\n")
    else()
      file(SHA256 "${directory}/${file}" digest)
      string(APPEND contents "file ${file}: ${digest}\n")
    endif()
  endforeach()
  string(SHA256 digest "${contents}")
  set(${output} "${digest}" PARENT_SCOPE)
endfunction()

function(assert_no_files directory what)
  file(GLOB_RECURSE remaining LIST_DIRECTORIES FALSE "${directory}/*")
  if(remaining)
    message(FATAL_ERROR "${what} left files behind under ${directory}: ${remaining}")
  endif()
endfunction()

get_filename_component(cmake_bin "${CMAKE_COMMAND}" DIRECTORY)
find_program(cpack NAMES cpack HINTS "${cmake_bin}" NO_DEFAULT_PATH REQUIRED)
find_program(ctest NAMES ctest HINTS "${cmake_bin}" NO_DEFAULT_PATH REQUIRED)
find_program(pkg_config NAMES pkg-config pkgconf REQUIRED)

set(libdir "${parent_CMAKE_INSTALL_LIBDIR}")
if(NOT libdir)
  set(libdir lib)
endif()

set(dependency_args
  "-DMPFR_LIBRARY=${parent_MPFR_LIBRARY}"
  "-DMPFR_INCLUDE_DIR=${parent_MPFR_INCLUDE_DIR}")
if(parent_FPLLL_USE_MPIR)
  list(APPEND dependency_args
    "-DMPIR_LIBRARY=${parent_MPIR_LIBRARY}"
    "-DMPIR_INCLUDE_DIR=${parent_MPIR_INCLUDE_DIR}"
    "-DMPIR_GMP_INCLUDE_DIR=${parent_MPIR_GMP_INCLUDE_DIR}")
else()
  list(APPEND dependency_args
    "-DGMP_LIBRARY=${parent_GMP_LIBRARY}"
    "-DGMP_INCLUDE_DIR=${parent_GMP_INCLUDE_DIR}")
endif()
if(parent_FPLLL_WITH_QD)
  if(parent_QD_INCLUDE_DIR)
    list(APPEND dependency_args "-DQD_INCLUDE_DIR=${parent_QD_INCLUDE_DIR}")
  endif()
  if(parent_QD_LIBRARY)
    list(APPEND dependency_args "-DQD_LIBRARY=${parent_QD_LIBRARY}")
  endif()
endif()
if(parent_FPLLL_EXTENUM_LIBRARY)
  list(APPEND dependency_args
    "-DFPLLL_EXTENUM_LIBRARY=${parent_FPLLL_EXTENUM_LIBRARY}"
    "-DFPLLL_EXTENUM_FUNCTION=${parent_FPLLL_EXTENUM_FUNCTION}")
endif()

# Produce exactly one source archive and assert its content is self-contained.
file(GLOB stale_archives "${FPLLL_BINARY_DIR}/fplll-*.tar.gz")
if(stale_archives)
  file(REMOVE ${stale_archives})
endif()
distcheck_step(package "${FPLLL_BINARY_DIR}" COMMAND "${cpack}"
  --config "${FPLLL_BINARY_DIR}/CPackSourceConfig.cmake" -G TGZ -B "${FPLLL_BINARY_DIR}")
file(GLOB archives "${FPLLL_BINARY_DIR}/fplll-*.tar.gz")
list(LENGTH archives archive_count)
if(NOT archive_count EQUAL 1)
  message(FATAL_ERROR "CPack did not produce exactly one source archive: ${archives}")
endif()
list(GET archives 0 archive)
file(SHA256 "${archive}" archive_digest)

distcheck_step(archive-list "${work}" OUTPUT_VARIABLE entries
  COMMAND "${CMAKE_COMMAND}" -E tar tf "${archive}")
file(WRITE "${logs}/archive-contents.log" "${entries}")
if(entries MATCHES "(^|/)([.]git|[.]venv|[.]vscode|build|_install|CMakeFiles)(/|[\r\n])"
   OR entries MATCHES "(^|/)(CMakeUserPresets[.]json|CMakeCache[.]txt|compile_commands[.]json)([\r\n]|$)")
  message(FATAL_ERROR "Source archive contains local state; inspect ${logs}/archive-contents.log")
endif()
if(entries MATCHES "(^|[\r\n])(/|[A-Za-z]:)" OR entries MATCHES "(^|/)[.][.](/|[\r\n]|$)")
  message(FATAL_ERROR "Source archive contains an unsafe path")
endif()

set(unpack "${work}/unpack")
file(MAKE_DIRECTORY "${unpack}")
distcheck_step(archive-extract "${unpack}" COMMAND "${CMAKE_COMMAND}" -E tar xf "${archive}")
file(GLOB roots LIST_DIRECTORIES TRUE "${unpack}/*")
list(LENGTH roots root_count)
if(NOT root_count EQUAL 1)
  message(FATAL_ERROR "Expected one source directory in ${archive}")
endif()
list(GET roots 0 source)
foreach(required CMakeLists.txt fplll/fplll.h strategies/default.json)
  if(NOT EXISTS "${source}/${required}")
    message(FATAL_ERROR "Source archive is missing ${required}")
  endif()
endforeach()
source_digest("${source}" before)
file(CHMOD_RECURSE "${source}"
  FILE_PERMISSIONS OWNER_READ GROUP_READ WORLD_READ
  DIRECTORY_PERMISSIONS OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

set(binary "${work}/build")
set(prefix "${work}/prefix")
set(config "${parent_CMAKE_BUILD_TYPE}")

set(configure_args
  -S "${source}" -B "${binary}" -G "${FPLLL_GENERATOR}"
  "-DCMAKE_BUILD_TYPE=${config}"
  "-DCMAKE_INSTALL_PREFIX=${prefix}"
  "-DCMAKE_INSTALL_LIBDIR=${libdir}"
  "-DCMAKE_CXX_COMPILER=${parent_CMAKE_CXX_COMPILER}"
  -DBUILD_TESTING=ON
  "-DFPLLL_USE_MPIR=${parent_FPLLL_USE_MPIR}"
  "-DFPLLL_WITH_QD=${parent_FPLLL_WITH_QD}"
  "-DFPLLL_WITH_RECURSIVE_ENUM=${parent_FPLLL_WITH_RECURSIVE_ENUM}"
  "-DFPLLL_MAX_ENUM_DIM=${parent_FPLLL_MAX_ENUM_DIM}"
  "-DFPLLL_MAX_PARALLEL_ENUM_DIM=${parent_FPLLL_MAX_PARALLEL_ENUM_DIM}"
  "-DFPLLL_EXTENUM_LIBRARY=${parent_FPLLL_EXTENUM_LIBRARY}"
  "-DFPLLL_EXTENUM_FUNCTION=${parent_FPLLL_EXTENUM_FUNCTION}"
  ${dependency_args})
foreach(variable CMAKE_CXX_STANDARD CMAKE_CXX_FLAGS CMAKE_PREFIX_PATH
    CMAKE_OSX_SYSROOT CMAKE_OSX_ARCHITECTURES CMAKE_OSX_DEPLOYMENT_TARGET)
  if(DEFINED parent_${variable} AND NOT "${parent_${variable}}" STREQUAL "")
    list(APPEND configure_args "-D${variable}=${parent_${variable}}")
  endif()
endforeach()

distcheck_step(configure "${work}" COMMAND "${CMAKE_COMMAND}" ${configure_args})
distcheck_step(build "${work}" COMMAND "${CMAKE_COMMAND}" --build "${binary}" --config "${config}")
distcheck_step(test "${work}" COMMAND "${ctest}" --test-dir "${binary}" -C "${config}"
  --output-on-failure --no-tests=error)
distcheck_step(install "${work}" COMMAND "${CMAKE_COMMAND}" --install "${binary}" --config "${config}")

foreach(expected
    bin/fplll
    bin/latticegen
    include/fplll/fplll.h
    include/fplll/fplll_config.h
    include/fplll.h
    "lib/pkgconfig/fplll.pc"
    "lib/cmake/fplll/fplllConfig.cmake"
    share/fplll/strategies/default.json)
  if(NOT EXISTS "${prefix}/${expected}")
    message(FATAL_ERROR "Installation is missing ${expected}")
  endif()
endforeach()
file(GLOB installed_libraries "${prefix}/${libdir}/libfplll*")
list(FILTER installed_libraries INCLUDE REGEX "[.]a$")
if(NOT installed_libraries)
  message(FATAL_ERROR "Installation is missing the static library")
endif()

# The installed .pc file resolves gmp/mpfr through pkg-config modules.
set(pc_paths "${prefix}/${libdir}/pkgconfig")
foreach(dependency_library IN ITEMS
    "${parent_GMP_LIBRARY}" "${parent_MPIR_LIBRARY}" "${parent_MPFR_LIBRARY}" "${parent_QD_LIBRARY}")
  if(dependency_library)
    get_filename_component(dependency_dir "${dependency_library}" DIRECTORY)
    if(EXISTS "${dependency_dir}/pkgconfig")
      list(APPEND pc_paths "${dependency_dir}/pkgconfig")
    endif()
  endif()
endforeach()
if(DEFINED ENV{PKG_CONFIG_PATH} AND NOT "$ENV{PKG_CONFIG_PATH}" STREQUAL "")
  list(APPEND pc_paths "$ENV{PKG_CONFIG_PATH}")
endif()
list(REMOVE_DUPLICATES pc_paths)
string(JOIN ":" pc_path ${pc_paths})

distcheck_step(pkg-location "${work}" OUTPUT_VARIABLE pkg_location
  COMMAND "${CMAKE_COMMAND}" -E env "PKG_CONFIG_PATH=${pc_path}"
    "${pkg_config}" --variable=pcfiledir fplll)
string(STRIP "${pkg_location}" pkg_location)
cmake_path(NORMAL_PATH pkg_location)
set(expected_pkg_location "${prefix}/${libdir}/pkgconfig")
cmake_path(NORMAL_PATH expected_pkg_location)
if(NOT pkg_location STREQUAL expected_pkg_location)
  message(FATAL_ERROR "pkg-config selected '${pkg_location}', expected '${expected_pkg_location}'")
endif()

file(WRITE "${work}/matrix.txt" "[[1 1][1 0]]\n")
distcheck_step(cli "${work}" INPUT "${work}/matrix.txt"
  COMMAND "${prefix}/bin/fplll" -a lll)

distcheck_step(uninstall "${work}" COMMAND "${CMAKE_COMMAND}" --build "${binary}"
  --config "${config}" --target uninstall)
assert_no_files("${prefix}" "uninstall")

# DESTDIR only stages the configured prefix; install and uninstall must agree.
set(staging "${work}/stage")
distcheck_step(stage-install "${work}" COMMAND "${CMAKE_COMMAND}" -E env
  "DESTDIR=${staging}" "${CMAKE_COMMAND}" --install "${binary}" --config "${config}")
if(NOT EXISTS "${staging}${prefix}/bin/fplll")
  message(FATAL_ERROR "DESTDIR installation did not stage into ${staging}")
endif()
distcheck_step(stage-uninstall "${work}" COMMAND "${CMAKE_COMMAND}" -E env
  "DESTDIR=${staging}" "${CMAKE_COMMAND}" --build "${binary}" --config "${config}" --target uninstall)
assert_no_files("${staging}" "Staged uninstall")

distcheck_step(clean "${work}" COMMAND "${CMAKE_COMMAND}" --build "${binary}"
  --config "${config}" --target clean)
distcheck_step(rebuild "${work}" COMMAND "${CMAKE_COMMAND}" --build "${binary}" --config "${config}")
distcheck_step(retest "${work}" COMMAND "${ctest}" --test-dir "${binary}" -C "${config}"
  --output-on-failure --no-tests=error)

source_digest("${source}" after)
if(NOT before STREQUAL after)
  message(FATAL_ERROR "The extracted source tree was modified; inspect ${work}")
endif()
file(SHA256 "${archive}" final_archive_digest)
if(NOT archive_digest STREQUAL final_archive_digest)
  message(FATAL_ERROR "The tested archive changed during distcheck: ${archive}")
endif()

file(WRITE "${check_dir}/passed.txt"
  "Archive: ${archive}\nSHA256: ${archive_digest}\nAll distcheck stages passed.\n")
file(CHMOD_RECURSE "${work}" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
file(REMOVE_RECURSE "${work}")
message(STATUS "distcheck passed: ${archive}\n  SHA256: ${archive_digest}\n  Logs: ${logs}")
