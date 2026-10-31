#include "epch.h"
#include "CompilationCache.h"

#include <Engine/Graphics/Shader/ShaderLoader.h>

namespace Elixir::Materials::Compilation
{
    CompilationCache::CompilationCache(const ShaderLoader* shaderLoader)
      : m_ShaderLoader(shaderLoader) {}

    Ref<const SCompiledMaterial> CompilationCache::GetOrCompile(
        const Ref<Material>& material,
        const std::unordered_map<std::string, bool>& staticValues
    )
    {
        if (!material) return nullptr;

        // ShaderLoader and cache mutation are serialized while compiling a miss.
        const std::scoped_lock lock(m_Mutex);

        auto& entry = m_Entries[material.get()];

        if (entry.Revision != material->GetRevision())
            entry.Variants.clear();

        std::vector<std::pair<std::string, bool>> choices(
            staticValues.begin(),
            staticValues.end()
        );

        std::ranges::sort(choices);

        std::string key;
        for (const auto& [name, value] : choices)
            key += std::to_string(name.size()) + ":" + name + (value ? "1" : "0");

        if (const auto found = entry.Variants.find(key); found != entry.Variants.end())
            return found->second;

        const auto result = m_ShaderLoader
            ? Compiler::Compile(m_ShaderLoader, *material, staticValues)
            : Compiler::Build(*material, staticValues);

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
        entry.Variants.emplace(std::move(key), result.Material);

        return result.Material;
    }
}
