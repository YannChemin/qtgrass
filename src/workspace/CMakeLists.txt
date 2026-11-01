add_library(qtgrass_workspace STATIC
  grassworkspace.cpp
)
target_include_directories(qtgrass_workspace PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/.."
)
target_link_libraries(qtgrass_workspace PUBLIC Qt6::Core Qt6::Xml)
