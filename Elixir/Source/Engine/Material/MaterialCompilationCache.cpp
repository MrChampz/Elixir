#include "epch.h"
#include "MaterialCompilationCache.h"

#include <Engine/Graphics/Shader/ShaderLoader.h>

namespace Elixir
{
    MaterialCompilationCache::MaterialCompilationCache(const ShaderLoader* shaderLoader)
      : m_ShaderLoader(shaderLoader) {}

    Ref<const SCompiledMaterial> MaterialCompilationCache::GetOrCompile(
        const Ref<Material>& material
    )
    {
        if (!material) return nullptr;

        // ShaderLoader and cache mutation are serialized while compiling a miss.
        const std::scoped_lock lock(m_Mutex);

        auto& entry = m_Entries[material.get()];

        if (entry.Compiled && entry.Revision == material->GetRevision())
            return entry.Compiled;

        const auto result = m_ShaderLoader
            ? MaterialCompiler::Compile(m_ShaderLoader, *material)
            : MaterialCompiler::Build(*material);

        if (!result)
        {
            EE_CORE_ERROR(
                "Material '{}' compilation failed: {}",
                material->GetName(),
                result.Diagnostics
            )
            return nullptr;
        }

        entry.Source = material;
        entry.Revision = material->GetRevision();
        entry.Compiled = result.Material;

        return entry.Compiled;
    }
}
