cmake_minimum_required(VERSION 3.30)
project(Elixir)
include(FetchContent)
include(CTest)

# Options
option(ELIXIR_PROFILE "Enable/disable profiling" OFF)

# Define the valid build types
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;Dist" CACHE STRING "" FORCE)

# Detect the system arch
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" ARCH)

# Detect the system name
set(SYSTEM "${CMAKE_SYSTEM_NAME}")
if (SYSTEM MATCHES "^[Dd]arwin")
    set(SYSTEM "macOS")
endif()

# Detect the build type
set(BUILD_TYPE "${CMAKE_BUILD_TYPE}")

# Configurations
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

# Enable modern MSVC linker features
if (MSVC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /Zc:preprocessor")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zc:preprocessor")
endif()

# Configurations - Debug
if(BUILD_TYPE STREQUAL "Debug")
    set(ELIXIR_PROFILE ON)
    set_directory_properties(PROPERTIES COMPILE_DEFINITIONS "_DEBUG")
endif()

# Enable Hot Reload for MSVC compilers, if supported.
if (POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

# Configurations - Dist
set(CMAKE_C_FLAGS_DIST "${CMAKE_C_FLAGS_RELEASE}" CACHE STRING "Flags for Dist C" FORCE)
set(CMAKE_CXX_FLAGS_DIST "${CMAKE_CXX_FLAGS_RELEASE}" CACHE STRING "Flags for Dist C++" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_DIST "${CMAKE_EXE_LINKER_FLAGS_RELEASE}" CACHE STRING "Linker flags for Dist" FORCE)
set(CMAKE_SHARED_LINKER_FLAGS_DIST "${CMAKE_SHARED_LINKER_FLAGS_RELEASE}" CACHE STRING "Shared linker flags for Dist" FORCE)

# Output dir based on build config and arch
set(OUTPUT_DIR "${CMAKE_BINARY_DIR}/Build/${BUILD_TYPE}/${SYSTEM}/${ARCH}")

# Fetch testing library
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0
)

if (BUILD_TESTING)
    # Enable testing flag
    set(ELIXIR_TESTING ON)

    # Disable profiling for testing build
    set(ELIXIR_PROFILE OFF)

    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

include(Elixir/Elixir.cmake)
include(Dissolve/Dissolve.cmake)

# Copy Engine .DLL to all targets that depends on it, if needed
if (WIN32)
    copy_runtime_dlls_for_targets()
endif()