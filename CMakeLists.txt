cmake_minimum_required(VERSION 3.22)
project(qtgrass VERSION 0.5.8 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# ---------------------------------------------------------------------------
# Qt6 — only base modules needed for Phase 1 (module dialog)
# Phase 2 (map canvas) will add Qt6::OpenGL etc. later
# ---------------------------------------------------------------------------
find_package(Qt6 6.2 REQUIRED COMPONENTS
  Core
  Gui
  Widgets
  Xml
  Concurrent
  Network
  Sql
  PrintSupport
  Svg
  Charts
)

# ---------------------------------------------------------------------------
# GRASS GIS — prefer 8.6 dev build; falls back to packaged 8.4
# Override with -DGRASS_PREFIX=/path/to/grass at configure time
# ---------------------------------------------------------------------------
find_package(GRASS REQUIRED)
message(STATUS "GRASS ${GRASS_VERSION_STRING} found at ${GRASS_PREFIX}")

# ---------------------------------------------------------------------------
# GRASS Addons — scan source tree at configure time and generate module list
# ---------------------------------------------------------------------------
# Default: the invoking user's ~/dev/grass-addons — but debian/rules passes
# -DGRASS_ADDONS_DIR explicitly to avoid $ENV{HOME} sandbox issues.
set(GRASS_ADDONS_DIR "" CACHE PATH "Path to the grass-addons source tree (optional)")
if(NOT GRASS_ADDONS_DIR)
    # Resolve the real HOME via shell so debhelper sandbox does not corrupt it
    execute_process(COMMAND sh -c "eval echo ~$(whoami)"
                    OUTPUT_VARIABLE _real_home OUTPUT_STRIP_TRAILING_WHITESPACE)
    set(GRASS_ADDONS_DIR "${_real_home}/dev/grass-addons")
endif()

set(ADDONS_LIST_HEADER "${CMAKE_BINARY_DIR}/grassaddonslist.h")

if(EXISTS "${GRASS_ADDONS_DIR}/src")
    include(GenerateAddonsList)
    generate_addons_list("${GRASS_ADDONS_DIR}" "${ADDONS_LIST_HEADER}")
    add_compile_definitions(HAVE_GRASS_ADDONS)
    include_directories("${CMAKE_BINARY_DIR}")
    message(STATUS "GrassAddons: addons list header written to ${ADDONS_LIST_HEADER}")
else()
    message(STATUS "GrassAddons: ${GRASS_ADDONS_DIR}/src not found — addon menus disabled. "
                   "Set -DGRASS_ADDONS_DIR=<path> to enable.")
endif()

# ---------------------------------------------------------------------------
# Compiler flags
# ---------------------------------------------------------------------------
add_compile_options(-Wall -Wextra -Wpedantic)

# GRASS headers use C99 flexible array members; silence pedantic noise
add_compile_options($<$<COMPILE_LANGUAGE:C>:-Wno-pedantic>)

# ---------------------------------------------------------------------------
# Subdirectories
# ---------------------------------------------------------------------------
add_subdirectory(src)

# ---------------------------------------------------------------------------
# Documentation (optional — requires doxygen in PATH)
# ---------------------------------------------------------------------------
find_package(Doxygen QUIET)
if(DOXYGEN_FOUND)
    add_custom_target(doxy
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/doc/api
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_SOURCE_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM
    )
    message(STATUS "Documentation target 'doxy' available — run: cmake --build <build-dir> --target doxy")
else()
    message(STATUS "Doxygen not found — 'doxy' target unavailable")
endif()
