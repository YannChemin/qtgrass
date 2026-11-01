add_library(qtgrass_addon STATIC grassaddonmanager.cpp)
target_include_directories(qtgrass_addon PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(qtgrass_addon PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets)
