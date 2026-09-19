#include "epch.h"
#include "GLTFStaticMeshLoader.h"

#include <Engine/Materials/DefaultMaterials.h>

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>

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
    }

    constexpr auto GLTF_OPTIONS = fastgltf::Options::LoadExternalBuffers |
                                  fastgltf::Options::GenerateMeshIndices;

    constexpr auto GLTF_CATEGORIES = fastgltf::Category::Meshes |
                                             fastgltf::Category::Accessors |
                                             fastgltf::Category::BufferViews |
                                             fastgltf::Category::Buffers;

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
        section.MaterialIndex = 0;
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
        mesh.Materials.push_back(CreateDefaultMaterials()[0]);

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

        const fastgltf::Asset& asset = loadResult.get();
        bool hasBounds = false;

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
