add_library(qtgrass_attrib STATIC
  grassattribtable.cpp
  grassdbmanager.cpp
  grassfieldcalc.cpp
)
target_include_directories(qtgrass_attrib PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/.."
)
target_link_libraries(qtgrass_attrib PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets)
