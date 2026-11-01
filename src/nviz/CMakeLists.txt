find_package(Qt6 REQUIRED COMPONENTS OpenGL OpenGLWidgets)

add_library(qtgrass_nviz STATIC
  grassnvizwidget.cpp
  grassnvizdialog.cpp
)

target_include_directories(qtgrass_nviz PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/.."
  /usr/include/gdal          # transitive from canvas
)

target_link_libraries(qtgrass_nviz PUBLIC
  Qt6::Core
  Qt6::Gui
  Qt6::Widgets
  Qt6::OpenGL
  Qt6::OpenGLWidgets
  GRASS::GIS
  GRASS::Raster
)
