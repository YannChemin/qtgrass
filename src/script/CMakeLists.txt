add_library(qtgrass_script STATIC grassscriptrunner.cpp)
target_include_directories(qtgrass_script PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(qtgrass_script PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets)
