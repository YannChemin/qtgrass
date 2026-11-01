add_library(qtgrass_module STATIC
  grassxmlparser.cpp
  grassparamwidget.cpp
  grassmoduledialog.cpp
)

target_include_directories(qtgrass_module PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/.."   # for grassinterface.h / grassmessagebar.h
)

target_link_libraries(qtgrass_module PUBLIC
  Qt6::Core
  Qt6::Gui
  Qt6::Widgets
  Qt6::Xml
)
# Help dialog is in a sibling subdirectory — link at top-level executable
target_include_directories(qtgrass_module PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/.."
)
