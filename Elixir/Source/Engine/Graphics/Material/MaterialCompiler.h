#pragma once

#include <Engine/Graphics/Material/MaterialGraph.h>
#include <Engine/Graphics/Material/MaterialShaderPermutation.h>
#include <Engine/Graphics/Shader/ShaderLoader.h>

#include <filesystem>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace Elixir
{
    // Turns a MaterialGraph into a usable shader: injects the graph's generated
    // body into the GraphMaterial template, compiles it to SPIR-V with DXC at
    // runtime, and loads it (with the shared model vertex shader) into a Shader.
    class ELIXIR_API MaterialCompiler
    {
      public:
        // The on-disk SPIR-V produced by CompileToSpirv, ready for LoadCompiled.
        struct SCompiled
        {
            std::filesystem::path LoadDir;
            std::string Name;
            SMaterialShaderPermutation Permutation;
        };

        // Compile the graph into a ready-to-bind shader. Returns nullptr on failure.
        static Ref<Shader> Compile(const ShaderLoader* loader, const MaterialGraph& graph,
            const SMaterialShaderPermutation& permutation);

        // Stage 1 (no Vulkan): generate HLSL and compile it to SPIR-V on disk. Safe to
        // run on a worker thread. Output paths are content-addressed, so an existing
        // complete artifact is reused without invoking DXC. Returns nullopt on failure.
        static std::optional<SCompiled> CompileToSpirv(const MaterialGraph& graph,
            const SMaterialShaderPermutation& permutation);

        // Stable identity of the final stage sources. It covers both shader templates,
        // the selected graph permutation, renderer target, profiles/platform and this
        // compiler's format version. The filesystem overload reads runtime templates.
        static std::optional<uint64_t> BuildContentKey(const MaterialGraph& graph,
            const SMaterialShaderPermutation& permutation);
        static uint64_t BuildContentKeyFromTemplates(const MaterialGraph& graph,
            const SMaterialShaderPermutation& permutation,
            std::string_view pixelTemplate, std::string_view vertexTemplate);
        static std::string CacheName(uint64_t contentKey);

        // Stage 2 (Vulkan): load the compiled SPIR-V into a Shader. Must run on a
        // thread that may create graphics resources.
        static Ref<Shader> LoadCompiled(const ShaderLoader* loader, const SCompiled& compiled);

        // Splice the graph body into a template string (pure; testable).
        static std::string InjectBody(const std::string& templateHlsl, const MaterialGraph& graph);
    };
}
