add_library(qtgrass_export STATIC grassmapexport.cpp)
target_include_directories(qtgrass_export PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(qtgrass_export PRIVATE HAVE_QT6SVG)
target_link_libraries(qtgrass_export PUBLIC
  Qt6::Core Qt6::Gui Qt6::Widgets Qt6::PrintSupport Qt6::Svg)
