function (link_target_to_engine target)
    target_link_libraries(${target} PRIVATE Elixir)

    get_property(dependents GLOBAL PROPERTY "ELIXIR_DEPENDENTS" SET)
    if (NOT dependents)
        set_property(GLOBAL PROPERTY "ELIXIR_DEPENDENTS" "")
    endif()

    get_property(dependents GLOBAL PROPERTY "ELIXIR_DEPENDENTS")
    list(APPEND dependents ${target})
    set_property(GLOBAL PROPERTY "ELIXIR_DEPENDENTS" "${dependents}")
endfunction()