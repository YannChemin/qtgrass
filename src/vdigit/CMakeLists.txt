add_library(qtgrass_vdigit STATIC
  grassvectoreditor.cpp
  grassdigittoolbar.cpp
)

target_include_directories(qtgrass_vdigit PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/.."
  /usr/include/gdal            # needed by GRASS vector.h -> dig_structs.h -> ogr_api.h
)

target_link_libraries(qtgrass_vdigit PUBLIC
  Qt6::Core
  Qt6::Gui
  Qt6::Widgets
  GRASS::GIS
  GRASS::Vector
  GRASS::Vedit
)
