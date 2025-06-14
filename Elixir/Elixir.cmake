project("Elixir")

# Files
file(GLOB_RECURSE SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/Source/*.h"
    "${CMAKE_CURRENT_LIST_DIR}/Source/*.cpp"
)
add_library(${PROJECT_NAME} SHARED
    ${SOURCES}
    ${CMAKE_CURRENT_LIST_DIR}/Vendor/stb/stb_image.h
    ${CMAKE_CURRENT_LIST_DIR}/Vendor/stb/stb_image.cpp
)

# PCH
target_precompile_headers(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/Source/epch.h
)

# Set output name
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${PROJECT_NAME}")

# Properties

function(apply_properties_and_definitions target)
    set_target_properties(${target} PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
        POSITION_INDEPENDENT_CODE True
        INTERPROCEDURAL_OPTIMIZATION False
        LINKER_LANGUAGE CXX
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
    )

    set_target_properties(${target} PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${OUTPUT_DIR}/${target}"
        LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIR}/${target}"
        RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}/${target}"
    )

    # Compile definitions

    target_compile_definitions(${target} PRIVATE
        GLFW_INCLUDE_NONE
        $<$<CONFIG:Debug>:EE_DEBUG>
        $<$<CONFIG:Release>:EE_RELEASE>
        $<$<CONFIG:Dist>:EE_DIST>
    )

    # Profiling compile definitions
    if (ELIXIR_PROFILE)
        target_compile_definitions(${target} PRIVATE
            TRACY_ENABLE
            TRACY_FIBERS
            EE_PROFILE
        )
    endif()

    # Platform-specific compile definitions
    if (WIN32)
        target_compile_definitions(${target} PRIVATE
            _CRT_SECURE_NO_WARNINGS
            EE_PLATFORM_WINDOWS
            GLFW_EXPOSE_NATIVE_WIN32
            VK_USE_PLATFORM_WIN32_KHR
        )
    elseif (APPLE)
        target_compile_definitions(${target} PRIVATE
            EE_PLATFORM_MACOS
            GLFW_EXPOSE_NATIVE_COCOA
        )
    endif()
endfunction()

apply_properties_and_definitions(${PROJECT_NAME})

if (WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        EE_BUILD_DLL
    )
endif()

# Dependencies
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/Vendor/magic_enum)

set(UUID_USING_CXX20_SPAN ON)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/Vendor/stduuid)

set(FTL_BUILD_TESTS OFF)
set(FTL_BUILD_BENCHMARKS OFF)
set(FTL_BUILD_EXAMPLES OFF)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/Vendor/FiberTaskingLib)

set(GLM_ENABLE_CXX_20 ON)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/Vendor/glm)

set(SPDLOG_USE_STD_FORMAT ON)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/Vendor/spdlog)

set(FASTGLTF_COMPILE_AS_CPP20 ON)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/Vendor/simdjson)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/Vendor/fastgltf)

set(GLFW_BUILD_DOCS OFF)
set(GLFW_INSTALL OFF)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/Vendor/glfw)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/Vendor/imgui)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/Vendor/glad)

find_package(Vulkan REQUIRED)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/Vendor/vk-bootstrap)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/Vendor/VulkanMemoryAllocator)

set(SPIRV_CROSS_ENABLE_TESTS OFF)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/Vendor/SPIRV-Cross)

# Only when profiling is enabled
if (ELIXIR_PROFILE)
    set(TRACY_ENABLE ON)
    set(TRACY_FIBERS ON)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/Vendor/tracy)
endif()

# Include dirs
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/Source
    ${CMAKE_CURRENT_LIST_DIR}/Vendor/stb
)

# Linking
target_link_libraries(${PROJECT_NAME}
    magic_enum
    stduuid
    ftl
    glm
    spdlog
    fastgltf
    imgui
    glfw
    #glad
    Vulkan::Vulkan
    vk-bootstrap
    VulkanMemoryAllocator
    spirv-cross-cpp
)

# Only when profiling is enabled
if (ELIXIR_PROFILE)
    target_link_libraries(${PROJECT_NAME} Tracy::TracyClient)
endif()

# Testing
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/Tests)

# Copy Engine .DLL to all targets that depends on it, if needed
foreach(target IN LISTS ELIXIR_DEPENDENTS)
    add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${target}_copy_runtime_dlls"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        $<TARGET_RUNTIME_DLLS:${target}>
        "$<TARGET_FILE_DIR:${target}>"
        COMMAND_EXPAND_LISTS
        COMMENT "Copying runtime DLLs for ${target} to output directory..."
        VERBATIM
    )

    add_custom_target("${target}_copy_dlls" DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${target}_copy_runtime_dlls")
    add_dependencies(${target} "${target}_copy_dlls")
endforeach()