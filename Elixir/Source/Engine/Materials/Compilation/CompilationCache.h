#pragma once

#include <Engine/Materials/Material.h>
#include <Engine/Materials/Compilation/Compiler.h>

namespace Elixir { class ShaderLoader; }

namespace Elixir::Materials::Compilation
{
    /**
     * @brief Caches compiled material data for one shader loader.
     *
     * Cached entries are rebuilt when the source material revision changes. Access
     * is serialized so one compilation updates the cache at a time.
     */
    class ELIXIR_API CompilationCache final
    {
    public:
        /**
         * @brief Creates a material compilation cache.
         *
         * When shaderLoader is null, the cache validates graphs and builds layouts
         * without producing shader programs.
         *
         * @param shaderLoader Shader loader used for full shader compilation.
         */
        explicit CompilationCache(const ShaderLoader* shaderLoader);

        /**
         * @brief Gets compiled data for a material.
         *
         * The cache returns the existing result when its revision matches the source
         * material. Otherwise, it rebuilds the material before returning it.
         *
         * @param material Material to compile or retrieve.
         * @return Compiled material data, or null when material is null or compilation fails.
         */
        Ref<const SCompiledMaterial> GetOrCompile(const Ref<Material>& material);

    private:
        /** @brief Stores the source material, compiled revision, and cached result. */
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
