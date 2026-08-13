include(Utils.cmake)

FetchContent_Declare(
    RmlUi
    GIT_REPOSITORY https://github.com/mikke89/RmlUi.git
    GIT_TAG 61c600b4fde3995bc26c570d8156ba468fea1944 # RmlUi 5.0
    GIT_SHALLOW TRUE
)

set(BUILD_UNIVERSAL_BINARIES OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(ENABLE_SVG_PLUGIN ON CACHE BOOL "" FORCE)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)
set(EDITOR_BUILD_TESTING "${BUILD_TESTING}")
set(BUILD_TESTING OFF)
FetchContent_MakeAvailable(RmlUi)
find_package(plutovg CONFIG REQUIRED)
target_link_libraries(RmlCore PRIVATE plutovg::plutovg)
set(BUILD_TESTING "${EDITOR_BUILD_TESTING}")

project("Editor")

add_executable(${PROJECT_NAME}
    ${CMAKE_CURRENT_LIST_DIR}/Source/Editor.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/Editor.cpp
    ${CMAKE_CURRENT_LIST_DIR}/Source/ContentBrowser.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/ContentBrowser.cpp
    ${CMAKE_CURRENT_LIST_DIR}/Source/FileSystemWatcher.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/GameViewRenderer.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/GameViewRenderer.cpp
    ${CMAKE_CURRENT_LIST_DIR}/Source/RmlUiSystemInterface.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/RmlUiSystemInterface.cpp
    ${CMAKE_CURRENT_LIST_DIR}/Source/RmlUiRenderer.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/RmlUiRenderer.cpp
    ${CMAKE_CURRENT_LIST_DIR}/Source/RmlUiInput.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/RmlUiInput.cpp
)

if (APPLE)
    target_sources(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/Source/Platform/MacOSFileSystemWatcher.cpp
    )
    find_library(CORESERVICES_FRAMEWORK CoreServices REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${CORESERVICES_FRAMEWORK})
elseif (WIN32)
    target_sources(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/Source/Platform/WindowsFileSystemWatcher.cpp
    )
elseif (UNIX)
    target_sources(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/Source/Platform/LinuxFileSystemWatcher.cpp
    )
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "${PROJECT_NAME}"
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE False
    INTERPROCEDURAL_OPTIMIZATION False
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
    ARCHIVE_OUTPUT_DIRECTORY "${OUTPUT_DIR}/${PROJECT_NAME}"
    LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIR}/${PROJECT_NAME}"
    RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}/${PROJECT_NAME}"
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    EDITOR_PROJECT_ASSET_ROOT="${CMAKE_SOURCE_DIR}/Assets"
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
    target_compile_definitions(${PROJECT_NAME} PRIVATE EE_PLATFORM_MACOS)
    target_compile_options(${PROJECT_NAME} PRIVATE -stdlib=libc++)
    target_link_options(${PROJECT_NAME} PRIVATE -stdlib=libc++)
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/Source
    ${CMAKE_SOURCE_DIR}/Elixir/Source
)

add_dependencies(${PROJECT_NAME} Elixir RmlCore)
target_link_libraries(${PROJECT_NAME} PRIVATE RmlCore)
link_target_to_engine(${PROJECT_NAME})

set(EDITOR_ASSET_STAMP "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}_copy_assets.stamp")
file(GLOB EDITOR_ICON_ASSETS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/Assets/Icons/*.svg"
)
file(GLOB EDITOR_LAYOUT_ASSETS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/Assets/*.rml"
    "${CMAKE_CURRENT_LIST_DIR}/Assets/*.rcss"
)
add_custom_command(
    OUTPUT "${EDITOR_ASSET_STAMP}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/Assets/Fonts"
    COMMAND "${CMAKE_COMMAND}" -E make_directory
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/Assets/Editor"
    COMMAND "${CMAKE_COMMAND}" -E make_directory
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/Assets/Editor/Icons"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_SOURCE_DIR}/Assets/Button_Background.png"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/Assets/Button_Background.png"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_SOURCE_DIR}/Assets/Fonts/SF-Pro-Display-Regular.otf"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/Assets/Fonts/SF-Pro-Display-Regular.otf"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_SOURCE_DIR}/Assets/Fonts/OpenSans-Regular.ttf"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/Assets/Fonts/OpenSans-Regular.ttf"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_SOURCE_DIR}/Assets/Fonts/PlayfairDisplay-Regular.ttf"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/Assets/Fonts/PlayfairDisplay-Regular.ttf"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            ${EDITOR_LAYOUT_ASSETS}
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/Assets/Editor"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            ${EDITOR_ICON_ASSETS}
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/Assets/Editor/Icons"
    COMMAND "${CMAKE_COMMAND}" -E touch "${EDITOR_ASSET_STAMP}"
    DEPENDS
        "${CMAKE_SOURCE_DIR}/Assets/Button_Background.png"
        "${CMAKE_SOURCE_DIR}/Assets/Fonts/SF-Pro-Display-Regular.otf"
        "${CMAKE_SOURCE_DIR}/Assets/Fonts/OpenSans-Regular.ttf"
        "${CMAKE_SOURCE_DIR}/Assets/Fonts/PlayfairDisplay-Regular.ttf"
        ${EDITOR_LAYOUT_ASSETS}
        ${EDITOR_ICON_ASSETS}
    COMMENT "Copying Editor runtime assets..."
    VERBATIM
)

add_custom_target(${PROJECT_NAME}_copy_assets DEPENDS "${EDITOR_ASSET_STAMP}")
add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}_copy_assets)
