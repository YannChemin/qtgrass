add_library(qtgrass_region STATIC grassregiondialog.cpp)
target_include_directories(qtgrass_region PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(qtgrass_region PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets)
