add_library(qtgrass_temporal STATIC
  grasstemporaldataset.cpp
  grassanimationdialog.cpp
)

target_include_directories(qtgrass_temporal PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

target_link_libraries(qtgrass_temporal PUBLIC
  Qt6::Core
  Qt6::Gui
  Qt6::Widgets
)
