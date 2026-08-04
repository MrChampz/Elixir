#include "epch.h"
#include "ParticleMaterialDefaults.h"

namespace Elixir
{
    namespace
    {
        std::string GetDefaultParticleMaterialName(const EMaterialUsage usage)
        {
            switch (usage)
            {
                case EMaterialUsage::ParticleSprite:
                    return "Engine.DefaultParticleSprite";
                case EMaterialUsage::ParticleRibbon:
                    return "Engine.DefaultParticleRibbon";
                case EMaterialUsage::ParticleMesh:
                    return "Engine.DefaultParticleMesh";
                default:
                    return "Engine.DefaultParticle";
            }
        }
    }

    Ref<Material> CreateParticleMaterial(const SParticleMaterialDescription& desc)
    {
        const auto name = GetDefaultParticleMaterialName(desc.Usage);
        const auto material = CreateRef<Material>(name);

        const bool usageWasEnabled = material->SetUsage(desc.Usage, true);
        EE_CORE_ASSERT(
            usageWasEnabled,
            "A default particle material must enable its particle usage."
        )

        MaterialGraph graph;
        const auto baseColor = graph.AddNode({
            .Type = EMaterialNodeType::Constant,
            .OutputType = EMaterialGraphValueType::Float3,
            .ConstantValue = { desc.BaseColor, 0.0f },
        });
        const auto opacity = graph.AddNode({
            .Type = EMaterialNodeType::Constant,
            .OutputType = EMaterialGraphValueType::Float,
            .ConstantValue = { desc.Opacity, 0.0f, 0.0f, 0.0f },
        });
        const auto emissive = graph.AddNode({
            .Type = EMaterialNodeType::Constant,
            .OutputType = EMaterialGraphValueType::Float3,
            .ConstantValue = { desc.Emissive, 0.0f },
        });

        graph.SetChannel(EMaterialChannel::BaseColor, baseColor);
        graph.SetChannel(EMaterialChannel::Opacity, opacity);
        graph.SetChannel(EMaterialChannel::Emissive, emissive);

        if (desc.Usage == EMaterialUsage::ParticleSprite)
        {
            material->DefineParameter(std::string(DEFAULT_SPRITE_TEXTURE_PARAMETER), {
                .Kind = EMaterialParameterKind::Texture,
                .DefaultValue = SMaterialParam::MakeTexture(nullptr),
            });

            const auto texture = graph.AddNode({
                .Type = EMaterialNodeType::TextureSample,
                .TextureParameterName = std::string(DEFAULT_SPRITE_TEXTURE_PARAMETER),
            });
            const auto textureOpacity = graph.AddNode({
                .Type = EMaterialNodeType::ComponentMask,
                .Inputs = { int32_t(texture) },
                .ComponentIndex = 3,
            });
            const auto texturedBaseColor = graph.AddNode({
                .Type = EMaterialNodeType::Multiply,
                .Inputs = { int32_t(baseColor), int32_t(texture) },
            });
            const auto texturedOpacity = graph.AddNode({
                .Type = EMaterialNodeType::Multiply,
                .Inputs = { int32_t(opacity), int32_t(textureOpacity) },
            });
            graph.SetChannel(EMaterialChannel::BaseColor, texturedBaseColor);
            graph.SetChannel(EMaterialChannel::Opacity, texturedOpacity);

        }

        material->SetGraph(std::move(graph));

        return material;
    }
}
