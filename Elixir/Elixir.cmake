project("Elixir")

# Files
add_library(${PROJECT_NAME} SHARED
    ${CMAKE_CURRENT_LIST_DIR}/Source/Engine.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/Engine/Core/Core.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/Engine/Core/Color.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/Engine/Core/UUID.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/Engine/Core/UUID.cpp
    ${CMAKE_CURRENT_LIST_DIR}/Source/Engine/Core/Log.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/Engine/Core/Log.cpp
    ${CMAKE_CURRENT_LIST_DIR}/Source/Engine/Core/Timer.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/Engine/Core/FrameProfiler.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/Engine/Core/Window.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/Engine/Core/Executor.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/Engine/Core/Executor.cpp
    ${CMAKE_CURRENT_LIST_DIR}/Source/Engine/Input/Input.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/Engine/Input/InputManager.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/Engine/Input/InputManager.cpp
    ${CMAKE_CURRENT_LIST_DIR}/Source/Engine/Input/InputCodes.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/Engine/Core/Application.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/Engine/Core/Application.cpp
    ${CMAKE_CURRENT_LIST_DIR}/Source/Engine/Core/Entrypoint.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/Engine/Event/Event.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/Engine/Event/KeyEvent.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/Engine/Event/MouseEvent.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/Engine/Event/WindowEvent.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/Engine/Event/ApplicationEvent.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/Engine/Event/EventFormatter.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/Engine/Instrumentation/Profiler.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/Engine/Graphics/GraphicsContext.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/Engine/Graphics/GraphicsContext.cpp
    ${CMAKE_CURRENT_LIST_DIR}/Source/Engine/Graphics/CommandBuffer.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/Engine/Graphics/CommandBuffer.cpp
    ${CMAKE_CURRENT_LIST_DIR}/Source/Graphics/Vulkan/VulkanGraphicsContext.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/Graphics/Vulkan/VulkanGraphicsContext.cpp
    ${CMAKE_CURRENT_LIST_DIR}/Source/Graphics/Vulkan/VulkanCommandBuffer.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/Graphics/Vulkan/VulkanCommandBuffer.cpp
    ${CMAKE_CURRENT_LIST_DIR}/Source/Graphics/Vulkan/Initializers.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/Graphics/Vulkan/Converters.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/Graphics/Vulkan/Utils.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/Platform/GLFW/GLFWWindow.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/Platform/GLFW/GLFWWindow.cpp
    ${CMAKE_CURRENT_LIST_DIR}/Source/Platform/GLFW/GLFWInput.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/Platform/GLFW/GLFWInput.cpp
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

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE True
    INTERPROCEDURAL_OPTIMIZATION False
    LINKER_LANGUAGE CXX
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${OUTPUT_DIR}/${PROJECT_NAME}"
    LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIR}/${PROJECT_NAME}"
    RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}/${PROJECT_NAME}"
)

# Compile definitions

target_compile_definitions(${PROJECT_NAME} PRIVATE
    GLFW_INCLUDE_NONE
    $<$<CONFIG:Debug>:EE_DEBUG>
    $<$<CONFIG:Release>:EE_RELEASE>
    $<$<CONFIG:Dist>:EE_DIST>
)

# Profiling compile definitions
if (ELIXIR_PROFILE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        TRACY_ENABLE
        TRACY_FIBERS
        EE_PROFILE
    )
endif()

# Platform-specific compile definitions
if (WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        _CRT_SECURE_NO_WARNINGS
        EE_PLATFORM_WINDOWS
        EE_BUILD_DLL
        GLFW_EXPOSE_NATIVE_WIN32
        VK_USE_PLATFORM_WIN32_KHR
    )

    if (MSVC)
        target_compile_options(${PROJECT_NAME} PRIVATE "/Zc:preprocessor")
    endif()
elseif (APPLE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        EE_PLATFORM_MACOS
        GLFW_EXPOSE_NATIVE_COCOA
    )
endif()

# Dependencies
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