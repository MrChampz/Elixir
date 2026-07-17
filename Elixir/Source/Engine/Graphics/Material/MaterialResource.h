#pragma once

#include <Engine/Graphics/Material/Material.h>
#include <Engine/Graphics/Material/MaterialShaderPermutation.h>

namespace Elixir
{
    // Immutable identity of one compiled material variant. Renderer-owned shader and
    // pipeline objects remain outside this class; the resource records which authored
    // material revision and static permutation those objects were compiled for.
    class ELIXIR_API MaterialResource
    {
      public:
        static Ref<const MaterialResource> Create(
            const MaterialInstance& instance,
            const SMaterialShaderPermutation& permutation,
            EMaterialBlendMode blendMode);

        [[nodiscard]] const Ref<const Material>& GetMaterial() const { return m_Material; }
        [[nodiscard]] const SMaterialShaderPermutation& GetPermutation() const { return m_Permutation; }
        [[nodiscard]] EMaterialBlendMode GetBlendMode() const { return m_BlendMode; }
        [[nodiscard]] size_t GetRevision() const { return m_Revision; }
        [[nodiscard]] size_t GetVariantKey() const { return m_VariantKey; }

        [[nodiscard]] bool Matches(
            const SMaterialShaderPermutation& permutation,
            size_t revision, size_t variantKey) const;
        [[nodiscard]] bool IsCompatible(
            const MaterialInstance& instance,
            const SMaterialShaderPermutation& permutation,
            EMaterialBlendMode blendMode) const;

      private:
        MaterialResource(Ref<const Material> material,
            SMaterialShaderPermutation permutation, EMaterialBlendMode blendMode,
            size_t revision, size_t variantKey);

        Ref<const Material> m_Material;
        SMaterialShaderPermutation m_Permutation;
        EMaterialBlendMode m_BlendMode = EMaterialBlendMode::Opaque;
        size_t m_Revision = 0;
        size_t m_VariantKey = 0;
    };
}
