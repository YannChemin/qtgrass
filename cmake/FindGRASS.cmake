# FindGRASS.cmake
# Locates GRASS GIS headers and core libraries.
#
# Searches in order:
#   1. GRASS_PREFIX cmake variable or GRASS_PREFIX env var
#   2. grass --config path (running session)
#   3. Common install prefixes: /usr/local/grass8*, /usr/lib/grass8*
#
# Exported targets (if found):
#   GRASS::GIS      - libgrass_gis  + headers
#   GRASS::Raster   - libgrass_raster
#   GRASS::Vector   - libgrass_vector
#   GRASS::DBMI     - libgrass_dbmiclient + libgrass_dbmibase
#
# Variables set:
#   GRASS_FOUND
#   GRASS_PREFIX          - GISBASE directory (e.g. /usr/local/grass86)
#   GRASS_INCLUDE_DIR     - path containing grass/gis.h
#   GRASS_VERSION_STRING  - e.g. "8.6.0dev"
#   GRASS_VERSION_MAJOR
#   GRASS_VERSION_MINOR

cmake_minimum_required(VERSION 3.16)

# --- 1. Resolve prefix --------------------------------------------------
if(NOT GRASS_PREFIX)
  if(DEFINED ENV{GRASS_PREFIX})
    set(GRASS_PREFIX "$ENV{GRASS_PREFIX}")
  elseif(DEFINED ENV{GISBASE})
    set(GRASS_PREFIX "$ENV{GISBASE}")
  else()
    # Ask the running grass binary
    find_program(_grass_bin grass)
    if(_grass_bin)
      execute_process(
        COMMAND ${_grass_bin} --config path
        OUTPUT_VARIABLE _grass_path
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
      )
      if(_grass_path AND EXISTS "${_grass_path}/include/grass/gis.h")
        set(GRASS_PREFIX "${_grass_path}")
      endif()
    endif()
    # Fallback: glob common locations newest-first
    if(NOT GRASS_PREFIX)
      file(GLOB _candidates
        "/usr/local/grass8[0-9]*"
        "/usr/local/grass[0-9]*"
        "/usr/lib/grass8[0-9]*"
        "/opt/grass8[0-9]*"
      )
      list(SORT _candidates ORDER DESCENDING)
      foreach(_c IN LISTS _candidates)
        if(EXISTS "${_c}/include/grass/gis.h")
          set(GRASS_PREFIX "${_c}")
          break()
        endif()
      endforeach()
    endif()
  endif()
endif()

# --- 2. Find header directory -------------------------------------------
find_path(GRASS_INCLUDE_DIR
  NAMES grass/gis.h
  PATHS
    "${GRASS_PREFIX}/include"
    /usr/local/grass86/include
    /usr/lib/grass84/include
  NO_DEFAULT_PATH
)

# --- 3. Read version from version.h -------------------------------------
if(GRASS_INCLUDE_DIR)
  file(READ "${GRASS_INCLUDE_DIR}/grass/version.h" _ver_content)
  string(REGEX MATCH "GRASS_VERSION_STRING[^\"]*\"([^\"]+)\"" _ "${_ver_content}")
  set(GRASS_VERSION_STRING "${CMAKE_MATCH_1}")
  string(REGEX MATCH "GRASS_VERSION_MAJOR[^0-9]*([0-9]+)" _ "${_ver_content}")
  set(GRASS_VERSION_MAJOR "${CMAKE_MATCH_1}")
  string(REGEX MATCH "GRASS_VERSION_MINOR[^0-9]*([0-9]+)" _ "${_ver_content}")
  set(GRASS_VERSION_MINOR "${CMAKE_MATCH_1}")
endif()

# --- 4. Find libraries --------------------------------------------------
set(_grass_libdir "${GRASS_PREFIX}/lib")

macro(_find_grass_lib _target _libname)
  find_library(GRASS_${_target}_LIBRARY
    NAMES ${_libname}
    PATHS "${_grass_libdir}"
    NO_DEFAULT_PATH
  )
endmacro()

_find_grass_lib(GIS      grass_gis)
_find_grass_lib(RASTER   grass_raster)
_find_grass_lib(VECTOR   grass_vector)
_find_grass_lib(VEDIT    grass_vedit)
_find_grass_lib(DBMIBASE grass_dbmibase)
_find_grass_lib(DBMI     grass_dbmiclient)

# --- 5. Standard find_package machinery ---------------------------------
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GRASS
  REQUIRED_VARS
    GRASS_INCLUDE_DIR
    GRASS_GIS_LIBRARY
    GRASS_RASTER_LIBRARY
    GRASS_VECTOR_LIBRARY
  VERSION_VAR GRASS_VERSION_STRING
)

# --- 6. Exported targets ------------------------------------------------
if(GRASS_FOUND AND NOT TARGET GRASS::GIS)
  add_library(GRASS::GIS SHARED IMPORTED)
  set_target_properties(GRASS::GIS PROPERTIES
    IMPORTED_LOCATION "${GRASS_GIS_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${GRASS_INCLUDE_DIR}"
  )

  add_library(GRASS::Raster SHARED IMPORTED)
  set_target_properties(GRASS::Raster PROPERTIES
    IMPORTED_LOCATION "${GRASS_RASTER_LIBRARY}"
    INTERFACE_LINK_LIBRARIES GRASS::GIS
  )

  add_library(GRASS::Vector SHARED IMPORTED)
  set_target_properties(GRASS::Vector PROPERTIES
    IMPORTED_LOCATION "${GRASS_VECTOR_LIBRARY}"
    INTERFACE_LINK_LIBRARIES GRASS::GIS
  )

  if(GRASS_VEDIT_LIBRARY)
    add_library(GRASS::Vedit SHARED IMPORTED)
    set_target_properties(GRASS::Vedit PROPERTIES
      IMPORTED_LOCATION "${GRASS_VEDIT_LIBRARY}"
      INTERFACE_LINK_LIBRARIES GRASS::Vector
    )
  endif()

  if(GRASS_DBMI_LIBRARY AND GRASS_DBMIBASE_LIBRARY)
    add_library(GRASS::DBMI SHARED IMPORTED)
    set_target_properties(GRASS::DBMI PROPERTIES
      IMPORTED_LOCATION "${GRASS_DBMI_LIBRARY}"
      INTERFACE_LINK_LIBRARIES "GRASS::GIS;${GRASS_DBMIBASE_LIBRARY}"
    )
  endif()
endif()

mark_as_advanced(
  GRASS_INCLUDE_DIR
  GRASS_GIS_LIBRARY GRASS_RASTER_LIBRARY GRASS_VECTOR_LIBRARY
  GRASS_DBMI_LIBRARY GRASS_DBMIBASE_LIBRARY
)
