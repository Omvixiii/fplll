# An empty cache entry keeps automatic versions fresh on every configure.
set(FPLLL_VERSION "" CACHE STRING "Release version supplied by CI or a package builder (X.Y.Z)")

function(fplll_resolve_version)
  if(NOT "${FPLLL_VERSION}" STREQUAL "")
    set(version "${FPLLL_VERSION}")
    set(version_info "${version}")
  elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/FplllArchiveVersion.cmake")
    # CPack records the configured version in the archive, never in the checkout.
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/FplllArchiveVersion.cmake")
    set(version "${FPLLL_ARCHIVE_VERSION}")
    set(version_info "${FPLLL_ARCHIVE_VERSION_INFO}")
  else()
    set(version "0.0.0")
    set(version_info "0.0.0-dev")
    find_package(Git QUIET)
    if(Git_FOUND)
      execute_process(
        COMMAND "${GIT_EXECUTABLE}" rev-parse --show-toplevel
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE git_root OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE git_result ERROR_QUIET)
      if(git_result EQUAL 0)
        file(REAL_PATH "${git_root}" git_root)
        file(REAL_PATH "${CMAKE_CURRENT_SOURCE_DIR}" source_root)
        # An unpacked source tree must not inherit an unrelated parent repository.
        if(git_root STREQUAL source_root)
          execute_process(
            COMMAND "${GIT_EXECUTABLE}" tag --merged HEAD
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            OUTPUT_VARIABLE git_tags OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
          string(REPLACE "\n" ";" git_tags "${git_tags}")
          set(tag_filters)
          foreach(tag IN LISTS git_tags)
            if(tag MATCHES "^v?[0-9]+\\.[0-9]+\\.[0-9]+$")
              list(APPEND tag_filters --match "${tag}")
            endif()
          endforeach()
          if(NOT tag_filters)
            set(tag_filters --exclude "*")
          endif()
          execute_process(
            COMMAND "${GIT_EXECUTABLE}" describe --tags --long --always --dirty ${tag_filters}
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            OUTPUT_VARIABLE description OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
          string(REGEX REPLACE "^v" "" description "${description}")
          if(description MATCHES "^([0-9]+\\.[0-9]+\\.[0-9]+)-([0-9]+)-g([0-9a-f]+)(-dirty)?$")
            set(version "${CMAKE_MATCH_1}")
            if(CMAKE_MATCH_2 EQUAL 0 AND "${CMAKE_MATCH_4}" STREQUAL "")
              set(version_info "${version}")
            else()
              set(version_info "${description}")
            endif()
          elseif(description MATCHES "^[0-9a-f]+(-dirty)?$")
            set(version_info "0.0.0-g${description}")
          endif()
        endif()
      endif()
    endif()
  endif()

  if(NOT version MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "FPLLL_VERSION must be X.Y.Z, got: ${version}")
  endif()
  set(FPLLL_VERSION "${version}" PARENT_SCOPE)
  set(FPLLL_VERSION_INFO "${version_info}" PARENT_SCOPE)
endfunction()

fplll_resolve_version()
