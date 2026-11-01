add_library(qtgrass_layers STATIC
  grasslayertree.cpp
  grasslayerprops.cpp
)

target_include_directories(qtgrass_layers PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/.."    # for canvas/, colors/, profile/ headers
)

target_link_libraries(qtgrass_layers PUBLIC
  Qt6::Core
  Qt6::Gui
  Qt6::Widgets
  GRASS::GIS
)
