#pragma once

#include <Engine/Material/MaterialCompiler.h>
#include <Engine/Material/MaterialInstance.h>

namespace Elixir
{
    class ELIXIR_API MaterialRenderProxy final
    {
    public:
        static Ref<const MaterialRenderProxy> Create(
            Ref<const SCompiledMaterial> material,
            const MaterialInstance& instance
        );

        const Ref<const SCompiledMaterial> GetCompiledMaterial() const { return m_CompiledMaterial; }
        uint32_t GetInstanceRevision() const { return m_InstanceRevision; }
        const std::vector<glm::vec4>& GetValues() const { return m_Values; }
        const std::vector<Ref<Texture>>& GetTextures() const { return m_Textures; }

    private:
        Ref<const SCompiledMaterial> m_CompiledMaterial;
        uint32_t m_InstanceRevision = 0;
        std::vector<glm::vec4> m_Values;
        std::vector<Ref<Texture>> m_Textures;
    };
}