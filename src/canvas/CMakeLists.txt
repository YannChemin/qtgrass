add_library(qtgrass_canvas STATIC
  grassrastermaplayer.cpp
  grassvectormaplayer.cpp
  grassdisplaydriver.cpp
  grassdecorationpanel.cpp
  grassmapcanvas.cpp
  grasslayerpanel.cpp
  grasscompositelayers.cpp
  grassaddlayerdialog.cpp
)

target_include_directories(qtgrass_canvas PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}"
  /usr/include/gdal      # needed by GRASS vector.h → dig_structs.h → ogr_api.h
)

target_link_libraries(qtgrass_canvas PUBLIC
  Qt6::Core
  Qt6::Concurrent
  Qt6::Gui
  Qt6::Widgets
  GRASS::GIS
  GRASS::Raster
  GRASS::Vector
)
