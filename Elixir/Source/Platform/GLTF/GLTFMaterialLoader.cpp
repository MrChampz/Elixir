#include "epch.h"
#include "GLTFMaterialLoader.h"

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
#include <Engine/Materials/Nodes/Lerp.h>
#include <Engine/Materials/Nodes/Multiply.h>
#include <Engine/Materials/Nodes/Parameter.h>
#include <Engine/Materials/Nodes/ScaleNormal.h>
#include <Engine/Materials/Nodes/Sine.h>
#include <Engine/Materials/Nodes/Subtract.h>
#include <Engine/Materials/Nodes/TexCoord.h>
#include <Engine/Materials/Nodes/TextureSample.h>

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <stb_image.h>

namespace Elixir
{
    using namespace Materials;

    namespace
    {
        // Reject texture paths that escape the asset directory.
        bool IsPathWithinAssetDirectory(const std::filesystem::path& path)
        {
            if (path.is_absolute())
                return false;

            for (const auto& component : path.lexically_normal())
            {
                if (component == "..")
                    return false;
            }

            return true;
        }

        class MaterialLoader final
        {
        public:
            /** @brief Prepare material import for one source asset. */
            MaterialLoader(
                const GraphicsContext& context,
                fastgltf::Asset& asset,
                std::filesystem::path directory,
                std::string_view meshName
            );

            /** @brief Import source materials after the default Surface material. */
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
                Ref<Texture> SheenColor;
                Ref<Texture> SheenRoughness;
                Ref<Texture> Transmission;
                Ref<Texture> Thickness;
            };

            // Decode image bytes into a texture.
            Ref<Texture> DecodeImage(
                const std::byte* bytes,
                size_t size,
                const STexture2DCreateInfo& info
            ) const;

            // Resolve an image source and decode its pixels.
            Ref<Texture> LoadImage(
                size_t imageIndex,
                const STexture2DCreateInfo& info
            ) const;

            // Load and cache one texture for its requested format.
            Ref<Texture> LoadTexture(
                size_t textureIndex,
                const STexture2DCreateInfo& info
            );

            // Build a Surface graph from glTF material properties.
            Ref<Material> CreateSurfaceMaterial(
                const fastgltf::Material& source,
                size_t materialIndex,
                const SMaterialTextures& textures
            ) const;

            // Map a glTF alpha mode to an engine blend mode.
            static EMaterialBlendMode GetBlendMode(fastgltf::AlphaMode mode);

            // Resolve the UV channel selected by the texture transform.
            static uint32_t GetTextureCoordinateIndex(const fastgltf::TextureInfo& source);

            // Add a texture sample and its UV transform to the graph.
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
                        const auto relativePath = source.uri.fspath();
                        if (!IsPathWithinAssetDirectory(relativePath))
                        {
                            EE_CORE_WARN("glTF image URI escapes the asset directory.")
                            return;
                        }

                        const auto path = m_Directory / relativePath;
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

            const auto material = CreateRef<Material>(name, EMaterialUsage::Surface);
            material->SetBlendMode(GetBlendMode(source.alphaMode));
            material->SetDoubleSided(source.doubleSided);

            // Cloth and ClearCoat are separate material types; sheen takes precedence.
            if (source.transmission && source.transmission->transmissionFactor > 0)
            {
                material->SetShadingModel(EMaterialShadingModel::Transmission);

                // Volume boundaries ignore doubleSided; render their entry surface once.
                if (source.volume && source.volume->thicknessFactor > 0)
                    material->SetDoubleSided(false);
            }
            else if (source.sheen)
                material->SetShadingModel(EMaterialShadingModel::Cloth);
            else if (source.clearcoat)
                material->SetShadingModel(EMaterialShadingModel::ClearCoat);

            if (source.alphaMode == fastgltf::AlphaMode::Mask)
                material->SetAlphaCutoff(source.alphaCutoff);

            if (!material->DefineParameter("IOR", {
                .Kind = EMaterialParameterKind::Value,
                .ValueType = EMaterialValueType::Float,
                .DefaultValue = SMaterialParameter::MakeScalar(source.ior),
            }))
            {
                EE_CORE_ERROR("Could not define glTF index of refraction.")
                return nullptr;
            }

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

            if (source.clearcoat && !source.sheen)
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

            if (source.sheen)
            {
                const auto& colorFactor = source.sheen->sheenColorFactor;
                EE_CORE_ASSERT(material->DefineParameter("FuzzColorFactor", {
                    .Kind = EMaterialParameterKind::Value,
                    .ValueType = EMaterialValueType::Float3,
                    .DefaultValue = SMaterialParameter::MakeVector({
                        colorFactor.x(), colorFactor.y(), colorFactor.z(), 0.0f
                    }),
                }), "Could not define FuzzColorFactor.")

                EE_CORE_ASSERT(material->DefineParameter("FuzzRoughnessFactor", {
                    .Kind = EMaterialParameterKind::Value,
                    .ValueType = EMaterialValueType::Float,
                    .DefaultValue = SMaterialParameter::MakeScalar(
                        source.sheen->sheenRoughnessFactor
                    ),
                }), "Could not define FuzzRoughnessFactor.")
            }

            MaterialGraph graph;

            graph.SetChannel(
                EMaterialChannel::IOR,
                graph.AddNode<Parameter>("IOR", EMaterialValueType::Float)
            );

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

                const auto scaledNormal = graph.AddNode<ScaleNormal>();
                graph.Connect(sample, scaledNormal, 0);
                graph.Connect(scale, scaledNormal, 1);

                graph.SetChannel(
                    material->GetShadingModel() == EMaterialShadingModel::ClearCoat
                        ? EMaterialChannel::ClearCoatBottomNormal
                        : EMaterialChannel::Normal,
                    scaledNormal
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

            if (material->GetShadingModel() == EMaterialShadingModel::Transmission)
            {
                if (!material->DefineParameter("TransmissionFactor", {
                    .Kind = EMaterialParameterKind::Value,
                    .ValueType = EMaterialValueType::Float,
                    .DefaultValue = SMaterialParameter::MakeScalar(
                        source.transmission->transmissionFactor
                    ),
                }))
                {
                    EE_CORE_ERROR("Could not define glTF transmission parameters.")
                    return nullptr;
                }

                auto transmission = graph.AddNode<Parameter>(
                    "TransmissionFactor",
                    EMaterialValueType::Float
                );

                if (textures.Transmission)
                {
                    if (!material->DefineParameter("TransmissionTexture", {
                        .Kind = EMaterialParameterKind::Texture,
                        .DefaultValue = SMaterialParameter::MakeTexture(textures.Transmission),
                    }))
                    {
                        EE_CORE_ERROR("Could not define glTF transmission texture.")
                        return nullptr;
                    }

                    const auto sample = AddTextureSample(
                        graph,
                        *material,
                        "TransmissionTexture",
                        *source.transmission->transmissionTexture,
                        ETextureSampleType::Mask
                    );

                    const auto red = graph.AddNode<ComponentMask>(0);
                    graph.Connect(sample, red, 0);

                    const auto multiply = graph.AddNode<Multiply>();
                    graph.Connect(transmission, multiply, 0);
                    graph.Connect(red, multiply, 1);

                    transmission = multiply;
                }

                graph.SetChannel(EMaterialChannel::Transmission, transmission);

                if (source.volume)
                {
                    const auto& volume = *source.volume;

                    auto thicknessResult = material->DefineParameter("ThicknessFactor", {
                        .Kind = EMaterialParameterKind::Value,
                        .ValueType = EMaterialValueType::Float,
                        .DefaultValue = SMaterialParameter::MakeScalar(volume.thicknessFactor),
                    });

                    const auto attDistResult = material->DefineParameter("AttenuationDistance", {
                        .Kind = EMaterialParameterKind::Value,
                        .ValueType = EMaterialValueType::Float,
                        .DefaultValue = SMaterialParameter::MakeScalar(volume.attenuationDistance),
                    });

                    const auto attColorResult = material->DefineParameter("AttenuationColor", {
                        .Kind = EMaterialParameterKind::Value,
                        .ValueType = EMaterialValueType::Float3,
                        .DefaultValue = SMaterialParameter::MakeVector({
                            volume.attenuationColor.x(), volume.attenuationColor.y(),
                            volume.attenuationColor.z(), 0.0f,
                        })
                    });

                    if (!thicknessResult || !attDistResult || !attColorResult)
                    {
                        EE_CORE_ERROR("Could not define glTF volume parameters.")
                        return nullptr;
                    }

                    auto thickness = graph.AddNode<Parameter>(
                        "ThicknessFactor",
                        EMaterialValueType::Float
                    );

                    if (textures.Thickness)
                    {
                        if (!material->DefineParameter("ThicknessTexture", {
                            .Kind = EMaterialParameterKind::Texture,
                            .DefaultValue = SMaterialParameter::MakeTexture(textures.Thickness),
                        }))
                        {
                            EE_CORE_ERROR("Could not define glTF thickness texture.")
                            return nullptr;
                        }

                        const auto sample = AddTextureSample(
                            graph,
                            *material,
                            "ThicknessTexture",
                            *volume.thicknessTexture,
                            ETextureSampleType::Mask
                        );

                        const auto green = graph.AddNode<ComponentMask>(1);
                        graph.Connect(sample, green, 0);

                        const auto multiply = graph.AddNode<Multiply>();
                        graph.Connect(thickness, multiply, 0);
                        graph.Connect(green, multiply, 1);

                        thickness = multiply;
                    }
                    graph.SetChannel(EMaterialChannel::Thickness, thickness);
                    graph.SetChannel(EMaterialChannel::AttenuationDistance,
                        graph.AddNode<Parameter>(
                            "AttenuationDistance",
                            EMaterialValueType::Float
                        )
                    );
                    graph.SetChannel(EMaterialChannel::AttenuationColor,
                        graph.AddNode<Parameter>(
                            "AttenuationColor",
                            EMaterialValueType::Float3
                        )
                    );
                }
            }

            if (source.clearcoat && !source.sheen)
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

                    const auto scaledNormal = graph.AddNode<ScaleNormal>();
                    graph.Connect(sample, scaledNormal, 0);
                    graph.Connect(scale, scaledNormal, 1);

                    graph.SetChannel(EMaterialChannel::Normal, scaledNormal);
                }
            }

            if (source.sheen)
            {
                auto fuzzColor = graph.AddNode<Parameter>(
                    "FuzzColorFactor",
                    EMaterialValueType::Float3
                );

                if (textures.SheenColor)
                {
                    EE_CORE_ASSERT(material->DefineParameter("FuzzColorTexture", {
                        .Kind = EMaterialParameterKind::Texture,
                        .DefaultValue = SMaterialParameter::MakeTexture(textures.SheenColor),
                    }), "Could not define FuzzColorTexture.");

                    const auto sample = AddTextureSample(
                        graph,
                        *material,
                        "FuzzColorTexture",
                        *source.sheen->sheenColorTexture
                    );
                    const auto multiply = graph.AddNode<Multiply>();
                    graph.Connect(fuzzColor, multiply, 0);
                    graph.Connect(sample, multiply, 1);
                    fuzzColor = multiply;
                }

                auto fuzzRoughness = graph.AddNode<Parameter>(
                    "FuzzRoughnessFactor",
                    EMaterialValueType::Float
                );

                if (textures.SheenRoughness)
                {
                    EE_CORE_ASSERT(material->DefineParameter("FuzzRoughnessTexture", {
                        .Kind = EMaterialParameterKind::Texture,
                        .DefaultValue = SMaterialParameter::MakeTexture(textures.SheenRoughness),
                    }), "Could not define FuzzRoughnessTexture.");

                    const auto sample = AddTextureSample(
                        graph,
                        *material,
                        "FuzzRoughnessTexture",
                        *source.sheen->sheenRoughnessTexture,
                        ETextureSampleType::LinearColor
                    );
                    const auto alpha = graph.AddNode<ComponentMask>(3);
                    graph.Connect(sample, alpha, 0);
                    const auto multiply = graph.AddNode<Multiply>();
                    graph.Connect(fuzzRoughness, multiply, 0);
                    graph.Connect(alpha, multiply, 1);
                    fuzzRoughness = multiply;
                }

                const auto cloth = graph.AddNode<Constant>(
                    glm::vec4{ 1.0f },
                    EMaterialValueType::Float
                );
                graph.SetChannel(EMaterialChannel::FuzzColor, fuzzColor);
                graph.SetChannel(EMaterialChannel::FuzzRoughness, fuzzRoughness);
                graph.SetChannel(EMaterialChannel::Cloth, cloth);
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

                if (source.transmission && source.transmission->transmissionTexture)
                {
                    STexture2DCreateInfo info;
                    info.Format = EImageFormat::R8G8B8A8_UNORM;

                    textures.Transmission = LoadTexture(
                        source.transmission->transmissionTexture->textureIndex,
                        info
                    );

                    if (!textures.Transmission)
                    {
                        EE_CORE_ERROR("Could not load glTF transmission texture.")
                        return {};
                    }
                }

                if (source.volume && source.volume->thicknessTexture)
                {
                    STexture2DCreateInfo info;
                    info.Format = EImageFormat::R8G8B8A8_UNORM;

                    textures.Thickness = LoadTexture(
                        source.volume->thicknessTexture->textureIndex,
                        info
                    );

                    if (!textures.Thickness)
                    {
                        EE_CORE_ERROR("Could not load glTF thickness texture.")
                        return {};
                    }
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

                if (source.clearcoat && !source.sheen)
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

                if (source.sheen)
                {
                    if (source.sheen->sheenColorTexture)
                    {
                        STexture2DCreateInfo info;
                        info.Format = EImageFormat::R8G8B8A8_SRGB;
                        textures.SheenColor = LoadTexture(
                            source.sheen->sheenColorTexture->textureIndex,
                            info
                        );
                    }

                    if (source.sheen->sheenRoughnessTexture)
                    {
                        STexture2DCreateInfo info;
                        info.Format = EImageFormat::R8G8B8A8_UNORM;
                        textures.SheenRoughness = LoadTexture(
                            source.sheen->sheenRoughnessTexture->textureIndex,
                            info
                        );
                    }
                }

                auto material = CreateSurfaceMaterial(source, i, textures);
                if (!material)
                {
                    EE_CORE_ERROR("Could not create glTF material {}.", i)
                    return {};
                }

                materials.push_back(std::move(material));
            }

            return materials;
        }

    }

    std::vector<Ref<Material>> LoadGLTFMaterials(
        const GraphicsContext& context,
        fastgltf::Asset& asset,
        const std::filesystem::path& directory,
        const std::string_view name
    )
    {
        return MaterialLoader(context, asset, directory, name).Load();
    }
}
