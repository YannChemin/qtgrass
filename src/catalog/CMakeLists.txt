add_library(qtgrass_catalog STATIC grassdatacat.cpp)
target_include_directories(qtgrass_catalog PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(qtgrass_catalog PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets)
