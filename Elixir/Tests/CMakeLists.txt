include(../../Utils.cmake)

# Files
file(GLOB_RECURSE TEST_SOURCES *.h *.cpp)
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
gtest_discover_tests(${PROJECT_NAME}.UnitTests)