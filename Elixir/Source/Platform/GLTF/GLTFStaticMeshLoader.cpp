#include "epch.h"
#include "GLTFStaticMeshLoader.h"

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>

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
                EE_CORE_WARN("{}", message);
            result.Diagnostics.push_back({ severity, std::move(message) });
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

    constexpr auto GLTF_OPTIONS = fastgltf::Options::LoadExternalBuffers |
                                  fastgltf::Options::GenerateMeshIndices;

    constexpr auto GLTF_CATEGORIES = fastgltf::Category::Meshes |
                                             fastgltf::Category::Accessors |
                                             fastgltf::Category::BufferViews |
                                             fastgltf::Category::Buffers;

    static void LoadPrimitive(
        const SStaticMeshLoadRequest& request,
        SStaticMeshLoadResult& result,
        SStaticMeshCreateInfo& mesh,
        const fastgltf::Asset& asset,
        const fastgltf::Primitive& primitive,
        bool& hasBounds
    )
    {
        if (primitive.type != fastgltf::PrimitiveType::Triangles)
        {
            AddDiagnostic(
                result,
                EStaticMeshLoadDiagnosticSeverity::Warning,
                std::format(
                    "Skipping primitive in mesh '{}': only triangles are supported.",
                    mesh.Name
                )
            );
            return;
        }

        const auto position = primitive.findAttribute("POSITION");

        if (position == primitive.attributes.end() ||
            position->accessorIndex >= asset.accessors.size())
        {
            AddDiagnostic(
                result,
                EStaticMeshLoadDiagnosticSeverity::Warning,
                std::format(
                    "Skipping primitive in mesh '{}': POSITION is missing.",
                    mesh.Name
                )
            );
            return;
        }

        const auto& positionAccessor = asset.accessors[position->accessorIndex];
        if (positionAccessor.count > std::numeric_limits<uint32_t>::max())
        {
            AddDiagnostic(
                result,
                EStaticMeshLoadDiagnosticSeverity::Warning,
                std::format(
                    "Skipping primitive in mesh '{}': it exceeds the vertex limit.",
                    mesh.Name
                )
            );
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
                AddDiagnostic(
                    result,
                    EStaticMeshLoadDiagnosticSeverity::Warning,
                    std::format(
                        "Ignoring {} in primitive of mesh '{}': its count differs from POSITION.",
                        name,
                        mesh.Name
                    )
                );
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
                AddDiagnostic(
                    result,
                    EStaticMeshLoadDiagnosticSeverity::Warning,
                    std::format(
                        "Skipping primitive in mesh '{}': it exceeds the index limit.",
                        mesh.Name
                    )
                );
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
            AddDiagnostic(
                result,
                EStaticMeshLoadDiagnosticSeverity::Warning,
                std::format(
                    "Skipping empty primitive in mesh '{}'.",
                    mesh.Name
                )
            );
            return;
        }

        SStaticMeshSection section;
        section.IndexCount = (uint32_t)indices.size();
        section.MaterialSlot = primitive.materialIndex &&
            *primitive.materialIndex <= StaticMesh::NO_MATERIAL_SLOT - 1
                ? (uint32_t)*primitive.materialIndex
                : StaticMesh::NO_MATERIAL_SLOT;
        section.LocalBounds = GetBounds(vertices);
        section.Vertices = VertexBuffer::Create(
            request.GraphicsContext,
            vertices.size() * sizeof(SStaticMeshVertex),
            vertices.data()
        );
        section.Indices = IndexBuffer::Create(
            request.GraphicsContext,
            indices.size() * sizeof(uint32_t),
            indices.data(),
            EIndexType::UInt32
        );

        if (!section.Vertices || !section.Indices)
        {
            AddDiagnostic(
                result,
                EStaticMeshLoadDiagnosticSeverity::Error,
                std::format(
                    "Failed to create GPU buffers for primitive in mesh '{}'.",
                    mesh.Name
                )
            );
            return;
        }

        section.Vertices->SetLayout(StaticMesh::GetVertexLayout());

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

    static void LoadMesh(
        const SStaticMeshLoadRequest& request,
        SStaticMeshLoadResult& result,
        const fastgltf::Asset& asset,
        const fastgltf::Mesh& mesh
    )
    {
        const auto name = mesh.name;

        SStaticMeshCreateInfo info;
        info.Name = name;

        bool hasBounds = false;

        for (const auto& primitive : mesh.primitives)
            LoadPrimitive(request, result, info, asset, primitive, hasBounds);

        if (const auto staticMesh = StaticMesh::Create(std::move(info)))
            result.Meshes.push_back(staticMesh);
        else
            AddDiagnostic(
                result,
                EStaticMeshLoadDiagnosticSeverity::Error,
                std::format(
                    "Skipping mesh '{}' because it has no valid triangle sections.",
                    name
                )
            );
    }

    static void LoadMergedMesh(
        const SStaticMeshLoadRequest& request,
        SStaticMeshLoadResult& result,
        const fastgltf::Asset& asset
    )
    {
        SStaticMeshCreateInfo info;
        info.Name = request.Path.stem().string();

        bool hasBounds = false;

        for (const auto& mesh : asset.meshes)
            for (const auto& primitive : mesh.primitives)
                LoadPrimitive(request, result, info, asset, primitive, hasBounds);

        if (!info.Sections.empty())
            if (const auto staticMesh = StaticMesh::Create(std::move(info)))
                result.Meshes.push_back(staticMesh);
    }

    GLTFStaticMeshLoader::GLTFStaticMeshLoader(SGLTFStaticMeshImportOptions options)
      : m_Options(options) {}

    SStaticMeshLoadResult GLTFStaticMeshLoader::Load(
        const SStaticMeshLoadRequest& request
    ) const
    {
        SStaticMeshLoadResult result;

        if (!request.GraphicsContext)
        {
            AddDiagnostic(
                result,
                EStaticMeshLoadDiagnosticSeverity::Error,
                "Cannot load a static mesh without a graphics context."
            );
            return result;
        }

        auto data = fastgltf::GltfDataBuffer::FromPath(request.Path);
        if (data.error() != fastgltf::Error::None)
        {
            AddDiagnostic(
                result,
                EStaticMeshLoadDiagnosticSeverity::Error,
                std::format(
                    "Failed to open glTF file '{}': {}.",
                    request.Path.string(),
                    fastgltf::getErrorMessage(data.error())
                )
            );
            return result;
        }

        fastgltf::Parser parser;

        auto loadResult = parser.loadGltf(
            data.get(),
            request.Path.parent_path(),
            GLTF_OPTIONS,
            GLTF_CATEGORIES
        );

        if (loadResult.error() != fastgltf::Error::None)
        {
            AddDiagnostic(
                result,
                EStaticMeshLoadDiagnosticSeverity::Error,
                std::format(
                    "Failed to parse glTF file '{}': {}.",
                    request.Path.string(),
                    fastgltf::getErrorMessage(loadResult.error())
                )
            );
            return result;
        }

        const fastgltf::Asset& asset = loadResult.get();

        if (m_Options.MergeMeshes)
            LoadMergedMesh(request, result, asset);
        else
            for (auto& mesh : asset.meshes)
                LoadMesh(request, result, asset, mesh);

        if (result.Meshes.empty() && !result.HasErrors())
        {
            AddDiagnostic(
                result,
                EStaticMeshLoadDiagnosticSeverity::Error,
                std::format(
                    "No valid static meshes were found in '{}'.",
                    request.Path.string()
                )
            );
        }

        return result;
    }
}
