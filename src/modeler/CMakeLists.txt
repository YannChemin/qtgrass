add_library(qtgrass_modeler STATIC
  grassmodel.cpp
  grassmodeleritems.cpp
  grassmodelerdialog.cpp
)

target_include_directories(qtgrass_modeler PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

target_link_libraries(qtgrass_modeler PUBLIC
  Qt6::Core
  Qt6::Gui
  Qt6::Widgets
  Qt6::Xml
)
