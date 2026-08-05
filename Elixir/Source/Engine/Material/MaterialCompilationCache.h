#pragma once

#include <Engine/Material/Material.h>
#include <Engine/Material/MaterialCompiler.h>

namespace Elixir
{
    class ShaderLoader;

    // Renderer-owned, GraphicsContext-scoped cache for compiled material programs.
    class ELIXIR_API MaterialCompilationCache final
    {
    public:
        explicit MaterialCompilationCache(const ShaderLoader* shaderLoader);

        Ref<const SCompiledMaterial> GetOrCompile(const Ref<Material>& material);

    private:
        struct SEntry
        {
            Ref<Material> Source;
            uint32_t Revision = 0;
            Ref<const SCompiledMaterial> Compiled;
        };

        const ShaderLoader* m_ShaderLoader = nullptr;
        std::unordered_map<const Material*, SEntry> m_Entries;
        std::mutex m_Mutex;
    };
}