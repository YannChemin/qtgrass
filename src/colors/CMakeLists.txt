add_library(qtgrass_colors STATIC grasscolortabledialog.cpp)
target_include_directories(qtgrass_colors PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(qtgrass_colors PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets GRASS::GIS GRASS::Raster)
