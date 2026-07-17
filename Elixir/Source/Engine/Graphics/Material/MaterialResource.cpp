#include "epch.h"
#include "MaterialResource.h"

namespace Elixir
{
    MaterialResource::MaterialResource(Ref<const Material> material,
        const SMaterialShaderPermutation permutation, const EMaterialBlendMode blendMode,
        const size_t revision, const size_t variantKey)
        : m_Material(std::move(material)), m_Permutation(permutation),
          m_BlendMode(blendMode), m_Revision(revision), m_VariantKey(variantKey)
    {
    }

    Ref<const MaterialResource> MaterialResource::Create(
        const MaterialInstance& instance,
        const SMaterialShaderPermutation& permutation,
        const EMaterialBlendMode blendMode)
    {
        const Ref<Material>& material = instance.GetParent();
        if (!material || !MaterialShaderPermutation::Matches(
                material->GetDomain(), material->GetUsages(), permutation))
            return nullptr;

        return Ref<const MaterialResource>(new MaterialResource(
            material, permutation, blendMode,
            instance.GraphRevision(), instance.StaticVariantKey()));
    }

    bool MaterialResource::Matches(
        const SMaterialShaderPermutation& permutation,
        const size_t revision, const size_t variantKey) const
    {
        return m_Permutation == permutation
            && m_Revision == revision
            && m_VariantKey == variantKey;
    }

    bool MaterialResource::IsCompatible(
        const MaterialInstance& instance,
        const SMaterialShaderPermutation& permutation,
        const EMaterialBlendMode blendMode) const
    {
        return instance.GetParent().get() == m_Material.get()
            && m_BlendMode == blendMode
            && Matches(permutation, instance.GraphRevision(), instance.StaticVariantKey());
    }
}
