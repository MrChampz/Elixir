#include "epch.h"
#include "Material.h"

namespace Elixir
{
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
