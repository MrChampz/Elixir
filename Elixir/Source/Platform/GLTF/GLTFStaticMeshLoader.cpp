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
#include <glm/gtc/matrix_inverse.hpp>
#include <stb_image.h>

namespace Elixir
{
    namespace
    {
        constexpr auto GLTF_OPTIONS = fastgltf::Options::LoadExternalBuffers |
                                      fastgltf::Options::GenerateMeshIndices;

        constexpr auto GLTF_CATEGORIES = fastgltf::Category::Meshes |
                                         fastgltf::Category::Accessors |
                                         fastgltf::Category::BufferViews |
                                         fastgltf::Category::Buffers |
                                         fastgltf::Category::Images |
                                         fastgltf::Category::Textures |
                                         fastgltf::Category::Materials |
                                         fastgltf::Category::Nodes |
                                         fastgltf::Category::Scenes;

        constexpr auto GLTF_EXTENSIONS =
            fastgltf::Extensions::KHR_materials_emissive_strength |
            fastgltf::Extensions::KHR_materials_specular |
            fastgltf::Extensions::KHR_texture_transform |
            fastgltf::Extensions::KHR_materials_clearcoat;

        class MaterialLoader final
        {
        public:
            MaterialLoader(
                const GraphicsContext& context,
                fastgltf::Asset& asset,
                std::filesystem::path directory,
                std::string_view meshName
            );

            std::vector<Ref<Material>> Load();

        private:
            struct SMaterialTextures
            {
                Ref<Texture> BaseColor;
                Ref<Texture> MetallicRoughness;
                Ref<Texture> Normal;
                Ref<Texture> Occlusion;
                Ref<Texture> Emissive;
                Ref<Texture> Specular;
                Ref<Texture> SpecularColor;
                Ref<Texture> ClearCoat;
                Ref<Texture> ClearCoatRoughness;
                Ref<Texture> ClearCoatNormal;
            };

            Ref<Texture> DecodeImage(
                const std::byte* bytes,
                size_t size,
                const STexture2DCreateInfo& info
            ) const;

            Ref<Texture> LoadImage(
                size_t imageIndex,
                const STexture2DCreateInfo& info
            ) const;

            Ref<Texture> LoadTexture(
                size_t textureIndex,
                const STexture2DCreateInfo& info
            );

            Ref<Material> CreateSurfaceMaterial(
                const fastgltf::Material& source,
                size_t materialIndex,
                const SMaterialTextures& textures
            ) const;

            static EMaterialBlendMode GetBlendMode(fastgltf::AlphaMode mode);
            static uint32_t GetTextureCoordinateIndex(const fastgltf::TextureInfo& source);
            static uint32_t AddTextureSample(
                MaterialGraph& graph,
                Material& material,
                std::string_view parameterName,
                const fastgltf::TextureInfo& source,
                Nodes::ETextureSampleType sampleType = Nodes::ETextureSampleType::Color
            );

            const GraphicsContext& m_Context;
            fastgltf::Asset& m_Asset;
            std::filesystem::path m_Directory;
            std::string_view m_MeshName;
            std::unordered_map<uint64_t, Ref<Texture>> m_TextureCache;
        };

        MaterialLoader::MaterialLoader(
            const GraphicsContext& context,
            fastgltf::Asset& asset,
            std::filesystem::path directory,
            const std::string_view meshName
        ) : m_Context(context),
            m_Asset(asset),
            m_Directory(std::move(directory)),
            m_MeshName(meshName) {}

        Ref<Texture> MaterialLoader::DecodeImage(
            const std::byte* bytes,
            const size_t size,
            const STexture2DCreateInfo& info
        ) const
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

            const auto texture = Texture2D::Create(&m_Context, createInfo);

            stbi_image_free(pixels);

            return texture;
        }

        Ref<Texture> MaterialLoader::LoadImage(
            const size_t imageIndex,
            const STexture2DCreateInfo& info
        ) const
        {
            if (imageIndex >= m_Asset.images.size())
            {
                EE_CORE_WARN("glTF material references an invalid image index.")
                return nullptr;
            }

            Ref<Texture> texture;
            auto& image = m_Asset.images[imageIndex];

            std::visit(
                fastgltf::visitor{
                    [](const auto&) {},
                    [&](const fastgltf::sources::URI& source)
                    {
                        const auto path = m_Directory / source.uri.fspath();
                        texture = TextureLoader::Load(path, info);
                    },
                    [&](const fastgltf::sources::Array& source)
                    {
                        texture = DecodeImage(
                            source.bytes.data(),
                            source.bytes.size(),
                            info
                        );
                    },
                    [&](const fastgltf::sources::Vector& source)
                    {
                        texture = DecodeImage(
                            source.bytes.data(),
                            source.bytes.size(),
                            info
                        );
                    },
                    [&](const fastgltf::sources::BufferView& viewSource)
                    {
                        if (viewSource.bufferViewIndex >= m_Asset.bufferViews.size())
                        {
                            EE_CORE_WARN("glTF image references an invalid buffer view.")
                            return;
                        }

                        const auto& view = m_Asset.bufferViews[viewSource.bufferViewIndex];
                        if (view.bufferIndex >= m_Asset.buffers.size())
                        {
                            EE_CORE_WARN("glTF image references an invalid buffer.")
                            return;
                        }

                        auto& buffer = m_Asset.buffers[view.bufferIndex];
                        std::visit(
                            fastgltf::visitor{
                                [](const auto&) {},
                                [&](const fastgltf::sources::Array& source)
                                {
                                    texture = DecodeImage(
                                        source.bytes.data() + view.byteOffset,
                                        view.byteLength,
                                        info
                                    );
                                },
                                [&](const fastgltf::sources::Vector& source)
                                {
                                    texture = DecodeImage(
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

        Ref<Texture> MaterialLoader::LoadTexture(
            const size_t textureIndex,
            const STexture2DCreateInfo& info
        )
        {
            const uint64_t cacheKey =
                (uint64_t(textureIndex) << 3) |
                (info.Format == EImageFormat::R8G8B8A8_SRGB ? 1ull : 0ull) |
                (static_cast<uint64_t>(info.MipmapMode) << 1);

            const auto existing = m_TextureCache.find(cacheKey);
            if (existing != m_TextureCache.end())
            {
                return existing->second;
            }

            if (textureIndex >= m_Asset.textures.size())
            {
                EE_CORE_WARN("glTF material references an invalid texture index.")
                return nullptr;
            }

            const auto& texture = m_Asset.textures[textureIndex];
            if (!texture.imageIndex)
            {
                EE_CORE_WARN("glTF texture does not reference an image.")
                return nullptr;
            }

            const auto result = LoadImage(
                *texture.imageIndex,
                info
            );
            m_TextureCache.emplace(cacheKey, result);

            return result;
        }

        EMaterialBlendMode MaterialLoader::GetBlendMode(const fastgltf::AlphaMode mode)
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

        uint32_t MaterialLoader::GetTextureCoordinateIndex(const fastgltf::TextureInfo& source)
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

        uint32_t MaterialLoader::AddTextureSample(
            MaterialGraph& graph,
            Material& material,
            const std::string_view parameterName,
            const fastgltf::TextureInfo& source,
            const Nodes::ETextureSampleType sampleType
        )
        {
            using namespace Materials;
            using namespace Nodes;

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

        Ref<Material> MaterialLoader::CreateSurfaceMaterial(
            const fastgltf::Material& source,
            const size_t materialIndex,
            const SMaterialTextures& textures
        ) const
        {
            using namespace Materials;
            using namespace Nodes;

            const std::string name = source.name.empty()
                ? std::string(m_MeshName) + ".Material." + std::to_string(materialIndex)
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

            if (textures.BaseColor)
            {
                EE_CORE_ASSERT(material->DefineParameter("BaseColorTexture", {
                    .Kind = EMaterialParameterKind::Texture,
                    .DefaultValue = SMaterialParameter::MakeTexture(textures.BaseColor),
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

            if (textures.MetallicRoughness)
            {
                EE_CORE_ASSERT(material->DefineParameter("MetallicRoughnessTexture", {
                    .Kind = EMaterialParameterKind::Texture,
                    .DefaultValue = SMaterialParameter::MakeTexture(textures.MetallicRoughness),
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

            if (textures.Emissive)
            {
                EE_CORE_ASSERT(material->DefineParameter("EmissiveTexture", {
                    .Kind = EMaterialParameterKind::Texture,
                    .DefaultValue = SMaterialParameter::MakeTexture(textures.Emissive),
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

            if (textures.Normal)
            {
                EE_CORE_ASSERT(material->DefineParameter("NormalTexture", {
                    .Kind = EMaterialParameterKind::Texture,
                    .DefaultValue = SMaterialParameter::MakeTexture(textures.Normal),
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

            if (textures.Occlusion)
            {
                EE_CORE_ASSERT(material->DefineParameter("OcclusionTexture", {
                    .Kind = EMaterialParameterKind::Texture,
                    .DefaultValue = SMaterialParameter::MakeTexture(textures.Occlusion),
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

                if (textures.Specular)
                {
                    EE_CORE_ASSERT(material->DefineParameter("SpecularTexture", {
                        .Kind = EMaterialParameterKind::Texture,
                        .DefaultValue = SMaterialParameter::MakeTexture(textures.Specular),
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

                if (textures.SpecularColor)
                {
                    EE_CORE_ASSERT(material->DefineParameter("SpecularColorTexture", {
                        .Kind = EMaterialParameterKind::Texture,
                        .DefaultValue = SMaterialParameter::MakeTexture(textures.SpecularColor),
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

                if (textures.ClearCoat)
                {
                    EE_CORE_ASSERT(material->DefineParameter("ClearCoatTexture", {
                        .Kind = EMaterialParameterKind::Texture,
                        .DefaultValue = SMaterialParameter::MakeTexture(textures.ClearCoat),
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

                if (textures.ClearCoatRoughness)
                {
                    EE_CORE_ASSERT(material->DefineParameter("ClearCoatRoughnessTexture", {
                        .Kind = EMaterialParameterKind::Texture,
                        .DefaultValue = SMaterialParameter::MakeTexture(
                            textures.ClearCoatRoughness
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
                if (textures.ClearCoatNormal)
                {
                    EE_CORE_ASSERT(material->DefineParameter("ClearCoatNormalTexture", {
                        .Kind = EMaterialParameterKind::Texture,
                        .DefaultValue = SMaterialParameter::MakeTexture(
                            textures.ClearCoatNormal
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

        std::vector<Ref<Material>> MaterialLoader::Load()
        {
            // Index zero is the fallback for primitives without a glTF material.
            std::vector<Ref<Material>> materials;
            materials.push_back(CreateDefaultMaterials()[0]);

            materials.reserve(m_Asset.materials.size() + 1);

            for (size_t i = 0; i < m_Asset.materials.size(); ++i)
            {
                const auto& source = m_Asset.materials[i];
                SMaterialTextures textures;

                if (source.pbrData.baseColorTexture)
                {
                    STexture2DCreateInfo info;
                    info.Format = EImageFormat::R8G8B8A8_SRGB;
                    textures.BaseColor = LoadTexture(
                        source.pbrData.baseColorTexture->textureIndex,
                        info
                    );
                }

                if (source.pbrData.metallicRoughnessTexture)
                {
                    STexture2DCreateInfo info;
                    info.Format = EImageFormat::R8G8B8A8_UNORM;
                    textures.MetallicRoughness = LoadTexture(
                        source.pbrData.metallicRoughnessTexture->textureIndex,
                        info
                    );
                }

                if (source.normalTexture)
                {
                    STexture2DCreateInfo info;
                    info.Format = EImageFormat::R8G8B8A8_UNORM;
                    info.MipmapMode = EImageMipmapMode::NormalMap;
                    textures.Normal = LoadTexture(
                        source.normalTexture->textureIndex,
                        info
                    );
                }

                if (source.occlusionTexture)
                {
                    STexture2DCreateInfo info;
                    info.Format = EImageFormat::R8G8B8A8_UNORM;
                    textures.Occlusion = LoadTexture(
                        source.occlusionTexture->textureIndex,
                        info
                    );
                }

                if (source.emissiveTexture)
                {
                    STexture2DCreateInfo info;
                    info.Format = EImageFormat::R8G8B8A8_SRGB;
                    textures.Emissive = LoadTexture(
                        source.emissiveTexture->textureIndex,
                        info
                    );
                }

                if (source.specular)
                {
                    if (source.specular->specularTexture)
                    {
                        STexture2DCreateInfo info;
                        info.Format = EImageFormat::R8G8B8A8_UNORM;
                        textures.Specular = LoadTexture(
                            source.specular->specularTexture->textureIndex,
                            info
                        );
                    }

                    if (source.specular->specularColorTexture)
                    {
                        STexture2DCreateInfo info;
                        info.Format = EImageFormat::R8G8B8A8_SRGB;
                        textures.SpecularColor = LoadTexture(
                            source.specular->specularColorTexture->textureIndex,
                            info
                        );
                    }
                }

                if (source.clearcoat)
                {
                    if (source.clearcoat->clearcoatTexture)
                    {
                        STexture2DCreateInfo info;
                        info.Format = EImageFormat::R8G8B8A8_UNORM;
                        textures.ClearCoat = LoadTexture(
                            source.clearcoat->clearcoatTexture->textureIndex,
                            info
                        );
                    }

                    if (source.clearcoat->clearcoatRoughnessTexture)
                    {
                        STexture2DCreateInfo info;
                        info.Format = EImageFormat::R8G8B8A8_UNORM;
                        textures.ClearCoatRoughness = LoadTexture(
                            source.clearcoat->clearcoatRoughnessTexture->textureIndex,
                            info
                        );
                    }

                    if (source.clearcoat->clearcoatNormalTexture)
                    {
                        STexture2DCreateInfo info;
                        info.Format = EImageFormat::R8G8B8A8_UNORM;
                        info.MipmapMode = EImageMipmapMode::NormalMap;
                        textures.ClearCoatNormal = LoadTexture(
                            source.clearcoat->clearcoatNormalTexture->textureIndex,
                            info
                        );
                    }
                }

                materials.push_back(CreateSurfaceMaterial(source, i, textures));
            }

            return materials;
        }

        class Loader final
        {
        public:
            Loader(const GraphicsContext& context, std::filesystem::path path);

            std::optional<SStaticMeshData> Load();

        private:
            bool ParseAsset();
            bool LoadGeometry();
            void LoadPrimitive(
                const fastgltf::Primitive& primitive,
                const glm::mat4& localTransform
            );
            void LoadNodeMesh(
                const fastgltf::Node& node,
                const glm::mat4& localTransform
            );

            static SStaticMeshBounds GetBounds(
                const std::vector<SStaticMeshVertex>& vertices
            );
            static void ExpandBounds(
                SStaticMeshBounds& target,
                const SStaticMeshBounds& source
            );
            static glm::mat4 ToGlmMatrix(const fastgltf::math::fmat4x4& source);
            static bool ApplyLocalTransform(
                std::vector<SStaticMeshVertex>& vertices,
                const glm::mat4& localTransform
            );

            const GraphicsContext& m_Context;
            std::filesystem::path m_Path;
            std::optional<fastgltf::Asset> m_Asset;
            SStaticMeshData m_Mesh;
            bool m_HasBounds = false;
        };

        SStaticMeshBounds Loader::GetBounds(const std::vector<SStaticMeshVertex>& vertices)
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

        void Loader::ExpandBounds(
            SStaticMeshBounds& target,
            const SStaticMeshBounds& source
        )
        {
            target.Min = glm::min(target.Min, source.Min);
            target.Max = glm::max(target.Max, source.Max);
        }

        glm::mat4 Loader::ToGlmMatrix(const fastgltf::math::fmat4x4& source)
        {
            glm::mat4 result{ 1.0f };
            for (size_t column = 0; column < 4; ++column)
                for (size_t row = 0; row < 4; ++row)
                    result[column][row] = source.col(column)[row];

            return result;
        }

        bool Loader::ApplyLocalTransform(
            std::vector<SStaticMeshVertex>& vertices,
            const glm::mat4& localTransform
        )
        {
            const glm::mat3 linearTransform{ localTransform };
            const float determinant = glm::determinant(linearTransform);
            if (determinant == 0.0f)
            {
                EE_CORE_WARN("glTF node has a singular local transform; skipping its primitive.")
                return false;
            }

            const glm::mat3 normalTransform = glm::transpose(glm::inverse(linearTransform));
            const float handedness = determinant < 0.0f ? -1.0f : 1.0f;

            for (auto& vertex : vertices)
            {
                vertex.Position = glm::vec3(localTransform * glm::vec4(vertex.Position, 1.0f));
                vertex.Normal = glm::normalize(normalTransform * vertex.Normal);

                glm::vec3 tangent = linearTransform * glm::vec3(vertex.Tangent);
                tangent -= vertex.Normal * glm::dot(vertex.Normal, tangent);
                vertex.Tangent = {
                    glm::normalize(tangent),
                    vertex.Tangent.w * handedness
                };
            }

            return true;
        }

        void Loader::LoadPrimitive(
            const fastgltf::Primitive& primitive,
            const glm::mat4& localTransform
        )
        {
            const auto& asset = *m_Asset;
            auto& mesh = m_Mesh;

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

            if (!ApplyLocalTransform(vertices, localTransform))
                return;

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

            if (!m_HasBounds)
            {
                mesh.LocalBounds = section.LocalBounds;
                m_HasBounds = true;
            }
            else
            {
                ExpandBounds(mesh.LocalBounds, section.LocalBounds);
            }

            mesh.Sections.push_back(std::move(section));
        }

        void Loader::LoadNodeMesh(
            const fastgltf::Node& node,
            const glm::mat4& localTransform
        )
        {
            const auto& asset = *m_Asset;

            if (!node.meshIndex)
                return;

            if (*node.meshIndex >= asset.meshes.size())
            {
                EE_CORE_WARN("glTF node references an invalid mesh index.")
                return;
            }

            for (const auto& primitive : asset.meshes[*node.meshIndex].primitives)
                LoadPrimitive(primitive, localTransform);
        }

        Loader::Loader(const GraphicsContext& context, std::filesystem::path path)
          : m_Context(context),
            m_Path(std::move(path)) {}

        bool Loader::ParseAsset()
        {
            auto data = fastgltf::GltfDataBuffer::FromPath(m_Path);
            if (data.error() != fastgltf::Error::None)
            {
                EE_CORE_ERROR(
                    "Failed to open glTF file '{}': {}.",
                    m_Path.string(),
                    fastgltf::getErrorMessage(data.error())
                )
                return false;
            }

            fastgltf::Parser parser{ GLTF_EXTENSIONS };

            auto loadResult = parser.loadGltf(
                data.get(),
                m_Path.parent_path(),
                GLTF_OPTIONS,
                GLTF_CATEGORIES
            );

            if (loadResult.error() != fastgltf::Error::None)
            {
                EE_CORE_ERROR(
                    "Failed to parse glTF file '{}': {}.",
                    m_Path.string(),
                    fastgltf::getErrorMessage(loadResult.error())
                )
                return false;
            }

            m_Asset.emplace(std::move(loadResult.get()));
            return true;
        }

        bool Loader::LoadGeometry()
        {
            const auto& asset = *m_Asset;

            if (!asset.scenes.empty())
            {
                const size_t sceneIndex = asset.defaultScene.value_or(0);
                if (sceneIndex >= asset.scenes.size())
                {
                    EE_CORE_ERROR("glTF default scene index is invalid.")
                    return false;
                }

                fastgltf::iterateSceneNodes(
                    asset,
                    sceneIndex,
                    fastgltf::math::fmat4x4{},
                    [&](const fastgltf::Node& node, const fastgltf::math::fmat4x4& transform)
                    {
                        LoadNodeMesh(node, ToGlmMatrix(transform));
                    }
                );
            }

            else
            {
                if (asset.nodes.empty())
                {
                    for (const auto& assetMesh : asset.meshes)
                        for (const auto& primitive : assetMesh.primitives)
                            LoadPrimitive(primitive, glm::mat4{ 1.0f });
                }
                else
                {
                    for (const auto& node : asset.nodes)
                        LoadNodeMesh(
                            node,
                            ToGlmMatrix(fastgltf::getTransformMatrix(node))
                        );
                }
            }

            return true;
        }

        std::optional<SStaticMeshData> Loader::Load()
        {
            m_Mesh.Name = m_Path.stem().string();

            if (!ParseAsset())
                return std::nullopt;

            m_Mesh.Materials = MaterialLoader(
                m_Context,
                *m_Asset,
                m_Path.parent_path(),
                m_Mesh.Name
            ).Load();

            if (!LoadGeometry())
                return std::nullopt;

            if (m_Mesh.Sections.empty())
            {
                EE_CORE_ERROR("No valid static meshes were found in '{}'.", m_Path.string())
                return std::nullopt;
            }

            return std::move(m_Mesh);
        }
    }

    std::optional<SStaticMeshData> GLTFStaticMeshLoader::Load(
        const GraphicsContext& context,
        std::filesystem::path path
    ) const
    {
        return Loader(context, std::move(path)).Load();
    }
}
