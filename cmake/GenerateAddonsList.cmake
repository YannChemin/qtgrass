# cmake/GenerateAddonsList.cmake
# Scans the grass-addons/src tree at CMake configure time via scan_addons.py
# and generates grassaddonslist.h with AddonInfo {name, label} for all modules.
#
# Usage (called from the top-level CMakeLists.txt):
#   include(GenerateAddonsList)
#   generate_addons_list("<addons_dir>" "<output_header>")

function(generate_addons_list addons_dir output_header)

    set(scanner "${CMAKE_SOURCE_DIR}/cmake/scan_addons.py")

    if(NOT EXISTS "${scanner}")
        message(WARNING "GrassAddons: scan_addons.py not found at ${scanner}")
        return()
    endif()

    find_package(Python3 QUIET COMPONENTS Interpreter)
    if(NOT Python3_FOUND)
        # Try plain "python3"
        find_program(PYTHON3_EXE python3)
        if(NOT PYTHON3_EXE)
            message(WARNING "GrassAddons: python3 not found — addon menus disabled")
            return()
        endif()
        set(Python3_EXECUTABLE ${PYTHON3_EXE})
    endif()

    # The .md lives at the repo root — use the source dir of the top-level cmake
    set(addons_md "${CMAKE_SOURCE_DIR}/ADDONS_MENU.md")
    message(STATUS "GrassAddons: scanning ${addons_dir}/src (md: ${addons_md})")

    execute_process(
        COMMAND "${Python3_EXECUTABLE}" "${scanner}" "${addons_dir}" "${output_header}" "${addons_md}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE out_msg
        ERROR_VARIABLE  err_msg
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_STRIP_TRAILING_WHITESPACE
    )

    if(result EQUAL 0)
        message(STATUS "GrassAddons: ${out_msg}")
    else()
        message(WARNING "GrassAddons: scan_addons.py failed: ${err_msg}")
        return()
    endif()

endfunction()
