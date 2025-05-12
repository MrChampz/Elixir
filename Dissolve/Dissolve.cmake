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
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${OUTPUT_DIR}/${PROJECT_NAME}"
    LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIR}/${PROJECT_NAME}"
    RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}/${PROJECT_NAME}"
)

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
    )
elseif (APPLE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        EE_PLATFORM_MACOS
    )
endif()

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