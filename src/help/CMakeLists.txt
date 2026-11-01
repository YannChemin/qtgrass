add_library(qtgrass_help STATIC grasshelpdialog.cpp)
target_include_directories(qtgrass_help PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(qtgrass_help PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets)
