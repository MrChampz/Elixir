project("Dissolve")

# Files
add_executable(${PROJECT_NAME}
    ${CMAKE_CURRENT_LIST_DIR}/Source/Dissolve.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/Dissolve.cpp
)

# Set output name
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${PROJECT_NAME}")

# Properties

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE False
    INTERPROCEDURAL_OPTIMIZATION False
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${OUTPUT_DIR}/${PROJECT_NAME}"
    LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIR}/${PROJECT_NAME}"
    RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}/${PROJECT_NAME}"
)

# Compile flags

if (BUILD_TYPE STREQUAL "Debug")
    set_target_properties(${PROJECT_NAME} PROPERTIES
        CMAKE_CXX_FLAGS_DEBUG "-g"  # For GCC/Clang to enable debugging symbols
        CMAKE_C_FLAGS_DEBUG "-g"    # For GCC/Clang to enable debugging symbols
    )

    if(MSVC)
        set_target_properties(${PROJECT_NAME} PROPERTIES
            CMAKE_CXX_FLAGS_DEBUG "/MTd"  # For MSVC Debug runtime
            CMAKE_C_FLAGS_DEBUG "/MTd"    # For MSVC Debug runtime
        )
    endif()
elseif (BUILD_TYPE STREQUAL "Release" OR BUILD_TYPE STREQUAL "Dist")
    set_target_properties(${PROJECT_NAME} PROPERTIES
        CMAKE_CXX_FLAGS_RELEASE "-O3"  # Enable optimizations for GCC/Clang
        CMAKE_C_FLAGS_RELEASE "-O3"    # Enable optimizations for GCC/Clang
    )

    if(MSVC)
        set_target_properties(${PROJECT_NAME} PROPERTIES
            CMAKE_CXX_FLAGS_RELEASE "/O2"  # For MSVC Release optimizations
            CMAKE_C_FLAGS_RELEASE "/O2"    # For MSVC Release optimizations
        )
    endif()
endif()

# Compile definitions

target_compile_definitions(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Debug>:EE_DEBUG>
    $<$<CONFIG:Release>:EE_RELEASE>
    $<$<CONFIG:Dist>:EE_DIST>
)

if (WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        _CRT_SECURE_NO_WARNINGS
        EE_PLATFORM_WINDOWS
        EE_BUILD_DLL
    )
elseif (APPLE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        EE_PLATFORM_MACOS
    )
endif()

target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<COMPILE_LANG_AND_ID:C,MSVC>:/MP>
    $<$<CONFIG:Release>:$<$<COMPILE_LANG_AND_ID:C,MSVC>:/Ot>>
    $<$<CONFIG:Release>:$<$<COMPILE_LANG_AND_ID:C,MSVC>:/MD>>
    $<$<CONFIG:Debug>:$<$<COMPILE_LANG_AND_ID:C,MSVC>:/MDd>>
    $<$<CONFIG:Debug>:$<$<COMPILE_LANG_AND_ID:C,MSVC>:/Z7>>
    $<$<CONFIG:Dist>:$<$<COMPILE_LANG_AND_ID:C,MSVC>:/Ot>>
    $<$<CONFIG:Dist>:$<$<COMPILE_LANG_AND_ID:C,MSVC>:/MD>>
    $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/MP>
    $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/std:c++20>
    $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/EHsc>
    $<$<CONFIG:Release>:$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/Ot>>
    $<$<CONFIG:Release>:$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/MD>>
    $<$<CONFIG:Debug>:$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/MDd>>
    $<$<CONFIG:Debug>:$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/Z7>>
    $<$<CONFIG:Dist>:$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/Ot>>
    $<$<CONFIG:Dist>:$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/MD>>
)

# Include dirs

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/Elixir/Source
)

# Linking

add_dependencies(${PROJECT_NAME}
    Elixir
)

target_link_libraries(${PROJECT_NAME}
    Elixir
)