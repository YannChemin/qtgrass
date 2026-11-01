add_library(qtgrass_startup STATIC
  grasscrsmodel.cpp
  grassmapsetbrowser.cpp
  grasslocationwizard.cpp
  grassstartupdialog.cpp
)

target_include_directories(qtgrass_startup PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_compile_definitions(qtgrass_startup PRIVATE
  QTGRASS_VERSION="${PROJECT_VERSION}"
)

# PROJ 9 C API — for authoritative ellipsoid queries in fixupProjInfo()
find_package(PROJ REQUIRED CONFIG)

target_link_libraries(qtgrass_startup PUBLIC
  Qt6::Core
  Qt6::Gui
  Qt6::Widgets
  Qt6::Sql      # for proj.db / SQLite queries
  GRASS::GIS    # for grass/version.h
  PROJ::proj    # for proj.h / proj_get_ellipsoid() etc.
)
