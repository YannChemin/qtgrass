add_library(qtgrass_planetary STATIC
  grasscraterdialog.cpp
  grasscraterdrawdialog.cpp
  grasscraterfreqdialog.cpp
  grassplanetarymenu.cpp
  grassspicekerneldialog.cpp
)

target_include_directories(qtgrass_planetary PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(qtgrass_planetary PUBLIC
  Qt6::Core
  Qt6::Gui
  Qt6::Widgets
  Qt6::Charts
  GRASS::GIS
  GRASS::Raster
)
