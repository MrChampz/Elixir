function (copy_runtime_dlls_for_targets engine_target)
    get_property(all_targets GLOBAL PROPERTY TARGETS)
    foreach(target IN LISTS all_targets)
        get_target_property(target_type "${target}" TYPE)
        if (target_type STREQUAL "EXECUTABLE")
            message(STATUS "Adding post-build DLL copy for target: ${target}")
            add_custom_command(
                    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${target}_copy_runtime_dlls.stamp"
                    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    $<TARGET_RUNTIME_DLLS:${target}>
                    "$<TARGET_FILE_DIR:${target}>"
                    COMMAND "${CMAKE_COMMAND}" -E touch
                    "${CMAKE_CURRENT_BINARY_DIR}/${target}_copy_runtime_dlls.stamp"
                    COMMAND_EXPAND_LISTS
                    COMMENT "Copying runtime DLLs for ${target}..."
                    VERBATIM
            )

            add_custom_target("${target}_copy_dlls" DEPENDS
                "${CMAKE_CURRENT_BINARY_DIR}/${target}_copy_runtime_dlls.stamp"
            )
            add_dependencies("${target}_copy_dlls" "${engine_target}")
            add_dependencies(${target} "${target}_copy_dlls")
        endif()
    endforeach()
endfunction()