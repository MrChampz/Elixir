#include "epch.h"
#include "DefaultMaterials.h"

#include <Engine/Material/Nodes/Constant.h>
#include <Engine/Material/Nodes/Checkerboard.h>
#include <Engine/Material/Nodes/RadialGradientExponential.h>

namespace Elixir
{
    using namespace Materials::Nodes;

    namespace
    {
        Ref<Material> MakeMaterial(
            std::string name,
            const EMaterialUsage usage,
            MaterialGraph graph
        )
        {
            const auto material = CreateRef<Material>(std::move(name));
            const auto result = material->SetUsage(usage, true);
            EE_CORE_ASSERT(result, "Default material usage must be enabled.")
            material->SetGraph(std::move(graph));
            return material;
        }

        Ref<Material> CreateDefaultSpriteMaterial()
        {
            MaterialGraph graph;

            const auto baseColor = graph.AddNode<Constant>(
                glm::vec4{ 1.0f, 1.0f, 1.0f, 0.0f },
                EMaterialValueType::Float3
            );
            graph.SetChannel(EMaterialChannel::BaseColor, baseColor);

            const auto opacity = graph.AddNode<RadialGradientExponential>(
                glm::vec2{ 0.5f, 0.5f },
                0.5f,
                2.0f
            );
            graph.SetChannel(EMaterialChannel::Opacity, opacity);

            return MakeMaterial(
                "Engine.Materials.Defaults.ParticleSprite",
                EMaterialUsage::ParticleSprite,
                std::move(graph)
            );
        }

        Ref<Material> CreateDefaultRibbonMaterial()
        {
            MaterialGraph graph;

            const auto checkerboard = graph.AddNode<Checkerboard>(8.0f);
            graph.SetChannel(EMaterialChannel::BaseColor, checkerboard);

            return MakeMaterial(
                "Engine.Materials.Defaults.ParticleRibbon",
                EMaterialUsage::ParticleRibbon,
                std::move(graph)
            );
        }

        Ref<Material> CreateDefaultMeshMaterial()
        {
            MaterialGraph graph;

            const auto checkerboard = graph.AddNode<Checkerboard>(8.0f);
            graph.SetChannel(EMaterialChannel::BaseColor, checkerboard);

            return MakeMaterial(
                "Engine.Materials.Defaults.ParticleMesh",
                EMaterialUsage::ParticleMesh,
                std::move(graph)
            );
        }
    }

    DefaultMaterialArray CreateDefaultMaterials()
    {
        return {
            CreateDefaultSpriteMaterial(),
            CreateDefaultRibbonMaterial(),
            CreateDefaultMeshMaterial()
        };
    }
}
