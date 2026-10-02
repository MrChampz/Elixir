#include "epch.h"
#include "CompilationCache.h"

#include <Engine/Graphics/Shader/ShaderLoader.h>

namespace Elixir::Materials::Compilation
{
    CompilationCache::CompilationCache(const ShaderLoader* shaderLoader)
      : m_ShaderLoader(shaderLoader) {}

    Ref<const SCompiledMaterial> CompilationCache::GetOrCompile(
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
            ? Compiler::Compile(m_ShaderLoader, *material)
            : Compiler::Build(*material);

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
