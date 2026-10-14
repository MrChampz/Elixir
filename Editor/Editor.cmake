include (Utils.cmake)

project("Editor")

# Files
file(GLOB_RECURSE SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/Source/*.h"
    "${CMAKE_CURRENT_LIST_DIR}/Source/*.cpp"
)
# Reuse Dissolve's HDR environment baking for the Editor scene.
list(APPEND SOURCES
    "${CMAKE_SOURCE_DIR}/Dissolve/Source/Environment.cpp"
    "${CMAKE_SOURCE_DIR}/Elixir/Vendor/stb/stb_image.cpp"
)
add_executable(${PROJECT_NAME} ${SOURCES})

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
    ELIXIR_EDITOR_PROJECT_ROOT="${CMAKE_SOURCE_DIR}"
    $<$<BOOL:${ELIXIR_PROFILE}>:EE_PROFILE>
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

if (APPLE)
    target_compile_options(${PROJECT_NAME} PRIVATE -stdlib=libc++)
    target_link_options(${PROJECT_NAME} PRIVATE -stdlib=libc++)
endif()

# Include dirs

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/Elixir/Source
    ${CMAKE_SOURCE_DIR}/Dissolve/Source
    ${CMAKE_SOURCE_DIR}/Elixir/Vendor/stb
)

# Linking

add_dependencies(${PROJECT_NAME}
    Elixir
)

link_target_to_engine(${PROJECT_NAME})

if (BUILD_TESTING)
    add_executable(Editor.UnitTests
        "${CMAKE_CURRENT_LIST_DIR}/Tests/VFXAssetsTest.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Source/Assets/VFXAssets.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Tests/VFXDocumentTest.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Source/VFX/VFXDocument.cpp"
    )
    target_include_directories(Editor.UnitTests PRIVATE "${CMAKE_CURRENT_LIST_DIR}/Source")
    target_link_libraries(Editor.UnitTests PRIVATE GTest::gtest_main simdjson)
    include(GoogleTest)
    gtest_discover_tests(Editor.UnitTests)
endif()

# Application shaders belong to Editor and are never packaged with Engine targets.
if (DXC)
    set(EDITOR_SHADER_OUTPUTS "")
    foreach(stage vs ps)
        get_dxc_profile("${stage}" profile)
        set(source "${CMAKE_CURRENT_LIST_DIR}/Shaders/Leaf.${stage}.hlsl")
        set(output "${OUTPUT_DIR}/Editor/EditorShaders/Leaf.${stage}.spirv")
        add_custom_command(OUTPUT "${output}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${OUTPUT_DIR}/Editor/EditorShaders"
            COMMAND "${DXC}" -spirv -T "${profile}" -E main "${source}" -Fo "${output}"
            DEPENDS "${source}" "${CMAKE_SOURCE_DIR}/Shaders/Aether/ParticleData.hlsl"
            VERBATIM)
        list(APPEND EDITOR_SHADER_OUTPUTS "${output}")
    endforeach()
    add_custom_target(EditorShaders DEPENDS ${EDITOR_SHADER_OUTPUTS})
    add_dependencies(Editor EditorShaders)
endif()

if (BUILD_TESTING)
    target_sources(Editor.UnitTests PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/Tests/LeafSettingsTest.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Source/VFX/Rendering/LeafSettings.cpp")
endif()
