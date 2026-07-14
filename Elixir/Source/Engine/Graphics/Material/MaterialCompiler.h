#pragma once

#include <Engine/Graphics/Material/MaterialGraph.h>
#include <Engine/Graphics/Shader/ShaderLoader.h>

#include <filesystem>
#include <string>

namespace Elixir
{
    // Turns a MaterialGraph into a usable shader: injects the graph's generated
    // body into the GraphMaterial template, compiles it to SPIR-V with DXC at
    // runtime, and loads it (with the shared model vertex shader) into a Shader.
    class ELIXIR_API MaterialCompiler
    {
      public:
        // Compile the graph into a ready-to-bind shader. Returns nullptr on failure.
        static Ref<Shader> Compile(const ShaderLoader* loader, const MaterialGraph& graph);

        // Splice the graph body into a template string (pure; testable).
        static std::string InjectBody(const std::string& templateHlsl, const MaterialGraph& graph);
    };
}
