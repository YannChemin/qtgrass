add_library(qtgrass_toolbox STATIC
  grasstoolboxtree.cpp
)

target_include_directories(qtgrass_toolbox PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(qtgrass_toolbox PUBLIC
  Qt6::Core
  Qt6::Gui
  Qt6::Widgets
  Qt6::Xml
)
