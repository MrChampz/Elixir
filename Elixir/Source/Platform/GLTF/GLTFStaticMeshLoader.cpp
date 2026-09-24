#include "epch.h"
#include "GLTFStaticMeshLoader.h"

#include <Engine/Graphics/Texture.h>
#include <Engine/Graphics/TextureLoader.h>
#include <Engine/Materials/DefaultMaterials.h>
#include <Engine/Materials/Material.h>
#include <Engine/Materials/MaterialParameter.h>
#include <Engine/Materials/Nodes/Add.h>
#include <Engine/Materials/Nodes/Append.h>
#include <Engine/Materials/Nodes/ComponentMask.h>
#include <Engine/Materials/Nodes/Constant.h>
#include <Engine/Materials/Nodes/Cosine.h>
#include <Engine/Materials/Nodes/Dot.h>
#include <Engine/Materials/Nodes/FlattenNormal.h>
#include <Engine/Materials/Nodes/Lerp.h>
#include <Engine/Materials/Nodes/Multiply.h>
#include <Engine/Materials/Nodes/Parameter.h>
#include <Engine/Materials/Nodes/Sine.h>
#include <Engine/Materials/Nodes/Subtract.h>
#include <Engine/Materials/Nodes/TexCoord.h>
#include <Engine/Materials/Nodes/TextureSample.h>

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <stb_image.h>

namespace Elixir
{
    namespace
    {
        SStaticMeshBounds GetBounds(const std::vector<SStaticMeshVertex>& vertices)
        {
            SStaticMeshBounds bounds{
                .Min = glm::vec3(std::numeric_limits<float>::max()),
                .Max = glm::vec3(std::numeric_limits<float>::lowest()),
            };

            for (const auto& vertex : vertices)
            {
                bounds.Min = glm::min(bounds.Min, vertex.Position);
                bounds.Max = glm::max(bounds.Max, vertex.Position);
            }

            return bounds;
        }

        void ExpandBounds(SStaticMeshBounds& target, const SStaticMeshBounds& source)
        {
            target.Min = glm::min(target.Min, source.Min);
            target.Max = glm::max(target.Max, source.Max);
        }

        uint32_t GetFullMipLevelCount(const uint32_t width, const uint32_t height)
        {
            uint32_t largestDimension = std::max(width, height);
            uint32_t levelCount = 1;

            while (largestDimension > 1)
            {
                largestDimension >>= 1;
                ++levelCount;
            }

            return levelCount;
        }

        Ref<Texture> DecodeImage(
            const GraphicsContext& context,
            const std::byte* bytes,
            const size_t size,
            const STexture2DCreateInfo& info
        )
        {
            int width = 0;
            int height = 0;
            int channels = 0;

            stbi_uc* pixels = stbi_load_from_memory(
                reinterpret_cast<const stbi_uc*>(bytes),
                (int)size,
                &width,
                &height,
                &channels,
                STBI_rgb_alpha
            );

            if (!pixels)
            {
                EE_CORE_WARN(
                    "Failed to decode embedded glTF image: {}.",
                    stbi_failure_reason()
                )
                return nullptr;
            }

            auto createInfo = info;
            createInfo.InitialData = pixels;
            createInfo.Width = width;
            createInfo.Height = height;

            if (createInfo.GenerateMipmaps)
                createInfo.MipLevels = GetFullMipLevelCount(width, height);

            const auto texture = Texture2D::Create(&context, createInfo);

            stbi_image_free(pixels);

            return texture;
        }

        Ref<Texture> LoadImage(
            const GraphicsContext& context,
            fastgltf::Asset& asset,
            const size_t imageIndex,
            const std::filesystem::path& dir,
            const STexture2DCreateInfo& info
        )
        {
            if (imageIndex >= asset.images.size())
            {
                EE_CORE_WARN("glTF material references an invalid image index.")
                return nullptr;
            }

            Ref<Texture> texture;
            auto& image = asset.images[imageIndex];

            std::visit(
                fastgltf::visitor{
                    [](const auto&) {},
                    [&](const fastgltf::sources::URI& source)
                    {
                        const auto path = dir / source.uri.fspath();
                        texture = TextureLoader::Load(path, info);
                    },
                    [&](const fastgltf::sources::Array& source)
                    {
                        texture = DecodeImage(
                            context,
                            source.bytes.data(),
                            source.bytes.size(),
                            info
                        );
                    },
                    [&](const fastgltf::sources::Vector& source)
                    {
                        texture = DecodeImage(
                            context,
                            source.bytes.data(),
                            source.bytes.size(),
                            info
                        );
                    },
                    [&](const fastgltf::sources::BufferView& viewSource)
                    {
                        if (viewSource.bufferViewIndex >= asset.bufferViews.size())
                        {
                            EE_CORE_WARN("glTF image references an invalid buffer view.")
                            return;
                        }

                        const auto& view = asset.bufferViews[viewSource.bufferViewIndex];
                        if (view.bufferIndex >= asset.buffers.size())
                        {
                            EE_CORE_WARN("glTF image references an invalid buffer.")
                            return;
                        }

                        auto& buffer = asset.buffers[view.bufferIndex];
                        std::visit(
                            fastgltf::visitor{
                                [](const auto&) {},
                                [&](const fastgltf::sources::Array& source)
                                {
                                    texture = DecodeImage(
                                        context,
                                        source.bytes.data() + view.byteOffset,
                                        view.byteLength,
                                        info
                                    );
                                },
                                [&](const fastgltf::sources::Vector& source)
                                {
                                    texture = DecodeImage(
                                        context,
                                        source.bytes.data() + view.byteOffset,
                                        view.byteLength,
                                        info
                                    );
                                }
                            },
                            buffer.data
                        );
                    }
                },
                image.data
            );

            return texture;
        }

        Ref<Texture> LoadTexture(
            const GraphicsContext& context,
            fastgltf::Asset& asset,
            const size_t textureIndex,
            const std::filesystem::path& dir,
            const STexture2DCreateInfo& info,
            std::unordered_map<uint64_t, Ref<Texture>>& cache
        )
        {
            const uint64_t cacheKey =
                (uint64_t(textureIndex) << 2) |
                (info.Format == EImageFormat::R8G8B8A8_SRGB ? 1ull : 0ull) |
                (info.GenerateMipmaps ? 2ull : 0ull);

            const auto existing = cache.find(cacheKey);
            if (existing != cache.end())
            {
                return existing->second;
            }

            if (textureIndex >= asset.textures.size())
            {
                EE_CORE_WARN("glTF material references an invalid texture index.")
                return nullptr;
            }

            const auto& texture = asset.textures[textureIndex];
            if (!texture.imageIndex)
            {
                EE_CORE_WARN("glTF texture does not reference an image.")
                return nullptr;
            }

            const auto result = LoadImage(
                context,
                asset,
                *texture.imageIndex,
                dir,
                info
            );
            cache.emplace(cacheKey, result);

            return result;
        }

        EMaterialBlendMode GetBlendMode(const fastgltf::AlphaMode mode)
        {
            using namespace Materials;

            switch (mode)
            {
                case fastgltf::AlphaMode::Mask:
                    return EMaterialBlendMode::Masked;

                case fastgltf::AlphaMode::Blend:
                    return EMaterialBlendMode::Translucent;

                case fastgltf::AlphaMode::Opaque:
                default:
                    return EMaterialBlendMode::Opaque;
            }
        }

        uint32_t GetTextureCoordinateIndex(const fastgltf::TextureInfo& source)
        {
            const size_t index = source.transform && source.transform->texCoordIndex
                ? *source.transform->texCoordIndex
                : source.texCoordIndex;

            if (index <= 1)
                return static_cast<uint32_t>(index);

            EE_CORE_WARN(
                "glTF texture coordinate channel {} is unsupported; using TEXCOORD_0.",
                index
            )
            return 0;
        }

        uint32_t AddTextureSample(
            MaterialGraph& graph,
            Material& material,
            const std::string_view parameterName,
            const fastgltf::TextureInfo& source,
            const Nodes::ETextureSampleType sampleType = Nodes::ETextureSampleType::Color
        )
        {
            using namespace Materials;
            using namespace Materials::Nodes;

            const auto sample = graph.AddNode<TextureSample>(
                std::string(parameterName),
                sampleType
            );
            const auto texCoord = graph.AddNode<TexCoord>(
                GetTextureCoordinateIndex(source)
            );

            if (!source.transform)
            {
                graph.Connect(texCoord, sample, 0);
                return sample;
            }

            const std::string scaleParameter = std::string(parameterName) + "UVScale";
            const std::string offsetParameter = std::string(parameterName) + "UVOffset";
            const std::string rotationParameter = std::string(parameterName) + "UVRotation";

            EE_CORE_ASSERT(material.DefineParameter(scaleParameter, {
                .Kind = EMaterialParameterKind::Value,
                .ValueType = EMaterialValueType::Float2,
                .DefaultValue = SMaterialParameter::MakeVector({
                    source.transform->uvScale.x(),
                    source.transform->uvScale.y(),
                    0.0f,
                    0.0f,
                }),
            }), "Could not define texture UV scale parameter.")

            EE_CORE_ASSERT(material.DefineParameter(offsetParameter, {
                .Kind = EMaterialParameterKind::Value,
                .ValueType = EMaterialValueType::Float2,
                .DefaultValue = SMaterialParameter::MakeVector({
                    source.transform->uvOffset.x(),
                    source.transform->uvOffset.y(),
                    0.0f,
                    0.0f,
                }),
            }), "Could not define texture UV offset parameter.")

            EE_CORE_ASSERT(material.DefineParameter(rotationParameter, {
                .Kind = EMaterialParameterKind::Value,
                .ValueType = EMaterialValueType::Float,
                .DefaultValue = SMaterialParameter::MakeScalar(source.transform->rotation),
            }), "Could not define texture UV rotation parameter.")

            const auto scale = graph.AddNode<Parameter>(
                scaleParameter,
                EMaterialValueType::Float2
            );
            const auto offset = graph.AddNode<Parameter>(
                offsetParameter,
                EMaterialValueType::Float2
            );
            const auto rotation = graph.AddNode<Parameter>(
                rotationParameter,
                EMaterialValueType::Float
            );
            const auto scaled = graph.AddNode<Multiply>();
            graph.Connect(texCoord, scaled, 0);
            graph.Connect(scale, scaled, 1);

            const auto sine = graph.AddNode<Sine>();
            const auto cosine = graph.AddNode<Cosine>();
            graph.Connect(rotation, sine, 0);
            graph.Connect(rotation, cosine, 0);

            const auto zero = graph.AddNode<Constant>(
                glm::vec4(0.0f),
                EMaterialValueType::Float
            );
            const auto negativeSine = graph.AddNode<Subtract>();
            graph.Connect(zero, negativeSine, 0);
            graph.Connect(sine, negativeSine, 1);

            const auto firstRow = graph.AddNode<Append>();
            graph.Connect(cosine, firstRow, 0);
            graph.Connect(negativeSine, firstRow, 1);

            const auto secondRow = graph.AddNode<Append>();
            graph.Connect(sine, secondRow, 0);
            graph.Connect(cosine, secondRow, 1);

            const auto rotatedX = graph.AddNode<Dot>();
            graph.Connect(scaled, rotatedX, 0);
            graph.Connect(firstRow, rotatedX, 1);

            const auto rotatedY = graph.AddNode<Dot>();
            graph.Connect(scaled, rotatedY, 0);
            graph.Connect(secondRow, rotatedY, 1);

            const auto rotated = graph.AddNode<Append>();
            graph.Connect(rotatedX, rotated, 0);
            graph.Connect(rotatedY, rotated, 1);

            const auto transformed = graph.AddNode<Add>();
            graph.Connect(rotated, transformed, 0);
            graph.Connect(offset, transformed, 1);
            graph.Connect(transformed, sample, 0);

            return sample;
        }

        Ref<Material> CreateSurfaceMaterial(
            const fastgltf::Material& source,
            const size_t materialIndex,
            const std::string_view meshName,
            const Ref<Texture>& baseColorTexture,
            const Ref<Texture>& metallicRoughnessTexture,
            const Ref<Texture>& normalTexture,
            const Ref<Texture>& occlusionTexture,
            const Ref<Texture>& emissiveTexture,
            const Ref<Texture>& specularTexture,
            const Ref<Texture>& specularColorTexture,
            const Ref<Texture>& clearCoatTexture,
            const Ref<Texture>& clearCoatRoughnessTexture,
            const Ref<Texture>& clearCoatNormalTexture
        )
        {
            using namespace Materials;
            using namespace Materials::Nodes;

            const std::string name = source.name.empty()
                ? std::string(meshName) + ".Material." + std::to_string(materialIndex)
                : std::string(source.name);

            const auto material = CreateRef<Material>(name);
            material->SetUsage(EMaterialUsage::Surface, true);
            material->SetBlendMode(GetBlendMode(source.alphaMode));
            material->SetDoubleSided(source.doubleSided);

            if (source.clearcoat)
                material->SetShadingModel(EMaterialShadingModel::ClearCoat);

            if (source.alphaMode == fastgltf::AlphaMode::Mask)
                material->SetAlphaCutoff(source.alphaCutoff);

            const auto& pbr = source.pbrData;
            const auto baseColorFactor = glm::vec4(
                pbr.baseColorFactor.x(),
                pbr.baseColorFactor.y(),
                pbr.baseColorFactor.z(),
                pbr.baseColorFactor.w()
            );
            const auto emissiveFactor =glm::vec4(
                source.emissiveFactor.x(),
                source.emissiveFactor.y(),
                source.emissiveFactor.z(),
                0.0f
            ) * source.emissiveStrength;

            EE_CORE_ASSERT(material->DefineParameter("BaseColorFactor", {
                .Kind = EMaterialParameterKind::Value,
                .ValueType = EMaterialValueType::Float4,
                .DefaultValue = SMaterialParameter::MakeVector(baseColorFactor),
            }), "Could not define BaseColorFactor.")

            EE_CORE_ASSERT(material->DefineParameter("MetallicFactor", {
                .Kind = EMaterialParameterKind::Value,
                .ValueType = EMaterialValueType::Float,
                .DefaultValue = SMaterialParameter::MakeScalar(pbr.metallicFactor),
            }), "Could not define MetallicFactor.")

            EE_CORE_ASSERT(material->DefineParameter("RoughnessFactor", {
                .Kind = EMaterialParameterKind::Value,
                .ValueType = EMaterialValueType::Float,
                .DefaultValue = SMaterialParameter::MakeScalar(pbr.roughnessFactor),
            }), "Could not define RoughnessFactor.")

            EE_CORE_ASSERT(material->DefineParameter("EmissiveFactor", {
                .Kind = EMaterialParameterKind::Value,
                .ValueType = EMaterialValueType::Float3,
                .DefaultValue = SMaterialParameter::MakeVector(emissiveFactor),
            }), "Could not define EmissiveFactor.")

            EE_CORE_ASSERT(material->DefineParameter("NormalScale", {
                .Kind = EMaterialParameterKind::Value,
                .ValueType = EMaterialValueType::Float,
                .DefaultValue = SMaterialParameter::MakeScalar(
                    source.normalTexture ? source.normalTexture->scale : 1.0f),
            }), "Could not define NormalScale.")

            EE_CORE_ASSERT(material->DefineParameter("OcclusionStrength", {
                .Kind = EMaterialParameterKind::Value,
                .ValueType = EMaterialValueType::Float,
                .DefaultValue = SMaterialParameter::MakeScalar(
                    source.occlusionTexture ? source.occlusionTexture->strength : 1.0f),
            }), "Could not define OcclusionStrength.")

            if (source.specular)
            {
                EE_CORE_ASSERT(material->DefineParameter("SpecularFactor", {
                    .Kind = EMaterialParameterKind::Value,
                    .ValueType = EMaterialValueType::Float,
                    .DefaultValue = SMaterialParameter::MakeScalar(
                        source.specular->specularFactor
                    ),
                }), "Could not define SpecularFactor.")

                const auto& colorFactor = source.specular->specularColorFactor;
                EE_CORE_ASSERT(material->DefineParameter("SpecularColorFactor", {
                    .Kind = EMaterialParameterKind::Value,
                    .ValueType = EMaterialValueType::Float3,
                    .DefaultValue = SMaterialParameter::MakeVector({
                        colorFactor.x(),
                        colorFactor.y(),
                        colorFactor.z(),
                        0.0f
                    }),
                }), "Could not define SpecularColorFactor.")
            }

            if (source.clearcoat)
            {
                EE_CORE_ASSERT(material->DefineParameter("ClearCoatFactor", {
                    .Kind = EMaterialParameterKind::Value,
                    .ValueType = EMaterialValueType::Float,
                    .DefaultValue = SMaterialParameter::MakeScalar(
                        source.clearcoat->clearcoatFactor
                    ),
                }), "Could not define ClearCoatFactor.")

                EE_CORE_ASSERT(material->DefineParameter("ClearCoatRoughnessFactor", {
                    .Kind = EMaterialParameterKind::Value,
                    .ValueType = EMaterialValueType::Float,
                    .DefaultValue = SMaterialParameter::MakeScalar(
                        source.clearcoat->clearcoatRoughnessFactor
                    ),
                }), "Could not define ClearCoatRoughnessFactor.")

                EE_CORE_ASSERT(material->DefineParameter("ClearCoatNormalScale", {
                    .Kind = EMaterialParameterKind::Value,
                    .ValueType = EMaterialValueType::Float,
                    .DefaultValue = SMaterialParameter::MakeScalar(
                        source.clearcoat->clearcoatNormalTexture
                            ? source.clearcoat->clearcoatNormalTexture->scale
                            : 1.0f
                    ),
                }), "Could not define ClearCoatNormalScale.")
            }

            MaterialGraph graph;

            auto baseColor = graph.AddNode<Parameter>(
                "BaseColorFactor",
                EMaterialValueType::Float4
            );

            if (baseColorTexture)
            {
                EE_CORE_ASSERT(material->DefineParameter("BaseColorTexture", {
                    .Kind = EMaterialParameterKind::Texture,
                    .DefaultValue = SMaterialParameter::MakeTexture(baseColorTexture),
                }), "Could not define BaseColorTexture.");

                const auto texture = AddTextureSample(
                    graph,
                    *material,
                    "BaseColorTexture",
                    *pbr.baseColorTexture
                );
                const auto multiply = graph.AddNode<Multiply>();
                graph.Connect(baseColor, multiply, 0);
                graph.Connect(texture, multiply, 1);
                baseColor = multiply;
            }

            auto metallic = graph.AddNode<Parameter>(
                "MetallicFactor",
                EMaterialValueType::Float
            );

            auto roughness = graph.AddNode<Parameter>(
                "RoughnessFactor",
                EMaterialValueType::Float
            );

            if (metallicRoughnessTexture)
            {
                EE_CORE_ASSERT(material->DefineParameter("MetallicRoughnessTexture", {
                    .Kind = EMaterialParameterKind::Texture,
                    .DefaultValue = SMaterialParameter::MakeTexture(metallicRoughnessTexture),
                }), "Could not define MetallicRoughnessTexture.");

                const auto sample = AddTextureSample(
                    graph,
                    *material,
                    "MetallicRoughnessTexture",
                    *pbr.metallicRoughnessTexture,
                    ETextureSampleType::LinearColor
                );

                const auto textureRoughness = graph.AddNode<ComponentMask>(1);
                graph.Connect(sample, textureRoughness, 0);

                const auto textureMetallic = graph.AddNode<ComponentMask>(2);
                graph.Connect(sample, textureMetallic, 0);

                const auto roughnessMultiply = graph.AddNode<Multiply>();
                graph.Connect(roughness, roughnessMultiply, 0);
                graph.Connect(textureRoughness, roughnessMultiply, 1);
                roughness = roughnessMultiply;

                const auto metallicMultiply = graph.AddNode<Multiply>();
                graph.Connect(metallic, metallicMultiply, 0);
                graph.Connect(textureMetallic, metallicMultiply, 1);
                metallic = metallicMultiply;
            }

            auto emissive = graph.AddNode<Parameter>(
                "EmissiveFactor",
                EMaterialValueType::Float3
            );

            if (emissiveTexture)
            {
                EE_CORE_ASSERT(material->DefineParameter("EmissiveTexture", {
                    .Kind = EMaterialParameterKind::Texture,
                    .DefaultValue = SMaterialParameter::MakeTexture(emissiveTexture),
                }), "Could not define EmissiveTexture.");

                const auto sample = AddTextureSample(
                    graph,
                    *material,
                    "EmissiveTexture",
                    *source.emissiveTexture
                );
                const auto multiply = graph.AddNode<Multiply>();
                graph.Connect(emissive, multiply, 0);
                graph.Connect(sample, multiply, 1);
                emissive = multiply;
            }

            const auto opacity = graph.AddNode<ComponentMask>(3);
            graph.Connect(baseColor, opacity, 0);

            graph.SetChannel(EMaterialChannel::BaseColor, baseColor);
            graph.SetChannel(EMaterialChannel::Metallic, metallic);
            graph.SetChannel( EMaterialChannel::Roughness, roughness);
            graph.SetChannel( EMaterialChannel::Opacity, opacity);
            graph.SetChannel(EMaterialChannel::Emissive, emissive);

            if (normalTexture)
            {
                EE_CORE_ASSERT(material->DefineParameter("NormalTexture", {
                    .Kind = EMaterialParameterKind::Texture,
                    .DefaultValue = SMaterialParameter::MakeTexture(normalTexture),
                }), "Could not define NormalTexture.");

                const auto sample = AddTextureSample(
                    graph,
                    *material,
                    "NormalTexture",
                    *source.normalTexture,
                    ETextureSampleType::Normal
                );

                const auto scale = graph.AddNode<Parameter>(
                    "NormalScale",
                    EMaterialValueType::Float
                );

                const auto flatten = graph.AddNode<FlattenNormal>();
                graph.Connect(sample, flatten, 0);
                graph.Connect(scale, flatten, 1);

                graph.SetChannel(
                    source.clearcoat
                        ? EMaterialChannel::ClearCoatBottomNormal
                        : EMaterialChannel::Normal,
                    flatten
                );
            }

            if (occlusionTexture)
            {
                EE_CORE_ASSERT(material->DefineParameter("OcclusionTexture", {
                    .Kind = EMaterialParameterKind::Texture,
                    .DefaultValue = SMaterialParameter::MakeTexture(occlusionTexture),
                }), "Could not define OcclusionTexture.");

                const auto sample = AddTextureSample(
                    graph,
                    *material,
                    "OcclusionTexture",
                    *source.occlusionTexture,
                    ETextureSampleType::LinearColor
                );
                const auto red = graph.AddNode<ComponentMask>(0);
                graph.Connect(sample, red, 0);

                const auto strength = graph.AddNode<Parameter>(
                    "OcclusionStrength",
                    EMaterialValueType::Float
                );

                const auto one = graph.AddNode<Constant>(
                    glm::vec4{ 1.0f },
                    EMaterialValueType::Float
                );

                const auto occlusion = graph.AddNode<Lerp>();
                graph.Connect(one, occlusion, 0);
                graph.Connect(red, occlusion, 1);
                graph.Connect(strength, occlusion, 2);

                graph.SetChannel(EMaterialChannel::AmbientOcclusion, occlusion);
            }

            if (source.specular)
            {
                auto specular = graph.AddNode<Parameter>(
                    "SpecularFactor",
                    EMaterialValueType::Float
                );

                if (specularTexture)
                {
                    EE_CORE_ASSERT(material->DefineParameter("SpecularTexture", {
                        .Kind = EMaterialParameterKind::Texture,
                        .DefaultValue = SMaterialParameter::MakeTexture(specularTexture),
                    }), "Could not define SpecularTexture.");

                    const auto sample = AddTextureSample(
                        graph,
                        *material,
                        "SpecularTexture",
                        *source.specular->specularTexture,
                        ETextureSampleType::LinearColor
                    );
                    const auto alpha = graph.AddNode<ComponentMask>(3);
                    graph.Connect(sample, alpha, 0);

                    const auto multiply = graph.AddNode<Multiply>();
                    graph.Connect(specular, multiply, 0);
                    graph.Connect(alpha, multiply, 1);
                    specular = multiply;
                }

                auto specularColor = graph.AddNode<Parameter>(
                    "SpecularColorFactor",
                    EMaterialValueType::Float3
                );

                if (specularColorTexture)
                {
                    EE_CORE_ASSERT(material->DefineParameter("SpecularColorTexture", {
                        .Kind = EMaterialParameterKind::Texture,
                        .DefaultValue = SMaterialParameter::MakeTexture(specularColorTexture),
                    }), "Could not define SpecularColorTexture.");

                    const auto sample = AddTextureSample(
                        graph,
                        *material,
                        "SpecularColorTexture",
                        *source.specular->specularColorTexture
                    );

                    const auto multiply = graph.AddNode<Multiply>();
                    graph.Connect(specularColor, multiply, 0);
                    graph.Connect(sample, multiply, 1);
                    specularColor = multiply;
                }

                graph.SetChannel(EMaterialChannel::Specular, specular);
                graph.SetChannel(EMaterialChannel::SpecularColor, specularColor);
            }

            if (source.clearcoat)
            {
                auto clearCoat = graph.AddNode<Parameter>(
                    "ClearCoatFactor",
                    EMaterialValueType::Float
                );

                if (clearCoatTexture)
                {
                    EE_CORE_ASSERT(material->DefineParameter("ClearCoatTexture", {
                        .Kind = EMaterialParameterKind::Texture,
                        .DefaultValue = SMaterialParameter::MakeTexture(clearCoatTexture),
                    }), "Could not define ClearCoatTexture.");

                    const auto sample = AddTextureSample(
                        graph,
                        *material,
                        "ClearCoatTexture",
                        *source.clearcoat->clearcoatTexture,
                        ETextureSampleType::LinearColor
                    );
                    const auto red = graph.AddNode<ComponentMask>(0);
                    graph.Connect(sample, red, 0);

                    const auto multiply = graph.AddNode<Multiply>();
                    graph.Connect(clearCoat, multiply, 0);
                    graph.Connect(red, multiply, 1);
                    clearCoat = multiply;
                }

                auto clearCoatRoughness = graph.AddNode<Parameter>(
                    "ClearCoatRoughnessFactor",
                    EMaterialValueType::Float
                );

                if (clearCoatRoughnessTexture)
                {
                    EE_CORE_ASSERT(material->DefineParameter("ClearCoatRoughnessTexture", {
                        .Kind = EMaterialParameterKind::Texture,
                        .DefaultValue = SMaterialParameter::MakeTexture(
                            clearCoatRoughnessTexture
                        ),
                    }), "Could not define ClearCoatRoughnessTexture.");

                    const auto sample = AddTextureSample(
                        graph,
                        *material,
                        "ClearCoatRoughnessTexture",
                        *source.clearcoat->clearcoatRoughnessTexture,
                        ETextureSampleType::LinearColor
                    );
                    const auto green = graph.AddNode<ComponentMask>(1);
                    graph.Connect(sample, green, 0);

                    const auto multiply = graph.AddNode<Multiply>();
                    graph.Connect(clearCoatRoughness, multiply, 0);
                    graph.Connect(green, multiply, 1);
                    clearCoatRoughness = multiply;
                }

                graph.SetChannel(EMaterialChannel::ClearCoat, clearCoat);
                graph.SetChannel(EMaterialChannel::ClearCoatRoughness, clearCoatRoughness);

                // In glTF, clearCoatNormalTexture belongs to the external
                // clear-coat layer.
                if (clearCoatNormalTexture)
                {
                    EE_CORE_ASSERT(material->DefineParameter("ClearCoatNormalTexture", {
                        .Kind = EMaterialParameterKind::Texture,
                        .DefaultValue = SMaterialParameter::MakeTexture(
                            clearCoatNormalTexture
                        ),
                    }), "Could not define ClearCoatNormalTexture.");

                    const auto sample = AddTextureSample(
                        graph,
                        *material,
                        "ClearCoatNormalTexture",
                        *source.clearcoat->clearcoatNormalTexture,
                        ETextureSampleType::Normal
                    );

                    const auto scale = graph.AddNode<Parameter>(
                        "ClearCoatNormalScale",
                        EMaterialValueType::Float
                    );

                    const auto flatten = graph.AddNode<FlattenNormal>();
                    graph.Connect(sample, flatten, 0);
                    graph.Connect(scale, flatten, 1);

                    graph.SetChannel(EMaterialChannel::Normal, flatten);
                }
            }

            material->SetGraph(std::move(graph));
            return material;
        }

        void LoadMaterials(
            SStaticMeshData& mesh,
            const GraphicsContext& context,
            fastgltf::Asset& asset,
            const std::filesystem::path& dir
        )
        {
            // Index zero is the fallback for primitives without a glTF material.
            mesh.Materials.push_back(CreateDefaultMaterials()[0]);

            std::unordered_map<uint64_t, Ref<Texture>> textureCache;
            mesh.Materials.reserve(asset.materials.size() + 1);

            for (size_t i = 0; i < asset.materials.size(); ++i)
            {
                const auto& source = asset.materials[i];
                Ref<Texture> baseColorTexture;
                Ref<Texture> metallicRoughnessTexture;
                Ref<Texture> normalTexture;
                Ref<Texture> occlusionTexture;
                Ref<Texture> emissiveTexture;
                Ref<Texture> specularTexture;
                Ref<Texture> specularColorTexture;
                Ref<Texture> clearCoatTexture;
                Ref<Texture> clearCoatRoughnessTexture;
                Ref<Texture> clearCoatNormalTexture;

                if (source.pbrData.baseColorTexture)
                {
                    STexture2DCreateInfo info;
                    info.Format = EImageFormat::R8G8B8A8_SRGB;
                    baseColorTexture = LoadTexture(
                        context,
                        asset,
                        source.pbrData.baseColorTexture->textureIndex,
                        dir,
                        info,
                        textureCache
                    );
                }

                if (source.pbrData.metallicRoughnessTexture)
                {
                    STexture2DCreateInfo info;
                    info.Format = EImageFormat::R8G8B8A8_UNORM;
                    metallicRoughnessTexture = LoadTexture(
                        context,
                        asset,
                        source.pbrData.metallicRoughnessTexture->textureIndex,
                        dir,
                        info,
                        textureCache
                    );
                }

                if (source.normalTexture)
                {
                    STexture2DCreateInfo info;
                    info.Format = EImageFormat::R8G8B8A8_UNORM;
                    info.GenerateMipmaps = true;
                    normalTexture = LoadTexture(
                        context,
                        asset,
                        source.normalTexture->textureIndex,
                        dir,
                        info,
                        textureCache
                    );
                }

                if (source.occlusionTexture)
                {
                    STexture2DCreateInfo info;
                    info.Format = EImageFormat::R8G8B8A8_UNORM;
                    occlusionTexture = LoadTexture(
                        context,
                        asset,
                        source.occlusionTexture->textureIndex,
                        dir,
                        info,
                        textureCache
                    );
                }

                if (source.emissiveTexture)
                {
                    STexture2DCreateInfo info;
                    info.Format = EImageFormat::R8G8B8A8_SRGB;
                    emissiveTexture = LoadTexture(
                        context,
                        asset,
                        source.emissiveTexture->textureIndex,
                        dir,
                        info,
                        textureCache
                    );
                }

                if (source.specular)
                {
                    if (source.specular->specularTexture)
                    {
                        STexture2DCreateInfo info;
                        info.Format = EImageFormat::R8G8B8A8_UNORM;
                        specularTexture = LoadTexture(
                            context,
                            asset,
                            source.specular->specularTexture->textureIndex,
                            dir,
                            info,
                            textureCache
                        );
                    }

                    if (source.specular->specularColorTexture)
                    {
                        STexture2DCreateInfo info;
                        info.Format = EImageFormat::R8G8B8A8_SRGB;
                        specularColorTexture = LoadTexture(
                            context,
                            asset,
                            source.specular->specularColorTexture->textureIndex,
                            dir,
                            info,
                            textureCache
                        );
                    }
                }

                if (source.clearcoat)
                {
                    if (source.clearcoat->clearcoatTexture)
                    {
                        STexture2DCreateInfo info;
                        info.Format = EImageFormat::R8G8B8A8_UNORM;
                        clearCoatTexture = LoadTexture(
                            context,
                            asset,
                            source.clearcoat->clearcoatTexture->textureIndex,
                            dir,
                            info,
                            textureCache
                        );
                    }

                    if (source.clearcoat->clearcoatRoughnessTexture)
                    {
                        STexture2DCreateInfo info;
                        info.Format = EImageFormat::R8G8B8A8_UNORM;
                        clearCoatRoughnessTexture = LoadTexture(
                            context,
                            asset,
                            source.clearcoat->clearcoatRoughnessTexture->textureIndex,
                            dir,
                            info,
                            textureCache
                        );
                    }

                    if (source.clearcoat->clearcoatNormalTexture)
                    {
                        STexture2DCreateInfo info;
                        info.Format = EImageFormat::R8G8B8A8_UNORM;
                        info.GenerateMipmaps = true;
                        clearCoatNormalTexture = LoadTexture(
                            context,
                            asset,
                            source.clearcoat->clearcoatNormalTexture->textureIndex,
                            dir,
                            info,
                            textureCache
                        );
                    }
                }

                mesh.Materials.push_back(CreateSurfaceMaterial(
                    source,
                    i,
                    mesh.Name,
                    baseColorTexture,
                    metallicRoughnessTexture,
                    normalTexture,
                    occlusionTexture,
                    emissiveTexture,
                    specularTexture,
                    specularColorTexture,
                    clearCoatTexture,
                    clearCoatRoughnessTexture,
                    clearCoatNormalTexture
                ));
            }
        }
    }

    constexpr auto GLTF_OPTIONS = fastgltf::Options::LoadExternalBuffers |
                                  fastgltf::Options::GenerateMeshIndices;

    constexpr auto GLTF_CATEGORIES = fastgltf::Category::Meshes |
                                             fastgltf::Category::Accessors |
                                             fastgltf::Category::BufferViews |
                                             fastgltf::Category::Buffers |
                                             fastgltf::Category::Images |
                                             fastgltf::Category::Textures |
                                             fastgltf::Category::Materials;

    static void LoadPrimitive(
        SStaticMeshData& mesh,
        const fastgltf::Asset& asset,
        const fastgltf::Primitive& primitive,
        bool& hasBounds
    )
    {
        if (primitive.type != fastgltf::PrimitiveType::Triangles)
        {
            EE_CORE_WARN(
                "Skipping primitive in mesh '{}': only triangles are supported.",
                mesh.Name
            )
            return;
        }

        const auto position = primitive.findAttribute("POSITION");

        if (position == primitive.attributes.end() ||
            position->accessorIndex >= asset.accessors.size())
        {
            EE_CORE_WARN("Skipping primitive in mesh '{}': POSITION is missing.", mesh.Name)
            return;
        }

        const auto& positionAccessor = asset.accessors[position->accessorIndex];
        if (positionAccessor.count > std::numeric_limits<uint32_t>::max())
        {
            EE_CORE_WARN(
                "Skipping primitive in mesh '{}': it exceeds the vertex limit.",
                mesh.Name
            )
            return;
        }

        std::vector<SStaticMeshVertex> vertices(positionAccessor.count);
        fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
            asset,
            positionAccessor,
            [&vertices](const auto value, const size_t index)
            {
                vertices[index].Position = { value.x(), value.y(), value.z() };
            }
        );

        const auto loadAttribute = [&]<typename TValue>(
            const std::string_view name,
            auto&& assign
        )
        {
            const auto attribute = primitive.findAttribute(name);

            if (attribute == primitive.attributes.end() ||
                attribute->accessorIndex >= asset.accessors.size())
            {
                return;
            }

            const auto& accessor = asset.accessors[attribute->accessorIndex];
            if (accessor.count != vertices.size())
            {
                EE_CORE_WARN(
                    "Ignoring {} in primitive of mesh '{}': its count differs from POSITION.",
                    name,
                    mesh.Name
                )
                return;
            }

            fastgltf::iterateAccessorWithIndex<TValue>(
                asset,
                accessor,
                [&assign](const auto value, const size_t index)
                {
                    assign(value, index);
                }
            );
        };

        loadAttribute.operator()<fastgltf::math::fvec3>(
            "NORMAL",
            [&vertices](const auto value, const size_t index)
            {
                vertices[index].Normal = { value.x(), value.y(), value.z() };
            }
        );

        loadAttribute.operator()<fastgltf::math::fvec4>(
            "TANGENT",
            [&vertices](const auto value, const size_t index)
            {
                vertices[index].Tangent = { value.x(), value.y(), value.z(), value.w() };
            }
        );

        loadAttribute.operator()<fastgltf::math::fvec2>(
            "TEXCOORD_0",
            [&vertices](const auto value, const size_t index)
            {
                vertices[index].TexCoord = { value.x(), value.y() };
            }
        );

        loadAttribute.operator()<fastgltf::math::fvec2>(
            "TEXCOORD_1",
            [&vertices](const auto value, const size_t index)
            {
                vertices[index].TexCoord1 = { value.x(), value.y() };
            }
        );

        std::vector<uint32_t> indices;

        if (primitive.indicesAccessor &&
            *primitive.indicesAccessor < asset.accessors.size())
        {
            const auto& accessor = asset.accessors[*primitive.indicesAccessor];

            if (accessor.count > std::numeric_limits<uint32_t>::max())
            {
                EE_CORE_WARN(
                    "Skipping primitive in mesh '{}': it exceeds the index limit.",
                    mesh.Name
                )
                return;
            }

            indices.resize(accessor.count);
            fastgltf::iterateAccessorWithIndex<uint32_t>(
                asset,
                accessor,
                [&indices](const uint32_t value, const size_t index)
                {
                    indices[index] = value;
                }
            );
        }
        else
        {
            indices.resize(vertices.size());
            std::iota(indices.begin(), indices.end(), 0u);
        }

        if (vertices.empty() || indices.empty())
        {
            EE_CORE_WARN("Skipping empty primitive in mesh '{}'.", mesh.Name)
            return;
        }

        SStaticMeshSection section;
        section.FirstIndex = (uint32_t)mesh.Indices.size();
        section.IndexCount = (uint32_t)indices.size();
        section.VertexOffset = (uint32_t)mesh.Vertices.size();
        section.MaterialIndex =
            primitive.materialIndex && *primitive.materialIndex < asset.materials.size()
                ? uint32_t(*primitive.materialIndex + 1)
                : 0;
        section.LocalBounds = GetBounds(vertices);

        mesh.Vertices.insert(
            mesh.Vertices.end(),
            std::make_move_iterator(vertices.begin()),
            std::make_move_iterator(vertices.end())
        );

        mesh.Indices.insert(
            mesh.Indices.end(),
            std::make_move_iterator(indices.begin()),
            std::make_move_iterator(indices.end())
        );

        if (!hasBounds)
        {
            mesh.LocalBounds = section.LocalBounds;
            hasBounds = true;
        }
        else
        {
            ExpandBounds(mesh.LocalBounds, section.LocalBounds);
        }

        mesh.Sections.push_back(std::move(section));
    }

    std::optional<SStaticMeshData> GLTFStaticMeshLoader::Load(
        const GraphicsContext& context,
        std::filesystem::path path
    ) const
    {
        SStaticMeshData mesh;
        mesh.Name = path.stem().string();

        auto data = fastgltf::GltfDataBuffer::FromPath(path);
        if (data.error() != fastgltf::Error::None)
        {
            EE_CORE_ERROR(
                "Failed to open glTF file '{}': {}.",
                path.string(),
                fastgltf::getErrorMessage(data.error())
            )
            return std::nullopt;
        }

        fastgltf::Parser parser{
            fastgltf::Extensions::KHR_materials_emissive_strength |
            fastgltf::Extensions::KHR_materials_specular |
            fastgltf::Extensions::KHR_texture_transform |
            fastgltf::Extensions::KHR_materials_clearcoat
        };

        auto loadResult = parser.loadGltf(
            data.get(),
            path.parent_path(),
            GLTF_OPTIONS,
            GLTF_CATEGORIES
        );

        if (loadResult.error() != fastgltf::Error::None)
        {
            EE_CORE_ERROR(
                "Failed to parse glTF file '{}': {}.",
                path.string(),
                fastgltf::getErrorMessage(loadResult.error())
            )
            return std::nullopt;
        }

        fastgltf::Asset& asset = loadResult.get();
        bool hasBounds = false;

        LoadMaterials(mesh, context, asset, path.parent_path());

        // Combine every glTF mesh into this single static mesh.
        for (const auto& assetMesh : asset.meshes)
            for (const auto& primitive : assetMesh.primitives)
                LoadPrimitive(mesh, asset, primitive, hasBounds);

        if (mesh.Sections.empty())
        {
            EE_CORE_ERROR("No valid static meshes were found in '{}'.", path.string())
            return std::nullopt;
        }

        return mesh;
    }
}
