include(../../Utils.cmake)

# Files
file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS *.h *.cpp)
add_executable(${PROJECT_NAME}.UnitTests ${TEST_SOURCES})

# Properties and Definitions
apply_properties_and_definitions(${PROJECT_NAME}.UnitTests)

# Include dirs
target_include_directories(${PROJECT_NAME}.UnitTests PRIVATE ../Source)

# Linking
link_target_to_engine(${PROJECT_NAME}.UnitTests)
target_link_libraries(${PROJECT_NAME}.UnitTests PRIVATE
    GTest::gmock
    GTest::gtest_main
)

# Test shaders belong to this project and are compiled only for its test executable.
set(TEST_SHADER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Shaders")
set(TEST_SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/Shaders")
file(GLOB_RECURSE TEST_HLSL_FILES CONFIGURE_DEPENDS "${TEST_SHADER_SOURCE_DIR}/*.hlsl")
set(TEST_SPIRV_OUTPUTS "")
if(DXC)
    foreach(SHADER_FILE IN LISTS TEST_HLSL_FILES)
        get_shader_stage("${SHADER_FILE}" STAGE)
        get_dxc_profile("${STAGE}" PROFILE)
        file(RELATIVE_PATH REL_PATH "${TEST_SHADER_SOURCE_DIR}" "${SHADER_FILE}")
        string(REGEX REPLACE "\\.hlsl$" ".spirv" REL_SPIRV "${REL_PATH}")
        set(SPIRV_OUTPUT "${TEST_SHADER_OUTPUT_DIR}/${REL_SPIRV}")
        get_filename_component(SPIRV_OUTPUT_DIR "${SPIRV_OUTPUT}" DIRECTORY)
        add_custom_command(
            OUTPUT "${SPIRV_OUTPUT}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${SPIRV_OUTPUT_DIR}"
            COMMAND "${DXC}" -spirv -T ${PROFILE} -E main
                    -I "${TEST_SHADER_SOURCE_DIR}" "${SHADER_FILE}" -Fo "${SPIRV_OUTPUT}"
            DEPENDS "${SHADER_FILE}"
            COMMENT "Compiling test shader: ${REL_PATH}"
            VERBATIM
        )
        list(APPEND TEST_SPIRV_OUTPUTS "${SPIRV_OUTPUT}")
    endforeach()
endif()
add_custom_target(${PROJECT_NAME}.TestShaders DEPENDS ${TEST_SPIRV_OUTPUTS})
add_dependencies(${PROJECT_NAME}.UnitTests ${PROJECT_NAME}.TestShaders)
target_compile_definitions(${PROJECT_NAME}.UnitTests PRIVATE
    ELIXIR_TEST_SHADER_DIR="${TEST_SHADER_OUTPUT_DIR}"
)

include (GoogleTest)

# GPU integration tests need a Vulkan device with the engine's full feature set
# (bindless, bufferDeviceAddress, ...), which CI's software rasterizer
# (SwiftShader) does not provide. Tag them with the "gpu" label via a separate
# discovery pass so CI can skip them with `ctest -LE gpu` while still running
# the pure unit tests (converters, initializers, traits, ...).
set(GPU_TEST_FILTER "VulkanBufferTest.*:VulkanImageTest.*:VulkanSkinningTest.*:VulkanDescriptorPoolTest.*:VulkanShaderTest.*")

gtest_discover_tests(${PROJECT_NAME}.UnitTests
    TEST_FILTER "${GPU_TEST_FILTER}"
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    PROPERTIES LABELS "gpu"
)
gtest_discover_tests(${PROJECT_NAME}.UnitTests
    TEST_FILTER "-${GPU_TEST_FILTER}"
)
