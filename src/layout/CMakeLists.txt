add_library(qtgrass_layout STATIC
  grasslayoutitems.cpp
  grasslayoutdialog.cpp
)

target_include_directories(qtgrass_layout PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/.."
  /usr/include/gdal          # transitive from canvas dependency
)

target_link_libraries(qtgrass_layout PUBLIC
  Qt6::Core
  Qt6::Gui
  Qt6::Widgets
  Qt6::Xml
  Qt6::PrintSupport
  Qt6::Svg
  qtgrass_canvas
)
