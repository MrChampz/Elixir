cmake_minimum_required(VERSION 3.30)
project(Elixir)

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

include(Elixir/Elixir.cmake)
include(Dissolve/Dissolve.cmake)
