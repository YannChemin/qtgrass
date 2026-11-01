add_library(qtgrass_profile STATIC
  grasschartwidget.cpp
  grassprofiledialog.cpp
  grasshistogramdialog.cpp
)
target_include_directories(qtgrass_profile PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(qtgrass_profile PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets)
