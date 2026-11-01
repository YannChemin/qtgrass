add_library(qtgrass_imagery STATIC
  grasstrainingcanvas.cpp
  grassscatterwidget.cpp
  grassiclassdialog.cpp
)

target_include_directories(qtgrass_imagery PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/.."
  /usr/include/gdal        # transitive from georect/grassgcpcanvas
)

target_link_libraries(qtgrass_imagery PUBLIC
  Qt6::Core
  Qt6::Gui
  Qt6::Widgets
  qtgrass_georect
)
