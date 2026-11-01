add_library(qtgrass_tiles STATIC
  grasstilecache.cpp
  grasstilefetcher.cpp
  grasstilebackground.cpp
  grasspreferencesdialog.cpp
)

target_include_directories(qtgrass_tiles PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}"
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(PROJ REQUIRED proj)
pkg_check_modules(GDAL REQUIRED gdal)

target_include_directories(qtgrass_tiles PRIVATE ${PROJ_INCLUDE_DIRS} ${GDAL_INCLUDE_DIRS})
target_link_libraries(qtgrass_tiles PUBLIC
  Qt6::Core
  Qt6::Gui
  Qt6::Network
  Qt6::Widgets
  qtgrass_canvas   # for GrassMapLayer / GrassExtent
  GRASS::GIS
  ${PROJ_LIBRARIES}
  ${GDAL_LIBRARIES}
  "${GRASS_PREFIX}/lib/libgrass_gproj.so"
)
