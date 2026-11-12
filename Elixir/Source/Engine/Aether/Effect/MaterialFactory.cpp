#include "epch.h"
#include "MaterialFactory.h"

#include <Engine/Graphics/TextureLoader.h>
#include <Engine/Materials/Nodes/Multiply.h>
#include <Engine/Materials/Nodes/Add.h>
#include <Engine/Materials/Nodes/Custom.h>
#include <Engine/Materials/Nodes/ParticleTime.h>
#include <Engine/Materials/Nodes/SceneColor.h>
#include <Engine/Materials/Nodes/RadialGradientExponential.h>
#include <Engine/Materials/Nodes/Constant.h>
#include <Engine/Materials/Nodes/ComponentMask.h>
#include <Engine/Materials/Nodes/Color.h>
#include <Engine/Materials/Nodes/TextureSample.h>
#include <Engine/Materials/Nodes/TextureAtlasSample.h>

namespace Elixir::Aether::Effect
{
    using namespace Elixir::Materials::Nodes;

    Ref<Material> CreateMaterial(
        std::string name,
        const SMaterialDescription& desc
    )
    {
        if (!desc.IsValid())
        {
            EE_CORE_ERROR("Invalid particle material description '{}'.", name)
            return nullptr;
        }

        const auto material = CreateRef<Material>(
            std::move(name),
            EMaterialUsage::Particle
        );

        material->SetBlendMode(desc.BlendMode);
        material->SetShadingModel(desc.ShadingModel);

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

        const auto color = graph.AddNode<Color>();
        const auto particleAlpha = graph.AddNode<ComponentMask>(3);
        graph.Connect(color, particleAlpha, 0);

        auto coloredBase = graph.AddNode<Multiply>();
        graph.Connect(baseColor, coloredBase, 0);
        graph.Connect(color, coloredBase, 1);
        graph.SetChannel(EMaterialChannel::BaseColor, coloredBase);

        const auto coloredOpacity = graph.AddNode<Multiply>();
        graph.Connect(opacity, coloredOpacity, 0);
        graph.Connect(particleAlpha, coloredOpacity, 1);
        graph.SetChannel(EMaterialChannel::Opacity, coloredOpacity);

        const auto age = graph.AddNode<ParticleTime>();
        bool valid = true;

        const auto sampleAtlas = [&](const char* parameter, const std::string& path)
        {
            const auto texture = TextureLoader::Load(path);
            if (desc.Atlas == STextureAtlasLayout{} && !desc.InterpolateAtlas)
            {
                material->DefineParameter(
                    parameter,
                    {
                        .Kind = EMaterialParameterKind::Texture,
                        .DefaultValue = SMaterialParameter::MakeTexture(texture)
                    }
                );
                return graph.AddNode<TextureSample>(parameter);
            }

            const auto atlas = TextureAtlas::Create(texture, desc.Atlas);
            if (!atlas)
            {
                valid = false;
                return uint32_t{};
            }

            material->DefineParameter(
                parameter,
                {
                    .Kind = EMaterialParameterKind::TextureAtlas,
                    .DefaultValue = SMaterialParameter::MakeTextureAtlas(atlas)
                }
            );

            const auto sample = graph.AddNode<TextureAtlasSample>(
                parameter,
                desc.InterpolateAtlas,
                ETextureAtlasFrameMode::Normalized
            );

            graph.Connect(age, sample, 1);
            return sample;
        };

        const auto multiply = [&](const uint32_t left, const uint32_t right)
        {
            const auto node = graph.AddNode<Multiply>();
            graph.Connect(left, node, 0);
            graph.Connect(right, node, 1);
            return node;
        };

        const auto sampleData = [&](const char* parameter, const std::string& path)
        {
            const auto texture = TextureLoader::Load(path, EImageFormat::R8G8B8A8_UNORM);
            material->DefineParameter(
                parameter,
                {
                    .Kind = EMaterialParameterKind::Texture,
                    .DefaultValue = SMaterialParameter::MakeTexture(texture)
                }
            );
            return graph.AddNode<TextureSample>(parameter, ETextureSampleType::Normal);
        };

        if (!desc.BaseColorTexturePath.empty())
        {
            const auto texture = sampleAtlas("BaseColorTexture", desc.BaseColorTexturePath);
            const auto alpha = graph.AddNode<ComponentMask>(3);
            graph.Connect(texture, alpha, 0);

            auto sampledColor = texture;

            graph.SetChannel(EMaterialChannel::Opacity, multiply(coloredOpacity, alpha));

            if (!desc.ColorRampTexturePath.empty())
            {
                material->DefineParameter(
                    "ColorRampTexture",
                    {
                        .Kind = EMaterialParameterKind::Texture,
                        .DefaultValue = SMaterialParameter::MakeTexture(
                            TextureLoader::Load(desc.ColorRampTexturePath)
                        )
                    }
                );

                const auto uv = graph.AddNode<Custom>(
                    "float2(saturate(dot({0}.rgb, float3(0.299, 0.587, 0.114))), 0.5)",
                    EMaterialValueType::Float2,
                    std::vector<SMaterialNodeInput>{
                        { "Color", EMaterialValueType::Float4 }
                    }
                );
                graph.Connect(texture, uv, 0);

                const auto ramp = graph.AddNode<TextureSample>(
                    "ColorRampTexture",
                    ETextureSampleType::Color,
                    STextureSampleSettings{ .Address = ETextureAddress::Clamp }
                );
                graph.Connect(uv, ramp, 0);

                sampledColor = ramp;
            }

            coloredBase = multiply(coloredBase, sampledColor);
        }

        graph.SetChannel(EMaterialChannel::BaseColor, coloredBase);

        if (!desc.NormalTexturePath.empty())
        {
            const auto normal = sampleData("NormalTexture", desc.NormalTexturePath);
            graph.SetChannel(EMaterialChannel::Normal, normal);
        }

        if (!desc.EmissiveTexturePath.empty())
        {
            const auto texture = sampleAtlas("EmissiveTexture", desc.EmissiveTexturePath);
            const auto scale = graph.AddNode<Constant>(
                glm::vec4{ desc.EmissiveScale, 0, 0, 0 },
                EMaterialValueType::Float
            );
            const auto emission = multiply(multiply(texture, scale), particleAlpha);

            const auto sum = graph.AddNode<Add>();
            graph.Connect(emissive, sum, 0);
            graph.Connect(emission, sum, 1);

            graph.SetChannel(EMaterialChannel::Emissive, sum);
        }

        if (!desc.RefractionTexturePath.empty())
        {
            const auto normal = sampleData("RefractionTexture", desc.RefractionTexturePath);

            const auto offset = graph.AddNode<Custom>(
                "{0}.xy * " + std::to_string(desc.RefractionStrength),
                EMaterialValueType::Float2,
                std::vector<SMaterialNodeInput>{
                    { "Normal", EMaterialValueType::Float3 }
                }
            );
            graph.Connect(normal, offset, 0);

            const auto scene = graph.AddNode<SceneColor>();
            graph.Connect(offset, scene, 0);
            graph.SetChannel(EMaterialChannel::Emissive, scene);

            const auto black = graph.AddNode<Constant>(
                glm::vec4{ 0 },
                EMaterialValueType::Float3
            );
            graph.SetChannel(EMaterialChannel::BaseColor, black);

            const auto falloff = graph.AddNode<RadialGradientExponential>(
                glm::vec2{ 0.5f },
                0.5f,
                1.0f
            );
            graph.SetChannel(EMaterialChannel::Opacity, multiply(coloredOpacity, falloff));
        }

        if (!valid)
            return nullptr;

        material->SetGraph(std::move(graph));
        return material;
    }
}
