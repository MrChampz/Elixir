# Shader Compilation
# Automatically compiles HLSL and GLSL shaders to SPIR-V when source files change.
# Source:  Shaders/**/*.{hlsl,glsl}
# Staging: ${CMAKE_BINARY_DIR}/Shaders/**/*.spirv  (mirrors source tree, stripping "Source/" prefix)
# Output:  Copied into each dependent target's output directory as Shaders/

set(SHADER_SOURCE_DIR "${CMAKE_SOURCE_DIR}/Shaders")
set(SHADER_STAGING_DIR "${CMAKE_BINARY_DIR}/Shaders")

# --- Locate compilers ---

find_program(DXC dxc HINTS "$ENV{VULKAN_SDK}/bin")
find_program(GLSLC glslc HINTS "$ENV{VULKAN_SDK}/bin")

# --- Collect shader sources ---

file(GLOB_RECURSE HLSL_FILES "${SHADER_SOURCE_DIR}/*.hlsl")
file(GLOB_RECURSE GLSL_FILES "${SHADER_SOURCE_DIR}/*.glsl")

# --- Separate compilable shaders from include/utility files ---
# Compilable shaders have a stage suffix: Name.{vs,ps,cs,gs,hs,ds}.{hlsl,glsl}
# Everything else (e.g. Quad.hlsl, SDF.hlsl) is treated as an include dependency.

set(SHADER_INCLUDES "")
set(COMPILABLE_HLSL "")
set(COMPILABLE_GLSL "")

foreach(SHADER_FILE IN LISTS HLSL_FILES)
    if(SHADER_FILE MATCHES "\\.(vs|ps|cs|gs|hs|ds)\\.hlsl$")
        list(APPEND COMPILABLE_HLSL "${SHADER_FILE}")
    else()
        list(APPEND SHADER_INCLUDES "${SHADER_FILE}")
    endif()
endforeach()

foreach(SHADER_FILE IN LISTS GLSL_FILES)
    if(SHADER_FILE MATCHES "\\.(vs|ps|cs|gs|hs|ds)\\.glsl$")
        list(APPEND COMPILABLE_GLSL "${SHADER_FILE}")
    else()
        list(APPEND SHADER_INCLUDES "${SHADER_FILE}")
    endif()
endforeach()

# --- Stage -> DXC profile mapping ---

function(get_dxc_profile STAGE OUT_PROFILE)
    if(STAGE STREQUAL "vs")
        set(${OUT_PROFILE} "vs_6_0" PARENT_SCOPE)
    elseif(STAGE STREQUAL "ps")
        set(${OUT_PROFILE} "ps_6_0" PARENT_SCOPE)
    elseif(STAGE STREQUAL "cs")
        set(${OUT_PROFILE} "cs_6_0" PARENT_SCOPE)
    elseif(STAGE STREQUAL "gs")
        set(${OUT_PROFILE} "gs_6_0" PARENT_SCOPE)
    elseif(STAGE STREQUAL "hs")
        set(${OUT_PROFILE} "hs_6_0" PARENT_SCOPE)
    elseif(STAGE STREQUAL "ds")
        set(${OUT_PROFILE} "ds_6_0" PARENT_SCOPE)
    else()
        message(FATAL_ERROR "Unknown HLSL shader stage: ${STAGE}")
    endif()
endfunction()

# --- Stage -> glslc stage mapping ---

function(get_glslc_stage STAGE OUT_GLSLC_STAGE)
    if(STAGE STREQUAL "vs")
        set(${OUT_GLSLC_STAGE} "vertex" PARENT_SCOPE)
    elseif(STAGE STREQUAL "ps")
        set(${OUT_GLSLC_STAGE} "fragment" PARENT_SCOPE)
    elseif(STAGE STREQUAL "cs")
        set(${OUT_GLSLC_STAGE} "compute" PARENT_SCOPE)
    elseif(STAGE STREQUAL "gs")
        set(${OUT_GLSLC_STAGE} "geometry" PARENT_SCOPE)
    elseif(STAGE STREQUAL "hs")
        set(${OUT_GLSLC_STAGE} "tesscontrol" PARENT_SCOPE)
    elseif(STAGE STREQUAL "ds")
        set(${OUT_GLSLC_STAGE} "tesseval" PARENT_SCOPE)
    else()
        message(FATAL_ERROR "Unknown GLSL shader stage: ${STAGE}")
    endif()
endfunction()

# --- Compute output path: Shaders/Source/X.vs.hlsl -> ${SHADER_STAGING_DIR}/X.vs.spirv ---

function(get_spirv_output_path SHADER_FILE OUT_PATH)
    file(RELATIVE_PATH REL_PATH "${SHADER_SOURCE_DIR}" "${SHADER_FILE}")
    string(REGEX REPLACE "\\.(hlsl|glsl)$" ".spirv" REL_SPIRV "${REL_PATH}")
    set(${OUT_PATH} "${SHADER_STAGING_DIR}/${REL_SPIRV}" PARENT_SCOPE)
endfunction()

# --- Extract stage suffix from filename ---

function(get_shader_stage SHADER_FILE OUT_STAGE)
    get_filename_component(FNAME "${SHADER_FILE}" NAME)
    string(REGEX MATCH "\\.(vs|ps|cs|gs|hs|ds)\\.(hlsl|glsl)$" _MATCH "${FNAME}")
    set(${OUT_STAGE} "${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction()

# --- Add compilation commands ---

set(ALL_SPIRV_OUTPUTS "")

# HLSL shaders (compiled with dxc)
if(COMPILABLE_HLSL)
    if(NOT DXC)
        message(FATAL_ERROR "dxc not found. Install the Vulkan SDK or set VULKAN_SDK environment variable.")
    endif()

    foreach(SHADER_FILE IN LISTS COMPILABLE_HLSL)
        get_shader_stage("${SHADER_FILE}" STAGE)
        get_dxc_profile("${STAGE}" PROFILE)
        get_spirv_output_path("${SHADER_FILE}" SPIRV_OUTPUT)

        get_filename_component(SHADER_DIR "${SHADER_FILE}" DIRECTORY)

        get_filename_component(SPIRV_OUTPUT_DIR "${SPIRV_OUTPUT}" DIRECTORY)
        file(MAKE_DIRECTORY "${SPIRV_OUTPUT_DIR}")

        add_custom_command(
            OUTPUT "${SPIRV_OUTPUT}"
            COMMAND "${DXC}" -spirv -T ${PROFILE} -E main
                    -I "${SHADER_DIR}"
                    "${SHADER_FILE}"
                    -Fo "${SPIRV_OUTPUT}"
            DEPENDS "${SHADER_FILE}" ${SHADER_INCLUDES}
            COMMENT "Compiling HLSL: ${SHADER_FILE} -> ${SPIRV_OUTPUT}"
            VERBATIM
        )

        list(APPEND ALL_SPIRV_OUTPUTS "${SPIRV_OUTPUT}")
    endforeach()
endif()

# GLSL shaders (compiled with glslc)
if(COMPILABLE_GLSL)
    if(NOT GLSLC)
        message(FATAL_ERROR "glslc not found. Install the Vulkan SDK or set VULKAN_SDK environment variable.")
    endif()

    foreach(SHADER_FILE IN LISTS COMPILABLE_GLSL)
        get_shader_stage("${SHADER_FILE}" STAGE)
        get_glslc_stage("${STAGE}" GLSLC_STAGE)
        get_spirv_output_path("${SHADER_FILE}" SPIRV_OUTPUT)

        get_filename_component(SHADER_DIR "${SHADER_FILE}" DIRECTORY)

        get_filename_component(SPIRV_OUTPUT_DIR "${SPIRV_OUTPUT}" DIRECTORY)
        file(MAKE_DIRECTORY "${SPIRV_OUTPUT_DIR}")

        add_custom_command(
            OUTPUT "${SPIRV_OUTPUT}"
            COMMAND "${GLSLC}" -fshader-stage=${GLSLC_STAGE}
                    -I "${SHADER_DIR}"
                    "${SHADER_FILE}"
                    -o "${SPIRV_OUTPUT}"
            DEPENDS "${SHADER_FILE}" ${SHADER_INCLUDES}
            COMMENT "Compiling GLSL: ${SHADER_FILE} -> ${SPIRV_OUTPUT}"
            VERBATIM
        )

        list(APPEND ALL_SPIRV_OUTPUTS "${SPIRV_OUTPUT}")
    endforeach()
endif()

# --- CompileShaders target (compiles all shaders to the staging dir) ---

add_custom_target(CompileShaders ALL DEPENDS ${ALL_SPIRV_OUTPUTS})

# --- Copy compiled shaders into each engine-dependent target's output directory ---
# Call copy_shaders_for_targets() after all targets have been defined (from root CMakeLists.txt).
# Each target gets: ${OUTPUT_DIR}/${target}/Shaders/

function(copy_shaders_for_targets)
    get_property(dependents GLOBAL PROPERTY "ELIXIR_DEPENDENTS")
    foreach(target IN LISTS dependents)
        set(TARGET_SHADER_DIR "${OUTPUT_DIR}/${target}/Shaders")

        add_custom_command(
            OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${target}_copy_shaders.stamp"
            COMMAND "${CMAKE_COMMAND}" -E copy_directory
                    "${SHADER_STAGING_DIR}"
                    "${TARGET_SHADER_DIR}"
            COMMAND "${CMAKE_COMMAND}" -E touch
                    "${CMAKE_CURRENT_BINARY_DIR}/${target}_copy_shaders.stamp"
            DEPENDS ${ALL_SPIRV_OUTPUTS}
            COMMENT "Copying compiled shaders to ${TARGET_SHADER_DIR}..."
            VERBATIM
        )

        add_custom_target("${target}_copy_shaders" DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/${target}_copy_shaders.stamp"
        )

        add_dependencies("${target}_copy_shaders" CompileShaders)
        add_dependencies(${target} "${target}_copy_shaders")

        message(STATUS "Adding shader copy for target: ${target} -> ${TARGET_SHADER_DIR}")
    endforeach()
endfunction()
