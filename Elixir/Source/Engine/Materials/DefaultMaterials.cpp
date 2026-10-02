#include "epch.h"
#include "DefaultMaterials.h"

#include <Engine/Materials/Nodes/Constant.h>
#include <Engine/Materials/Nodes/Checkerboard.h>
#include <Engine/Materials/Nodes/RadialGradientExponential.h>
#include <Engine/Materials/Nodes/Color.h>
#include <Engine/Materials/Nodes/ComponentMask.h>
#include <Engine/Materials/Nodes/Multiply.h>

namespace Elixir::Materials
{
    using namespace Nodes;

    namespace
    {
        Ref<Material> MakeMaterial(
            std::string name,
            const EMaterialUsage usage,
            MaterialGraph graph,
            const EMaterialBlendMode blendMode = EMaterialBlendMode::Opaque
        )
        {
            const auto material = CreateRef<Material>(std::move(name), usage);

            if (blendMode != EMaterialBlendMode::Opaque)
            {
                const auto result = material->SetBlendMode(blendMode);
                EE_CORE_ASSERT(result, "Default material blend mode must be enabled.")
            }

            material->SetGraph(std::move(graph));

            return material;
        }

        Ref<Material> CreateDefaultSurfaceMaterial()
        {
            MaterialGraph graph;

            const auto checkerboard = graph.AddNode<Checkerboard>(8.0f);
            graph.SetChannel(EMaterialChannel::BaseColor, checkerboard);

            return MakeMaterial(
                "Engine.Materials.Defaults.Surface",
                EMaterialUsage::Surface,
                std::move(graph)
            );
        }

        Ref<Material> CreateDefaultParticleMaterial()
        {
            MaterialGraph graph;

            const auto baseColor = graph.AddNode<Constant>(
                glm::vec4{ 1.0f, 1.0f, 1.0f, 0.0f },
                EMaterialValueType::Float3
            );
            const auto color = graph.AddNode<Color>();
            const auto coloredBase = graph.AddNode<Multiply>();
            graph.Connect(baseColor, coloredBase, 0);
            graph.Connect(color, coloredBase, 1);
            graph.SetChannel(EMaterialChannel::BaseColor, coloredBase);

            const auto opacity = graph.AddNode<RadialGradientExponential>(
                glm::vec2{ 0.5f, 0.5f },
                0.5f,
                2.0f
            );
            const auto particleAlpha = graph.AddNode<ComponentMask>(3);
            graph.Connect(color, particleAlpha, 0);
            const auto coloredOpacity = graph.AddNode<Multiply>();
            graph.Connect(opacity, coloredOpacity, 0);
            graph.Connect(particleAlpha, coloredOpacity, 1);
            graph.SetChannel(EMaterialChannel::Opacity, coloredOpacity);

            return MakeMaterial(
                "Engine.Materials.Defaults.Particle",
                EMaterialUsage::Particle,
                std::move(graph),
                EMaterialBlendMode::Translucent
            );
        }
    }

    DefaultMaterialArray CreateDefaultMaterials()
    {
        return {
            CreateDefaultSurfaceMaterial(),
            CreateDefaultParticleMaterial()
        };
    }
}
