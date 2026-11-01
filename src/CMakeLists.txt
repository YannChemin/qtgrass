add_subdirectory(tiles)
add_subdirectory(workspace)
add_subdirectory(module)
add_subdirectory(toolbox)
add_subdirectory(modeler)
add_subdirectory(temporal)
add_subdirectory(layout)
add_subdirectory(georect)
add_subdirectory(imagery)
add_subdirectory(tools)
add_subdirectory(nviz)
add_subdirectory(canvas)
add_subdirectory(startup)
add_subdirectory(mapcalc)
add_subdirectory(attrib)
add_subdirectory(profile)
add_subdirectory(layers)
add_subdirectory(vdigit)
add_subdirectory(export)
add_subdirectory(colors)
add_subdirectory(addon)
add_subdirectory(catalog)
add_subdirectory(region)
add_subdirectory(help)
add_subdirectory(script)
add_subdirectory(planetary)

find_package(PROJ REQUIRED CONFIG)

add_executable(qtgrass
  main.cpp
  grassenv.cpp
  grassmessagebar.cpp
  grassinterface.cpp
  grassapp.cpp
  grassmenubuilder.cpp
)

target_include_directories(qtgrass PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_compile_definitions(qtgrass PRIVATE
  QTGRASS_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(qtgrass PRIVATE
  Qt6::Core
  Qt6::Gui
  Qt6::Network
  Qt6::Widgets
  Qt6::Xml
  Qt6::Concurrent
  GRASS::GIS
  PROJ::proj
  GRASS::Raster
  GRASS::Vector
  qtgrass_tiles
  qtgrass_module
  qtgrass_toolbox
  qtgrass_canvas
  qtgrass_startup
  qtgrass_mapcalc
  qtgrass_attrib
  qtgrass_profile
  qtgrass_export
  qtgrass_colors
  qtgrass_layers
  qtgrass_vdigit
  qtgrass_modeler
  qtgrass_temporal
  qtgrass_layout
  qtgrass_georect
  qtgrass_imagery
  qtgrass_workspace
  qtgrass_tools
  qtgrass_nviz
  qtgrass_addon
  qtgrass_catalog
  qtgrass_region
  qtgrass_help
  qtgrass_script
  qtgrass_planetary
)







set_target_properties(qtgrass PROPERTIES
  INSTALL_RPATH "${GRASS_PREFIX}/lib"
  BUILD_RPATH   "${GRASS_PREFIX}/lib"
)

# Bake the GRASS installation path so qtgrass can find GISBASE even when
# launched from a desktop shortcut without a GRASS session active.
target_compile_definitions(qtgrass PRIVATE
  QTGRASS_DEFAULT_GISBASE="${GRASS_PREFIX}"
)

include(GNUInstallDirs)
install(TARGETS qtgrass
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
