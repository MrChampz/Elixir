function (link_target_to_engine target)
    target_link_libraries(${target} PRIVATE Elixir)

    if (WIN32)
        get_target_property(target_type "${target}" TYPE)
        if (target_type STREQUAL "EXECUTABLE")
            add_custom_target("${target}_copy_runtime_dlls"
                COMMAND "${CMAKE_COMMAND}" -E make_directory
                "$<TARGET_FILE_DIR:${target}>"
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:Elixir>"
                "$<TARGET_FILE_DIR:${target}>"
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                $<TARGET_RUNTIME_DLLS:${target}>
                "$<TARGET_FILE_DIR:${target}>"
                COMMAND_EXPAND_LISTS
                COMMENT "Copying runtime DLLs for ${target}..."
                VERBATIM
            )
            add_dependencies("${target}_copy_runtime_dlls" Elixir)
            add_dependencies(${target} "${target}_copy_runtime_dlls")
        endif()
    endif()

    get_property(dependents GLOBAL PROPERTY "ELIXIR_DEPENDENTS" SET)
    if (NOT dependents)
        set_property(GLOBAL PROPERTY "ELIXIR_DEPENDENTS" "")
    endif()

    get_property(dependents GLOBAL PROPERTY "ELIXIR_DEPENDENTS")
    list(APPEND dependents ${target})
    set_property(GLOBAL PROPERTY "ELIXIR_DEPENDENTS" "${dependents}")
endfunction()

function (copy_runtime_dlls_for_targets)
    # Runtime DLL copying is attached in link_target_to_engine(), where CMake allows
    # TARGET POST_BUILD commands because the consumer target is in the current directory.
endfunction()
