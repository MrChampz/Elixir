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
            }

            return "Engine.DefaultParticle";
        }
    }

    Ref<Material> CreateDefaultParticleMaterial(const EMaterialUsage usage)
    {
        const auto name = GetDefaultParticleMaterialName(usage);
        const auto material = CreateRef<Material>(name);

        const bool usageWasEnabled = material->SetUsage(usage, true);
        EE_CORE_ASSERT(
            usageWasEnabled,
            "A default particle material must enable its particle usage."
        )

        if (usage == EMaterialUsage::ParticleSprite)
        {
            material->DefineParameter(std::string(DEFAULT_SPRITE_TEXTURE_PARAMETER), {
                .Kind = EMaterialParameterKind::Texture,
                .DefaultValue = SMaterialParam::MakeTexture(nullptr),
            });

            MaterialGraph graph;
            const auto texture = graph.AddNode({
                .Type = EMaterialNodeType::TextureSample,
                .TextureParameterName = std::string(DEFAULT_SPRITE_TEXTURE_PARAMETER),
            });
            const auto alpha = graph.AddNode({
                .Type = EMaterialNodeType::ComponentMask,
                .Inputs = { int32_t(texture) },
                .ComponentIndex = 3,
            });
            graph.SetChannel(EMaterialChannel::BaseColor, texture);
            graph.SetChannel(EMaterialChannel::Opacity, alpha);

            material->SetGraph(std::move(graph));
        }

        return material;
    }
}
