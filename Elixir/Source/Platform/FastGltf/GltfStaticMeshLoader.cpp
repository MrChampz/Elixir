#include "epch.h"
#include "GltfStaticMeshLoader.h"

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>

#include <numeric>

namespace Elixir
{
    namespace
    {
        void AddDiagnostic(
            SStaticMeshLoadResult& result,
            const EStaticMeshLoadDiagnosticSeverity severity,
            std::string message
        )
        {
            if (severity == EStaticMeshLoadDiagnosticSeverity::Error)
                EE_CORE_ERROR("{}", message)
            else
                EE_CORE_WARN("{}", message)
            result.Diagnostics.push_back({ severity, std::move(message) });
        }

        BufferLayout GetStaticMeshVertexLayout()
        {
            return {{{
                { EDataType::Vec3, "a_Position" },
                { EDataType::Vec3, "a_Normal" },
                { EDataType::Vec4, "a_Tangent" },
                { EDataType::Vec2, "a_TexCoord" },
            }}};
        }

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

    SStaticMeshLoadResult GltfStaticMeshLoader::Load(const SStaticMeshLoadRequest& request) const
    {
        SStaticMeshLoadResult result;
        if (!request.GraphicsContext)
        {
            AddDiagnostic(result, EStaticMeshLoadDiagnosticSeverity::Error,
                "Cannot load a static mesh without a graphics context.");
            return result;
        }

        auto data = fastgltf::GltfDataBuffer::FromPath(request.Path);
        if (data.error() != fastgltf::Error::None)
        {
            AddDiagnostic(result, EStaticMeshLoadDiagnosticSeverity::Error,
                std::format("Failed to open glTF file '{}': {}.", request.Path.string(),
                    fastgltf::getErrorMessage(data.error())));
            return result;
        }

        fastgltf::Parser parser;
        constexpr auto options = fastgltf::Options::LoadExternalBuffers |
            fastgltf::Options::GenerateMeshIndices;
        auto assetResult = parser.loadGltf(
            data.get(), request.Path.parent_path(), options,
            fastgltf::Category::Meshes | fastgltf::Category::Accessors |
                fastgltf::Category::BufferViews | fastgltf::Category::Buffers
        );
        if (assetResult.error() != fastgltf::Error::None)
        {
            AddDiagnostic(result, EStaticMeshLoadDiagnosticSeverity::Error,
                std::format("Failed to parse glTF file '{}': {}.", request.Path.string(),
                    fastgltf::getErrorMessage(assetResult.error())));
            return result;
        }

        const fastgltf::Asset& asset = assetResult.get();
        for (size_t meshIndex = 0; meshIndex < asset.meshes.size(); ++meshIndex)
        {
            const auto& sourceMesh = asset.meshes[meshIndex];
            SStaticMeshCreateInfo createInfo;
            createInfo.Name = sourceMesh.name.empty()
                ? std::format("{}_{}", request.Path.stem().string(), meshIndex)
                : std::string(sourceMesh.name);

            bool hasBounds = false;
            for (size_t primitiveIndex = 0; primitiveIndex < sourceMesh.primitives.size(); ++primitiveIndex)
            {
                const auto& primitive = sourceMesh.primitives[primitiveIndex];
                if (primitive.type != fastgltf::PrimitiveType::Triangles)
                {
                    AddDiagnostic(result, EStaticMeshLoadDiagnosticSeverity::Warning,
                        std::format("Skipping primitive {} in mesh '{}': only triangles are supported.",
                            primitiveIndex, createInfo.Name));
                    continue;
                }

                const auto position = primitive.findAttribute("POSITION");
                if (position == primitive.attributes.end()
                    || position->accessorIndex >= asset.accessors.size())
                {
                    AddDiagnostic(result, EStaticMeshLoadDiagnosticSeverity::Warning,
                        std::format("Skipping primitive {} in mesh '{}': POSITION is missing.",
                            primitiveIndex, createInfo.Name));
                    continue;
                }

                const auto& positionAccessor = asset.accessors[position->accessorIndex];
                if (positionAccessor.count > std::numeric_limits<uint32_t>::max())
                {
                    AddDiagnostic(result, EStaticMeshLoadDiagnosticSeverity::Warning,
                        std::format("Skipping primitive {} in mesh '{}': it exceeds the vertex limit.",
                            primitiveIndex, createInfo.Name));
                    continue;
                }
                std::vector<SStaticMeshVertex> vertices(positionAccessor.count);
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
                    asset, positionAccessor, [&vertices](const auto value, const size_t index)
                    {
                        vertices[index].Position = { value.x(), value.y(), value.z() };
                    });

                const auto loadAttribute = [&]<typename TValue>(
                    const std::string_view name,
                    auto&& assign
                )
                {
                    const auto attribute = primitive.findAttribute(name);
                    if (attribute == primitive.attributes.end()
                        || attribute->accessorIndex >= asset.accessors.size())
                    {
                        return;
                    }
                    const auto& accessor = asset.accessors[attribute->accessorIndex];
                    if (accessor.count != vertices.size())
                    {
                        AddDiagnostic(result, EStaticMeshLoadDiagnosticSeverity::Warning,
                            std::format("Ignoring {} in primitive {} of mesh '{}': its count differs "
                                "from POSITION.", name, primitiveIndex, createInfo.Name));
                        return;
                    }
                    fastgltf::iterateAccessorWithIndex<TValue>(asset, accessor,
                        [&assign](const auto value, const size_t index) { assign(value, index); });
                };
                loadAttribute.template operator()<fastgltf::math::fvec3>("NORMAL",
                    [&vertices](const auto value, const size_t index)
                    {
                        vertices[index].Normal = { value.x(), value.y(), value.z() };
                    });
                loadAttribute.template operator()<fastgltf::math::fvec4>("TANGENT",
                    [&vertices](const auto value, const size_t index)
                    {
                        vertices[index].Tangent = { value.x(), value.y(), value.z(), value.w() };
                    });
                loadAttribute.template operator()<fastgltf::math::fvec2>("TEXCOORD_0",
                    [&vertices](const auto value, const size_t index)
                    {
                        vertices[index].TexCoord = { value.x(), value.y() };
                    });

                std::vector<uint32_t> indices;
                if (primitive.indicesAccessor
                    && *primitive.indicesAccessor < asset.accessors.size())
                {
                    const auto& accessor = asset.accessors[*primitive.indicesAccessor];
                    if (accessor.count > std::numeric_limits<uint32_t>::max())
                    {
                        AddDiagnostic(result, EStaticMeshLoadDiagnosticSeverity::Warning,
                            std::format("Skipping primitive {} in mesh '{}': it exceeds the index limit.",
                                primitiveIndex, createInfo.Name));
                        continue;
                    }
                    indices.resize(accessor.count);
                    fastgltf::iterateAccessorWithIndex<uint32_t>(asset, accessor,
                        [&indices](const uint32_t value, const size_t index) { indices[index] = value; });
                }
                else
                {
                    indices.resize(vertices.size());
                    std::iota(indices.begin(), indices.end(), 0u);
                }

                if (vertices.empty() || indices.empty())
                {
                    AddDiagnostic(result, EStaticMeshLoadDiagnosticSeverity::Warning,
                        std::format("Skipping empty primitive {} in mesh '{}'.",
                            primitiveIndex, createInfo.Name));
                    continue;
                }

                SStaticMeshSection section;
                section.IndexCount = static_cast<uint32_t>(indices.size());
                section.MaterialSlot = primitive.materialIndex
                    && *primitive.materialIndex <= StaticMesh::NO_MATERIAL_SLOT - 1
                    ? static_cast<uint32_t>(*primitive.materialIndex)
                    : StaticMesh::NO_MATERIAL_SLOT;
                section.LocalBounds = GetBounds(vertices);
                section.Vertices = VertexBuffer::Create(request.GraphicsContext,
                    vertices.size() * sizeof(SStaticMeshVertex), vertices.data());
                section.Indices = IndexBuffer::Create(request.GraphicsContext,
                    indices.size() * sizeof(uint32_t), indices.data(), EIndexType::UInt32);

                if (!section.Vertices || !section.Indices)
                {
                    AddDiagnostic(result, EStaticMeshLoadDiagnosticSeverity::Error,
                        std::format("Failed to create GPU buffers for primitive {} in mesh '{}'.",
                            primitiveIndex, createInfo.Name));
                    continue;
                }
                section.Vertices->SetLayout(GetStaticMeshVertexLayout());

                if (!hasBounds)
                {
                    createInfo.LocalBounds = section.LocalBounds;
                    hasBounds = true;
                }
                else
                {
                    ExpandBounds(createInfo.LocalBounds, section.LocalBounds);
                }
                createInfo.Sections.push_back(std::move(section));
            }

            if (const auto mesh = StaticMesh::Create(std::move(createInfo)))
                result.Meshes.push_back(mesh);
            else
                AddDiagnostic(result, EStaticMeshLoadDiagnosticSeverity::Warning,
                    std::format("Skipping mesh {} because it has no valid triangle sections.", meshIndex));
        }

        if (result.Meshes.empty() && !result.HasErrors())
        {
            AddDiagnostic(result, EStaticMeshLoadDiagnosticSeverity::Error,
                std::format("No valid static meshes were found in '{}'.", request.Path.string()));
        }
        return result;
    }
}
