function(fplll_add_documentation)
  find_package(Doxygen 1.9.8 QUIET OPTIONAL_COMPONENTS dot)
  if(NOT Doxygen_FOUND)
    message(STATUS "Doxygen 1.9.8 or newer not found; the doc target is unavailable")
    return()
  endif()

  set(DOXYGEN_PROJECT_BRIEF "Lattice algorithms using floating-point arithmetic")
  set(DOXYGEN_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/doc")
  set(DOXYGEN_USE_MDFILE_AS_MAINPAGE "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
  set(DOXYGEN_EXCLUDE
    "${CMAKE_CURRENT_SOURCE_DIR}/fplll/nr/dpe.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/fplll/io/json.hpp"
    "${CMAKE_CURRENT_BINARY_DIR}")
  set(DOXYGEN_INCLUDE_PATH "${FPLLL_CONFIG_INCLUDE_DIR}")
  # Keep undocumented API entries visible while preserving namespace scopes.
  set(DOXYGEN_EXTRACT_ALL YES)
  set(DOXYGEN_EXTRACT_STATIC YES)
  set(DOXYGEN_BUILTIN_STL_SUPPORT YES)
  # Existing API comments use TeX-style paired quotes.
  set(DOXYGEN_MARKDOWN_STRICT NO)
  set(DOXYGEN_MARKDOWN_ID_STYLE GITHUB)
  set(DOXYGEN_MACRO_EXPANSION YES)
  set(DOXYGEN_EXPAND_ONLY_PREDEF YES)
  set(DOXYGEN_PREDEFINED
    "FPLLL_BEGIN_NAMESPACE=namespace fplll {"
    "FPLLL_END_NAMESPACE=}")
  set(DOXYGEN_GENERATE_TREEVIEW YES)
  # Keep member-index and email links static.
  set(DOXYGEN_HTML_DYNAMIC_MENUS NO)
  set(DOXYGEN_OBFUSCATE_EMAILS NO)
  set(DOXYGEN_USE_MATHJAX YES)
  set(DOXYGEN_MATHJAX_VERSION MathJax_3)
  set(DOXYGEN_MATHJAX_FORMAT SVG)
  set(DOXYGEN_MATHJAX_RELPATH "https://cdn.jsdelivr.net/npm/mathjax@3.2.2")
  set(DOXYGEN_DOT_IMAGE_FORMAT svg)
  set(DOXYGEN_CLASS_GRAPH NO)
  set(DOXYGEN_COLLABORATION_GRAPH NO)
  set(DOXYGEN_GROUP_GRAPHS NO)
  set(DOXYGEN_INCLUDE_GRAPH NO)
  doxygen_add_docs(doc
    "${CMAKE_CURRENT_SOURCE_DIR}/fplll"
    "${CMAKE_CURRENT_SOURCE_DIR}/README.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/CONTRIBUTING.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/CHANGELOG.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/doc/usage.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/doc/building.md")
endfunction()
