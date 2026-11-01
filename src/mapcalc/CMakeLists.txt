add_library(qtgrass_mapcalc STATIC
  grassmapcalcdialog.cpp
)

target_include_directories(qtgrass_mapcalc PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(qtgrass_mapcalc PUBLIC
  Qt6::Core
  Qt6::Gui
  Qt6::Widgets
  GRASS::GIS
)
