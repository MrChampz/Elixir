#include "epch.h"
#include "MaterialFactory.h"

#include <Engine/Graphics/TextureLoader.h>

#include <Engine/Materials/Nodes/Add.h>
#include <Engine/Materials/Nodes/Subtract.h>
#include <Engine/Materials/Nodes/Multiply.h>
#include <Engine/Materials/Nodes/Divide.h>
#include <Engine/Materials/Nodes/Constant.h>
#include <Engine/Materials/Nodes/ComponentMask.h>
#include <Engine/Materials/Nodes/TextureSample.h>

namespace Elixir::Aether::Effect
{
    using namespace Elixir::Materials::Nodes;

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

        const auto baseColor = graph.AddNode<Constant>(
            glm::vec4{ desc.BaseColor, 0.0f },
            EMaterialValueType::Float3
        );
        graph.SetChannel(EMaterialChannel::BaseColor, baseColor);

        const auto opacity = graph.AddNode<Constant>(
            glm::vec4{ desc.Opacity, 0.0f, 0.0f, 0.0f },
            EMaterialValueType::Float
        );
        graph.SetChannel(EMaterialChannel::Opacity, opacity);

        const auto emissive = graph.AddNode<Constant>(
            glm::vec4{ desc.Emissive, 0.0f },
            EMaterialValueType::Float3
        );
        graph.SetChannel(EMaterialChannel::Emissive, emissive);

        if (renderMode == Core::EParticleRenderMode::Sprite &&
            !desc.BaseColorTexturePath.empty())
        {
            constexpr auto texParam = "BaseColorTexture";
            const auto tex = TextureLoader::Load(desc.BaseColorTexturePath);
            material->DefineParameter(texParam, {
                .Kind = EMaterialParameterKind::Texture,
                .DefaultValue = SMaterialParameter::MakeTexture(tex),
            });

            const auto texture = graph.AddNode<TextureSample>(texParam);
            const auto alpha = graph.AddNode<ComponentMask>(3);
            graph.Connect(texture, alpha, 0);

            const auto baseColorMul = graph.AddNode<Multiply>();
            graph.Connect(baseColor, baseColorMul, 0);
            graph.Connect(texture, baseColorMul, 1);
            graph.SetChannel(EMaterialChannel::BaseColor, baseColorMul);

            const auto opacityMul = graph.AddNode<Multiply>();
            graph.Connect(opacity, opacityMul, 0);
            graph.Connect(alpha, opacityMul, 1);
            graph.SetChannel(EMaterialChannel::Opacity, opacityMul);
        }

        material->SetGraph(std::move(graph));
        return material;
    }
}
