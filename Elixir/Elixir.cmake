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
    GLFW_INCLUDE_NONE
    TRACY_ENABLE
    TRACY_FIBERS
    $<$<CONFIG:Debug>:EE_DEBUG>
    $<$<CONFIG:Release>:EE_RELEASE>
    $<$<CONFIG:Dist>:EE_DIST>
)

if (WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        _CRT_SECURE_NO_WARNINGS
        EE_PLATFORM_WINDOWS
        EE_BUILD_DLL
        GLFW_EXPOSE_NATIVE_WIN32
        VK_USE_PLATFORM_WIN32_KHR
    )
elseif (APPLE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        EE_PLATFORM_MACOS
        GLFW_EXPOSE_NATIVE_COCOA
    )
endif()

target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<COMPILE_LANG_AND_ID:C,MSVC>:/MP>
    $<$<CONFIG:Debug>:$<$<COMPILE_LANG_AND_ID:C,MSVC>:/MDd>>
    $<$<CONFIG:Debug>:$<$<COMPILE_LANG_AND_ID:C,MSVC>:/Z7>>
    $<$<CONFIG:Release>:$<$<COMPILE_LANG_AND_ID:C,MSVC>:/Ot>>
    $<$<CONFIG:Release>:$<$<COMPILE_LANG_AND_ID:C,MSVC>:/MD>>
    $<$<CONFIG:Dist>:$<$<COMPILE_LANG_AND_ID:C,MSVC>:/Ot>>
    $<$<CONFIG:Dist>:$<$<COMPILE_LANG_AND_ID:C,MSVC>:/MD>>
    $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/MP>
    $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/std:c++20>
    $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/EHsc>
    $<$<CONFIG:Debug>:$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/MDd>>
    $<$<CONFIG:Debug>:$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/Z7>>
    $<$<CONFIG:Release>:$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/Ot>>
    $<$<CONFIG:Release>:$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/MD>>
    $<$<CONFIG:Dist>:$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/Ot>>
    $<$<CONFIG:Dist>:$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/MD>>
)

# Dependencies
option(UUID_USING_CXX20_SPAN "" ON)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/Vendor/stduuid)

option(FTL_BUILD_TESTS "" OFF)
option(FTL_BUILD_BENCHMARKS "" OFF)
option(FTL_BUILD_EXAMPLES "" OFF)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/Vendor/FiberTaskingLib)

option(GLM_ENABLE_CXX_20 "" ON)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/Vendor/glm)

option(SPDLOG_USE_STD_FORMAT "" ON)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/Vendor/spdlog)

option(FASTGLTF_COMPILE_AS_CPP20 "" ON)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/Vendor/fastgltf)

option(GLFW_BUILD_DOCS "" OFF)
option(GLFW_INSTALL "" OFF)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/Vendor/glfw)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/Vendor/imgui)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/Vendor/glad)

find_package(Vulkan REQUIRED)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/Vendor/vk-bootstrap)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/Vendor/VulkanMemoryAllocator)

option(SPIRV_CROSS_ENABLE_TESTS "" OFF)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/Vendor/SPIRV-Cross)

# Debug-only dependencies
if (BUILD_TYPE STREQUAL "Debug")
    option(TRACY_ENABLE "" ON)
    option(TRACY_ON_DEMAND "" ON)
    option(TRACY_FIBERS "" ON)
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
    glad
    Vulkan::Vulkan
    vk-bootstrap
    VulkanMemoryAllocator
    spirv-cross-cpp
)

# Debug-only linking
if (BUILD_TYPE STREQUAL "Debug")
    target_link_libraries(${PROJECT_NAME} Tracy::TracyClient)
endif()