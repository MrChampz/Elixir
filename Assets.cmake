# Asset Copying
# Copies the root Assets/ directory into each dependent target's output directory,
# Source: Assets/
# Output: Copied into each dependent target's output directory as Assets/

set(ASSETS_SOURCE_DIR "${CMAKE_SOURCE_DIR}/Assets")

# Rebuild the copy whenever any file under Assets/ changes.
file(GLOB_RECURSE ALL_ASSET_FILES "${ASSETS_SOURCE_DIR}/*")

# --- Copy assets into each engine-dependent target's output directory ---
# Call copy_assets_for_targets() after all targets have been defined (from root CMakeLists.txt).
# Each target gets: ${OUTPUT_DIR}/${target}/Assets/
#
# Uses rm -rf + copy_directory, same as copy_shaders_for_targets(), so assets
# renamed or deleted at the source don't linger as stale orphans in the output.

function(copy_assets_for_targets)
    get_property(dependents GLOBAL PROPERTY "ELIXIR_DEPENDENTS")
    foreach(target IN LISTS dependents)
        set(TARGET_ASSETS_DIR "${OUTPUT_DIR}/${target}/Assets")

        add_custom_command(
            OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${target}_copy_assets.stamp"
            COMMAND "${CMAKE_COMMAND}" -E rm -rf "${TARGET_ASSETS_DIR}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${TARGET_ASSETS_DIR}"
            COMMAND "${CMAKE_COMMAND}" -E copy_directory
                    "${ASSETS_SOURCE_DIR}"
                    "${TARGET_ASSETS_DIR}"
            COMMAND "${CMAKE_COMMAND}" -E touch
                    "${CMAKE_CURRENT_BINARY_DIR}/${target}_copy_assets.stamp"
            DEPENDS ${ALL_ASSET_FILES}
            COMMENT "Copying assets to ${TARGET_ASSETS_DIR}..."
            VERBATIM
        )

        add_custom_target("${target}_copy_assets" DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/${target}_copy_assets.stamp"
        )

        add_dependencies(${target} "${target}_copy_assets")

        message(STATUS "Adding asset copy for target: ${target} -> ${TARGET_ASSETS_DIR}")
    endforeach()
endfunction()
