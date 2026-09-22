#include "epch.h"
#include "GLTFStaticMeshLoader.h"

#include <Engine/Graphics/Texture.h>
#include <Engine/Graphics/TextureLoader.h>
#include <Engine/Materials/DefaultMaterials.h>
#include <Engine/Materials/Material.h>
#include <Engine/Materials/MaterialParameter.h>
#include <Engine/Materials/Nodes/ComponentMask.h>
#include <Engine/Materials/Nodes/Constant.h>
#include <Engine/Materials/Nodes/FlattenNormal.h>
#include <Engine/Materials/Nodes/Lerp.h>
#include <Engine/Materials/Nodes/Multiply.h>
#include <Engine/Materials/Nodes/Parameter.h>
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

        Ref<Texture> DecodeImage(
            const GraphicsContext& context,
            const std::byte* bytes,
            const size_t size,
            const EImageFormat format
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

            const auto texture = Texture2D::Create(
                &context,
                format,
                (uint32_t)width,
                (uint32_t)height,
                pixels
            );

            stbi_image_free(pixels);

            return texture;
        }

        Ref<Texture> LoadImage(
            const GraphicsContext& context,
            fastgltf::Asset& asset,
            const size_t imageIndex,
            const std::filesystem::path& sourceDirectory,
            const EImageFormat format
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
                    [](auto&) {},
                    [&](const fastgltf::sources::URI& source)
                    {
                        const auto path = sourceDirectory / source.uri.fspath();
                        texture = TextureLoader::Load(
                            path,
                            format
                        );
                    },
                    [&](const fastgltf::sources::Array& source)
                    {
                        texture = DecodeImage(
                            context,
                            source.bytes.data(),
                            source.bytes.size(),
                            format
                        );
                    },
                    [&](const fastgltf::sources::Vector& source)
                    {
                        texture = DecodeImage(
                            context,
                            source.bytes.data(),
                            source.bytes.size(),
                            format
                        );
                    },
                    [&](fastgltf::sources::BufferView& viewSource)
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
                                [](auto&) {},
                                [&](const fastgltf::sources::Array& source)
                                {
                                    texture = DecodeImage(
                                        context,
                                        source.bytes.data() + view.byteOffset,
                                        view.byteLength,
                                        format
                                    );
                                },
                                [&](const fastgltf::sources::Vector& source)
                                {
                                    texture = DecodeImage(
                                        context,
                                        source.bytes.data() + view.byteOffset,
                                        view.byteLength,
                                        format
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
            const std::filesystem::path& sourceDirectory,
            const EImageFormat format,
            std::unordered_map<uint64_t, Ref<Texture>>& cache
        )
        {
            const uint64_t cacheKey =
                (uint64_t(textureIndex) << 1) |
                (format == EImageFormat::R8G8B8A8_SRGB ? 1ull : 0ull);

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
                sourceDirectory,
                format
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

        Ref<Material> CreateSurfaceMaterial(
            const fastgltf::Material& source,
            const size_t materialIndex,
            const std::string_view meshName,
            const Ref<Texture>& baseColorTexture,
            const Ref<Texture>& metallicRoughnessTexture,
            const Ref<Texture>& normalTexture,
            const Ref<Texture>& occlusionTexture,
            const Ref<Texture>& emissiveTexture
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

            if (source.alphaMode == fastgltf::AlphaMode::Mask)
                material->SetAlphaCutoff(source.alphaCutoff);

            const auto& pbr = source.pbrData;
            const auto baseColorFactor = glm::vec4(
                pbr.baseColorFactor.x(),
                pbr.baseColorFactor.y(),
                pbr.baseColorFactor.z(),
                pbr.baseColorFactor.w()
            );
            const auto emissiveFactor = glm::vec4(
                source.emissiveFactor.x(),
                source.emissiveFactor.y(),
                source.emissiveFactor.z(),
                0.0f
            );

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

                const auto texture = graph.AddNode<TextureSample>("BaseColorTexture");
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

                const auto sample = graph.AddNode<TextureSample>("MetallicRoughnessTexture");

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

                const auto sample = graph.AddNode<TextureSample>("EmissiveTexture");
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

                const auto sample = graph.AddNode<TextureSample>(
                    "NormalTexture",
                    ETextureSampleType::Normal
                );

                const auto scale = graph.AddNode<Parameter>(
                    "NormalScale",
                    EMaterialValueType::Float
                );

                const auto flatten = graph.AddNode<FlattenNormal>();
                graph.Connect(sample, flatten, 0);
                graph.Connect(scale, flatten, 1);

                graph.SetChannel(EMaterialChannel::Normal, flatten);
            }

            if (occlusionTexture)
            {
                EE_CORE_ASSERT(material->DefineParameter("OcclusionTexture", {
                    .Kind = EMaterialParameterKind::Texture,
                    .DefaultValue = SMaterialParameter::MakeTexture(occlusionTexture),
                }), "Could not define OcclusionTexture.");

                const auto sample = graph.AddNode<TextureSample>("OcclusionTexture");
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

            material->SetGraph(std::move(graph));
            return material;
        }

        void LoadMaterials(
            SStaticMeshData& mesh,
            const GraphicsContext& context,
            fastgltf::Asset& asset,
            const std::filesystem::path& sourceDirectory
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

                if (source.pbrData.baseColorTexture)
                {
                    baseColorTexture = LoadTexture(
                        context,
                        asset,
                        source.pbrData.baseColorTexture->textureIndex,
                        sourceDirectory,
                        EImageFormat::R8G8B8A8_SRGB,
                        textureCache
                    );
                }

                if (source.pbrData.metallicRoughnessTexture)
                {
                    metallicRoughnessTexture = LoadTexture(
                        context,
                        asset,
                        source.pbrData.metallicRoughnessTexture->textureIndex,
                        sourceDirectory,
                        EImageFormat::R8G8B8A8_UNORM,
                        textureCache
                    );
                }

                if (source.normalTexture)
                {
                    normalTexture = LoadTexture(
                        context,
                        asset,
                        source.normalTexture->textureIndex,
                        sourceDirectory,
                        EImageFormat::R8G8B8A8_UNORM,
                        textureCache
                    );
                }

                if (source.occlusionTexture)
                {
                    occlusionTexture = LoadTexture(
                        context,
                        asset,
                        source.occlusionTexture->textureIndex,
                        sourceDirectory,
                        EImageFormat::R8G8B8A8_UNORM,
                        textureCache
                    );
                }

                if (source.emissiveTexture)
                {
                    emissiveTexture = LoadTexture(
                        context,
                        asset,
                        source.emissiveTexture->textureIndex,
                        sourceDirectory,
                        EImageFormat::R8G8B8A8_SRGB,
                        textureCache
                    );
                }

                mesh.Materials.push_back(CreateSurfaceMaterial(
                    source,
                    i,
                    mesh.Name,
                    baseColorTexture,
                    metallicRoughnessTexture,
                    normalTexture,
                    occlusionTexture,
                    emissiveTexture
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

        fastgltf::Parser parser;

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
