#include "epch.h"
#include "ParticleMaterialFactory.h"

#include <Engine/Graphics/TextureLoader.h>

namespace Elixir::Aether
{
    EMaterialUsage GetParticleMaterialUsage(const EParticleRenderMode mode)
    {
        switch (mode)
        {
            case EParticleRenderMode::Sprite:   return EMaterialUsage::ParticleSprite;
            case EParticleRenderMode::Ribbon:   return EMaterialUsage::ParticleRibbon;
            case EParticleRenderMode::Mesh:     return EMaterialUsage::ParticleMesh;
        }

        return EMaterialUsage::ParticleSprite;
    }

    Ref<Material> CreateParticleMaterial(
        std::string name,
        const EParticleRenderMode renderMode,
        const SParticleMaterialDefinition& definition
    )
    {
        const auto material = CreateRef<Material>(std::move(name));

        const auto result = material->SetUsage(GetParticleMaterialUsage(renderMode), true);
        EE_CORE_ASSERT(result, "Particle material usage must be enabled.")

        MaterialGraph graph;

        const auto baseColor = graph.AddNode({
            .Type = EMaterialNodeType::Constant,
            .OutputType = EMaterialGraphValueType::Float3,
            .ConstantValue = { definition.BaseColor, 0.0f },
        });
        graph.SetChannel(EMaterialChannel::BaseColor, baseColor);

        const auto opacity = graph.AddNode({
            .Type = EMaterialNodeType::Constant,
            .OutputType = EMaterialGraphValueType::Float,
            .ConstantValue = { definition.Opacity, 0.0f, 0.0f, 0.0f },
        });
        graph.SetChannel(EMaterialChannel::Opacity, opacity);

        const auto emissive = graph.AddNode({
            .Type = EMaterialNodeType::Constant,
            .OutputType = EMaterialGraphValueType::Float3,
            .ConstantValue = { definition.Emissive, 0.0f },
        });
        graph.SetChannel(EMaterialChannel::Emissive, emissive);

        if (renderMode == EParticleRenderMode::Sprite && !definition.BaseColorTexturePath.empty())
        {
            constexpr auto texParam = "BaseColorTexture";
            const auto tex = TextureLoader::Load(definition.BaseColorTexturePath);
            material->DefineParameter(texParam, {
                .Kind = EMaterialParameterKind::Texture,
                .DefaultValue = SMaterialParam::MakeTexture(tex),
            });

            const auto texture = graph.AddNode({
                .Type = EMaterialNodeType::TextureSample,
                .TextureParameterName = texParam,
            });

            const auto alpha = graph.AddNode({
                .Type = EMaterialNodeType::ComponentMask,
                .Inputs = { int32_t(texture) },
                .ComponentIndex = 3
            });

            const auto baseColorMul = graph.AddNode({
                .Type = EMaterialNodeType::Multiply,
                .Inputs = { int32_t(baseColor), int32_t(texture) },
            });
            graph.SetChannel(EMaterialChannel::BaseColor, baseColorMul);

            const auto opacityMul = graph.AddNode({
                .Type = EMaterialNodeType::Multiply,
                .Inputs = { int32_t(opacity), int32_t(alpha) },
            });
            graph.SetChannel(EMaterialChannel::Opacity, opacityMul);
        }

        material->SetGraph(std::move(graph));
        return material;
    }
}
