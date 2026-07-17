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

include (GoogleTest)

# GPU integration tests need a Vulkan device with the engine's full feature set
# (bindless, bufferDeviceAddress, ...), which CI's software rasterizer
# (SwiftShader) does not provide. Tag them with the "gpu" label via a separate
# discovery pass so CI can skip them with `ctest -LE gpu` while still running
# the pure unit tests (converters, initializers, traits, ...).
set(GPU_TEST_FILTER "VulkanBufferTest.*:VulkanImageTest.*")

gtest_discover_tests(${PROJECT_NAME}.UnitTests
    TEST_FILTER "${GPU_TEST_FILTER}"
    PROPERTIES LABELS "gpu"
)
gtest_discover_tests(${PROJECT_NAME}.UnitTests
    TEST_FILTER "-${GPU_TEST_FILTER}"
)
