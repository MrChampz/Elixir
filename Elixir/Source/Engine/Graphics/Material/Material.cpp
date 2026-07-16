#include "epch.h"
#include "Material.h"

#include <algorithm>

namespace Elixir
{
    void MaterialInstance::SetParent(const Ref<Material>& parent)
    {
        m_Parent = parent;
        if (!m_Parent)
            return;

        // Reparenting a slot to a newly compiled graph material keeps compatible glTF
        // and graph overrides, while dropping values no longer declared by the asset.
        for (auto it = m_Overrides.begin(); it != m_Overrides.end();)
        {
            const SMaterialParam* definition = m_Parent->GetDefault(it->first);
            if (!definition || definition->Type != it->second.Type)
                it = m_Overrides.erase(it);
            else
                ++it;
        }
    }

    void MaterialInstance::ApplyCompatibleOverridesFrom(const MaterialInstance& source)
    {
        if (!m_Parent)
            return;

        for (const auto& [name, value] : source.m_Overrides)
        {
            const SMaterialParam* definition = m_Parent->GetDefault(name);
            if (definition && definition->Type == value.Type)
                m_Overrides[name] = value;
        }
    }

    uint32_t MaterialInstance::CollectGraphParams(glm::vec4* out, uint32_t maxCount) const
    {
        if (!out || maxCount == 0 || !m_Parent || !m_Parent->HasGraph())
            return 0;

        uint32_t count = 0;
        for (const auto& parameter : m_Parent->GetGraphParameters())
        {
            if (parameter.Slot >= maxCount)
                continue;
            const SMaterialParam* value = Resolve(parameter.Name);
            if (!value)
                continue;

            if (parameter.Type == SMaterialParam::EType::Scalar && value->Type == SMaterialParam::EType::Scalar)
                out[parameter.Slot] = glm::vec4(value->Scalar, 0.0f, 0.0f, 0.0f);
            else if (parameter.Type == SMaterialParam::EType::Vector && value->Type == SMaterialParam::EType::Vector)
                out[parameter.Slot] = value->Vector;
            else
                continue;
            count = std::max(count, parameter.Slot + 1);
        }
        return count;
    }

    Ref<Material> Material::CreateStandardPBR()
    {
        auto material = CreateRef<Material>("StandardPBR");

        // Scalars (glTF metallic-roughness defaults).
        material->SetDefault("Metallic", SMaterialParam::MakeScalar(1.0f));
        material->SetDefault("Roughness", SMaterialParam::MakeScalar(1.0f));
        material->SetDefault("OcclusionStrength", SMaterialParam::MakeScalar(1.0f));
        material->SetDefault("NormalScale", SMaterialParam::MakeScalar(1.0f));
        material->SetDefault("AlphaCutoff", SMaterialParam::MakeScalar(0.5f));
        material->SetDefault("AlphaMode", SMaterialParam::MakeScalar(0.0f)); // 0 opaque, 1 mask, 2 blend
        material->SetDefault("ClearcoatFactor", SMaterialParam::MakeScalar(0.0f));
        material->SetDefault("ClearcoatRoughness", SMaterialParam::MakeScalar(0.0f));
        material->SetDefault("SpecularFactor", SMaterialParam::MakeScalar(1.0f));
        material->SetDefault("Transmission", SMaterialParam::MakeScalar(0.0f));

        // Vectors.
        material->SetDefault("BaseColorFactor", SMaterialParam::MakeVector(glm::vec4(1.0f)));
        material->SetDefault("EmissiveFactor", SMaterialParam::MakeVector(glm::vec4(0.0f)));
        material->SetDefault("SpecularColorFactor", SMaterialParam::MakeVector(glm::vec4(1.0f)));

        // KHR_texture_transform (xy scale, zw offset), identity by default.
        const auto identity = SMaterialParam::MakeVector(glm::vec4(1.0f, 1.0f, 0.0f, 0.0f));
        material->SetDefault("BaseColorTransform", identity);
        material->SetDefault("MetallicRoughnessTransform", identity);
        material->SetDefault("NormalTransform", identity);
        material->SetDefault("EmissiveTransform", identity);
        material->SetDefault("OcclusionTransform", identity);

        // Textures (none by default).
        const auto noTexture = SMaterialParam::MakeTexture(nullptr);
        material->SetDefault("BaseColorTexture", noTexture);
        material->SetDefault("MetallicRoughnessTexture", noTexture);
        material->SetDefault("NormalTexture", noTexture);
        material->SetDefault("EmissiveTexture", noTexture);
        material->SetDefault("OcclusionTexture", noTexture);
        material->SetDefault("SpecularTexture", noTexture);
        material->SetDefault("SpecularColorTexture", noTexture);

        return material;
    }
}
