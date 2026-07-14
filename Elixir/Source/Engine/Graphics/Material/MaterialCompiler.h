#pragma once

#include <Engine/Graphics/Material/MaterialGraph.h>
#include <Engine/Graphics/Shader/ShaderLoader.h>

#include <filesystem>
#include <optional>
#include <string>

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
        };

        // Compile the graph into a ready-to-bind shader. Returns nullptr on failure.
        static Ref<Shader> Compile(const ShaderLoader* loader, const MaterialGraph& graph);

        // Stage 1 (no Vulkan): generate HLSL and compile it to SPIR-V on disk. Safe to
        // run on a worker thread. Returns nullopt on failure.
        static std::optional<SCompiled> CompileToSpirv(const MaterialGraph& graph);

        // Stage 2 (Vulkan): load the compiled SPIR-V into a Shader. Must run on a
        // thread that may create graphics resources.
        static Ref<Shader> LoadCompiled(const ShaderLoader* loader, const SCompiled& compiled);

        // Splice the graph body into a template string (pure; testable).
        static std::string InjectBody(const std::string& templateHlsl, const MaterialGraph& graph);
    };
}
