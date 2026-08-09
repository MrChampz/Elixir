#include "epch.h"
#include "MaterialFactory.h"

#include <Engine/Graphics/TextureLoader.h>

namespace Elixir::Aether::Effect
{
    EMaterialUsage GetMaterialUsage(const Core::EParticleRenderMode mode)
    {
        switch (mode)
        {
            case Core::EParticleRenderMode::Sprite: return EMaterialUsage::ParticleSprite;
            case Core::EParticleRenderMode::Ribbon: return EMaterialUsage::ParticleRibbon;
            case Core::EParticleRenderMode::Mesh:   return EMaterialUsage::ParticleMesh;
        }

        return EMaterialUsage::ParticleSprite;
    }

    Ref<Material> CreateMaterial(
        std::string name,
        const Core::EParticleRenderMode renderMode,
        const SMaterialDescription& desc
    )
    {
        const auto material = CreateRef<Material>(std::move(name));

        const auto result = material->SetUsage(GetMaterialUsage(renderMode), true);
        EE_CORE_ASSERT(result, "Particle material usage must be enabled.")

        MaterialGraph graph;

        const auto baseColor = graph.AddNode({
            .Type = EMaterialNodeType::Constant,
            .OutputType = EMaterialGraphValueType::Float3,
            .ConstantValue = { desc.BaseColor, 0.0f },
        });
        graph.SetChannel(EMaterialChannel::BaseColor, baseColor);

        const auto opacity = graph.AddNode({
            .Type = EMaterialNodeType::Constant,
            .OutputType = EMaterialGraphValueType::Float,
            .ConstantValue = { desc.Opacity, 0.0f, 0.0f, 0.0f },
        });
        graph.SetChannel(EMaterialChannel::Opacity, opacity);

        const auto emissive = graph.AddNode({
            .Type = EMaterialNodeType::Constant,
            .OutputType = EMaterialGraphValueType::Float3,
            .ConstantValue = { desc.Emissive, 0.0f },
        });
        graph.SetChannel(EMaterialChannel::Emissive, emissive);

        if (renderMode == Core::EParticleRenderMode::Sprite && !desc.BaseColorTexturePath.empty())
        {
            constexpr auto texParam = "BaseColorTexture";
            const auto tex = TextureLoader::Load(desc.BaseColorTexturePath);
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
