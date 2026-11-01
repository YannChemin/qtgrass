add_library(qtgrass_georect STATIC
  grassgcpcanvas.cpp
  grassgeorectdialog.cpp
)

target_include_directories(qtgrass_georect PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

target_link_libraries(qtgrass_georect PUBLIC
  Qt6::Core
  Qt6::Gui
  Qt6::Widgets
)
