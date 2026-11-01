add_library(qtgrass_tools STATIC
  grassmapswipe.cpp
  grasswmsbrowser.cpp
  grassnetanalysis.cpp
  grasshistorybrowser.cpp
  grasstplot.cpp
)

target_include_directories(qtgrass_tools PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

target_link_libraries(qtgrass_tools PUBLIC
  Qt6::Core
  Qt6::Gui
  Qt6::Widgets
  Qt6::Network
  Qt6::Xml
  qtgrass_module        # for GrassModuleDialog
)
