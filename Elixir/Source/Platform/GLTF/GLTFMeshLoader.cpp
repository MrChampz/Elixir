#include "epch.h"
#include "GLTFMeshLoader.h"
#include "GLTFMaterialLoader.h"

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <numeric>
#include <glm/gtx/compatibility.hpp>

namespace Elixir
{
    namespace
    {
        constexpr auto GLTF_OPTIONS =
            fastgltf::Options::LoadExternalBuffers |
            fastgltf::Options::GenerateMeshIndices |
            fastgltf::Options::DecomposeNodeMatrices;

        constexpr auto GLTF_EXTENSIONS =
            fastgltf::Extensions::KHR_materials_emissive_strength |
            fastgltf::Extensions::KHR_materials_specular |
            fastgltf::Extensions::KHR_texture_transform |
            fastgltf::Extensions::KHR_materials_clearcoat |
            fastgltf::Extensions::KHR_materials_sheen |
            fastgltf::Extensions::KHR_materials_transmission |
            fastgltf::Extensions::KHR_materials_ior |
            fastgltf::Extensions::KHR_materials_volume;

        // Check storage and cross-references before parser validation or accessor iteration.
        bool ValidateStorage(const fastgltf::Asset& asset)
        {
            for (const auto& view : asset.bufferViews)
            {
                if (view.bufferIndex >= asset.buffers.size())
                    return false;

                const auto& buffer = asset.buffers[view.bufferIndex];

                const size_t loadedSize = std::visit(
                    [](const auto& source) -> size_t
                    {
                        if constexpr (requires { source.bytes.size(); })
                            return source.bytes.size();
                        return 0;
                    },
                    buffer.data
                );

                if (view.byteOffset > buffer.byteLength ||
                    view.byteLength > buffer.byteLength - view.byteOffset ||
                    view.byteOffset > loadedSize ||
                    view.byteLength > loadedSize - view.byteOffset)
                    return false;
            }

            const auto rangeValid = [&](
                const size_t viewIndex,
                const size_t offset,
                const size_t count,
                const size_t elementSize,
                const bool tightlyPacked
            ) -> bool
            {
                if (viewIndex >= asset.bufferViews.size() || elementSize == 0)
                    return false;

                const auto& view = asset.bufferViews[viewIndex];
                const auto stride = tightlyPacked
                    ? elementSize
                    : view.byteStride.value_or(elementSize);

                if (stride < elementSize || offset > view.byteLength)
                    return false;

                const auto remaining = view.byteLength - offset;

                return count == 0 || (remaining >= elementSize &&
                    count - 1 <= (remaining - elementSize) / stride);
            };

            for (const auto& accessor : asset.accessors)
            {
                if (accessor.type == fastgltf::AccessorType::Invalid ||
                    accessor.componentType == fastgltf::ComponentType::Invalid)
                    return false;

                const auto elementSize = fastgltf::getElementByteSize(
                    accessor.type,
                    accessor.componentType
                );

                if (accessor.bufferViewIndex &&
                    !rangeValid(
                        *accessor.bufferViewIndex,
                        accessor.byteOffset,
                        accessor.count,
                        elementSize,
                        false
                    ))
                    return false;

                if (!accessor.sparse) continue;
                const auto& sparse = *accessor.sparse;

                if (sparse.count > accessor.count ||
                    (sparse.indexComponentType != fastgltf::ComponentType::UnsignedByte &&
                        sparse.indexComponentType != fastgltf::ComponentType::UnsignedShort &&
                        sparse.indexComponentType != fastgltf::ComponentType::UnsignedInt))
                    return false;

                const auto indexSize = fastgltf::getComponentByteSize(sparse.indexComponentType);
                if (!rangeValid(
                        sparse.indicesBufferView,
                        sparse.indicesByteOffset,
                        sparse.count,
                        indexSize,
                        true
                    ) || !rangeValid(
                        sparse.valuesBufferView,
                        sparse.valuesByteOffset,
                        sparse.count,
                        elementSize,
                        true
                    ))
                    return false;

                const auto bytes = fastgltf::DefaultBufferDataAdapter{}(
                    asset,
                    sparse.indicesBufferView
                );

                uint32_t previous = 0;
                for (size_t i = 0; i < sparse.count; ++i)
                {
                    uint32_t index = 0;

                    const auto* source = bytes.data() + sparse.indicesByteOffset + i * indexSize;

                    if (indexSize == 1)
                    {
                        index = std::to_integer<uint8_t>(*source);
                    }
                    else if (indexSize == 2)
                    {
                        uint16_t value;
                        std::memcpy(&value, source, sizeof(value)); index = value;
                    }
                    else
                    {
                        std::memcpy(&index, source, sizeof(index));
                    }

                    if (index >= accessor.count || (i > 0 && index <= previous))
                        return false;

                    previous = index;
                }
            }

            for (const auto& animation : asset.animations)
            {
                for (const auto& sampler : animation.samplers)
                    if (sampler.inputAccessor >= asset.accessors.size() ||
                        sampler.outputAccessor >= asset.accessors.size())
                        return false;

                for (const auto& channel : animation.channels)
                    if (channel.samplerIndex >= animation.samplers.size() ||
                        !channel.nodeIndex ||
                        *channel.nodeIndex >= asset.nodes.size())
                        return false;
            }

            return true;
        }

        std::optional<fastgltf::Asset> Parse(const std::filesystem::path& path)
        {
            auto bytes = fastgltf::GltfDataBuffer::FromPath(path);

            if (bytes.error() != fastgltf::Error::None)
            {
                EE_CORE_ERROR("Cannot read mesh source '{}'.", path.string())
                return std::nullopt;
            }

            fastgltf::Parser parser{ GLTF_EXTENSIONS };
            auto result = parser.loadGltf(
                bytes.get(),
                path.parent_path(),
                GLTF_OPTIONS
            );

            if (result.error() != fastgltf::Error::None)
            {
                EE_CORE_ERROR("Cannot parse mesh '{}': {}.", path.string(), fastgltf::getErrorMessage(result.error()))
                return std::nullopt;
            }

            auto asset = std::move(result.get());

            if (!ValidateStorage(asset) || fastgltf::validate(asset) != fastgltf::Error::None)
            {
                EE_CORE_ERROR("Mesh source '{}' is not valid glTF.", path.string())
                return std::nullopt;
            }

            return asset;
        }

        glm::mat4 ToMatrix(const fastgltf::math::fmat4x4& value)
        {
            glm::mat4 matrix(1.0f);
            for (size_t c = 0; c < 4; ++c)
                for (size_t r = 0; r < 4; ++r) matrix[c][r] = value[c][r];
            return matrix;
        }

        class Loader final
        {
        public:
            Loader(fastgltf::Asset& asset, std::string name) : m_Asset(asset)
            {
                m_Result.Name = std::move(name);
            }

            // Import static sections through the same surface reader as skeletal meshes.
            std::optional<SStaticMeshData> LoadStaticMesh()
            {
                std::vector<size_t> nodes;
                if (!CollectMeshNodes(nodes)) return std::nullopt;

                SStaticMeshData result;
                result.Name = m_Result.Name;
                result.LocalBounds = {
                    .Min = glm::vec3(std::numeric_limits<float>::max()),
                    .Max = glm::vec3(std::numeric_limits<float>::lowest())
                };

                const auto appendMesh = [&](const fastgltf::Mesh& mesh, const glm::mat4& transform)
                {
                    for (const auto& primitive : mesh.primitives)
                    {
                        SPrimitiveData surface;

                        if (!ReadPrimitive(primitive, surface) ||
                            !ApplyLocalTransform(surface, transform))
                            return false;

                        if (surface.Vertices.size() > UINT32_MAX - result.Vertices.size() ||
                            surface.Indices.size() > UINT32_MAX - result.Indices.size())
                        {
                            ReportError("Model exceeds the supported geometry range.");
                            return false;
                        }

                        result.Sections.push_back({
                            .FirstIndex = uint32_t(result.Indices.size()),
                            .IndexCount = uint32_t(surface.Indices.size()),
                            .VertexOffset = uint32_t(result.Vertices.size()),
                            .MaterialIndex = surface.MaterialIndex,
                            .LocalBounds = surface.Bounds,
                            .VolumeToLocal = glm::inverse(glm::mat3(transform))
                        });

                        result.Vertices.insert(
                            result.Vertices.end(),
                            surface.Vertices.begin(),
                            surface.Vertices.end()
                        );

                        result.Indices.insert(
                            result.Indices.end(),
                            surface.Indices.begin(),
                            surface.Indices.end()
                        );

                        result.LocalBounds.Min = glm::min(
                            result.LocalBounds.Min,
                            surface.Bounds.Min
                        );

                        result.LocalBounds.Max = glm::max(
                            result.LocalBounds.Max,
                            surface.Bounds.Max
                        );
                    }

                    return true;
                };

                if (m_Asset.nodes.empty() && m_Asset.scenes.empty())
                {
                    for (const auto& mesh : m_Asset.meshes)
                        if (!appendMesh(mesh, glm::mat4(1)))
                            return std::nullopt;
                }
                else
                {
                    for (const auto index : nodes)
                    {
                        const auto meshIndex = *m_Asset.nodes[index].meshIndex;
                        if (meshIndex >= m_Asset.meshes.size() ||
                            !appendMesh(m_Asset.meshes[meshIndex], m_NodeTransforms[index]))
                            return std::nullopt;
                    }
                }

                if (result.Sections.empty())
                {
                    ReportError("Model has no static triangle sections.");
                    return std::nullopt;
                }

                return result;
            }

            std::optional<SSkeletalMeshData> LoadSkeletalMesh(Ref<const Skeleton> skeleton)
            {
                std::vector<size_t> nodes;
                if (!CollectMeshNodes(nodes)) return std::nullopt;

                const auto invalidIndex = std::ranges::none_of(
                    nodes,
                    [&](const auto i) {
                        return bool(m_Asset.nodes[i].skinIndex);
                    }
                );

                if (nodes.empty() || invalidIndex)
                {
                    ReportError("Scene contains no skinned mesh.");
                    return std::nullopt;
                }

                if (!LoadSkeleton(nodes, std::move(skeleton)))
                    return std::nullopt;

                for (const auto index : nodes)
                {
                    const auto& node = m_Asset.nodes[index];
                    if (!node.meshIndex || *node.meshIndex >= m_Asset.meshes.size())
                    {
                        ReportError("Invalid mesh node.");
                        return std::nullopt;
                    }

                    if (!LoadBinding(node)) return std::nullopt;

                    const auto& mesh = m_Asset.meshes[*node.meshIndex];
                    if (!mesh.weights.empty())
                    {
                        ReportError("Morph target weights are not supported by skeletal import yet.");
                        return std::nullopt;
                    }

                    for (const auto& primitive : mesh.primitives)
                        if (!LoadPrimitive(primitive, bool(node.skinIndex)))
                            return std::nullopt;
                }

                if (m_Result.Sections.empty())
                {
                    ReportError("Model has no triangle sections.");
                    return std::nullopt;
                }

                if (!LoadAnimations()) return std::nullopt;

                return std::move(m_Result);
            }

        private:
            struct SPrimitiveData
            {
                std::vector<SStaticMeshVertex> Vertices;
                std::vector<uint32_t> Indices;
                uint32_t MaterialIndex = 0;
                SMeshBounds Bounds;
            };

            // Log an import failure.
            static void ReportError(const std::string_view message)
            {
                EE_CORE_ERROR("glTF mesh import failed: {}", message)
            }

            // Validate the node forest and compose transforms in parent order.
            bool BuildHierarchy()
            {
                if (m_Asset.nodes.size() > INT32_MAX)
                {
                    ReportError("Node count exceeds the supported range.");
                    return false;
                }
                m_Parents.assign(m_Asset.nodes.size(), -1);
                m_NodeOrder.clear();
                for (size_t i = 0; i < m_Asset.nodes.size(); ++i)
                {
                    for (const auto child : m_Asset.nodes[i].children)
                    {
                        if (child >= m_Parents.size() || m_Parents[child] >= 0)
                        {
                            ReportError("Node hierarchy has invalid or multiple parents.");
                            return false;
                        }
                        m_Parents[child] = int32_t(i);
                    }
                }
                for (size_t i = 0; i < m_Parents.size(); ++i)
                    if (m_Parents[i] < 0) m_NodeOrder.push_back(i);
                for (size_t i = 0; i < m_NodeOrder.size(); ++i)
                    for (const auto child : m_Asset.nodes[m_NodeOrder[i]].children) m_NodeOrder.push_back(child);
                if (m_NodeOrder.size() != m_Parents.size())
                {
                    ReportError("Node hierarchy contains a cycle.");
                    return false;
                }
                m_NodeTransforms.resize(m_Parents.size());
                for (const auto index : m_NodeOrder)
                {
                    const auto local = ToMatrix(fastgltf::getTransformMatrix(m_Asset.nodes[index]));
                    m_NodeTransforms[index] = m_Parents[index] < 0 ? local : m_NodeTransforms[m_Parents[index]] * local;
                }
                return true;
            }

            // Collect mesh nodes from the selected scene after validating their hierarchy.
            bool CollectMeshNodes(std::vector<size_t>& nodes)
            {
                if (!BuildHierarchy()) return false;
                if (m_Asset.scenes.empty())
                {
                    for (const auto index : m_NodeOrder)
                        if (m_Asset.nodes[index].meshIndex) nodes.push_back(index);
                    return true;
                }
                const auto scene = m_Asset.defaultScene.value_or(0);
                if (scene >= m_Asset.scenes.size())
                {
                    ReportError("Invalid default scene.");
                    return false;
                }
                std::vector<size_t> pending(m_Asset.scenes[scene].nodeIndices.begin(), m_Asset.scenes[scene].nodeIndices.end());
                std::unordered_set<size_t> visited;
                for (size_t i = 0; i < pending.size(); ++i)
                {
                    const auto index = pending[i];
                    if (index >= m_Asset.nodes.size() || !visited.insert(index).second)
                    {
                        ReportError("Scene contains an invalid or repeated node.");
                        return false;
                    }
                    const auto& node = m_Asset.nodes[index];
                    if (node.meshIndex) nodes.push_back(index);
                    pending.insert(pending.end(), node.children.begin(), node.children.end());
                }
                return true;
            }

            // Read one optional or required attribute with the expected shape.
            template<typename TValue, typename TAssign>
            bool ReadAttribute(
                const fastgltf::Primitive& primitive,
                const std::string_view name,
                const fastgltf::AccessorType type,
                const size_t count,
                const bool required,
                TAssign assign
            ) const
            {
                const auto attribute = primitive.findAttribute(name);
                if (attribute == primitive.attributes.end()) return !required;
                const auto* accessor = Accessor(attribute->accessorIndex, type, count);
                if (!accessor) return false;
                fastgltf::iterateAccessorWithIndex<TValue>(m_Asset, *accessor, assign);
                return true;
            }

            // Compute a bound over source or transformed surface vertices.
            static SMeshBounds GetBounds(const std::vector<SStaticMeshVertex>& vertices)
            {
                SMeshBounds bounds{ .Min = glm::vec3(std::numeric_limits<float>::max()),
                    .Max = glm::vec3(std::numeric_limits<float>::lowest()) };
                for (const auto& vertex : vertices)
                {
                    bounds.Min = glm::min(bounds.Min, vertex.Position);
                    bounds.Max = glm::max(bounds.Max, vertex.Position);
                }
                return bounds;
            }

            // Read common triangle attributes and indices before applying any skin bindings.
            bool ReadPrimitive(const fastgltf::Primitive& primitive, SPrimitiveData& output) const
            {
                if (primitive.type != fastgltf::PrimitiveType::Triangles)
                {
                    ReportError("Only triangle primitives are supported.");
                    return false;
                }
                const auto position = primitive.findAttribute("POSITION");
                if (position == primitive.attributes.end() || position->accessorIndex >= m_Asset.accessors.size())
                {
                    ReportError("Primitive requires POSITION.");
                    return false;
                }
                const auto count = m_Asset.accessors[position->accessorIndex].count;
                if (count == 0 || count > UINT32_MAX)
                {
                    ReportError("Primitive exceeds the supported vertex range.");
                    return false;
                }
                auto& vertices = output.Vertices;
                vertices.resize(count);
                const auto attributesValid = ReadAttribute<fastgltf::math::fvec3>(
                    primitive,
                    "POSITION",
                    fastgltf::AccessorType::Vec3,
                    count,
                    true,
                    [&](const auto& v, const size_t i)
                    {
                        vertices[i].Position = { v.x(), v.y(), v.z() };
                    }
                ) && ReadAttribute<fastgltf::math::fvec3>(
                    primitive,
                    "NORMAL",
                    fastgltf::AccessorType::Vec3,
                    count,
                    false,
                    [&](const auto& v, const size_t i)
                    {
                        vertices[i].Normal = { v.x(), v.y(), v.z() };
                    }
                ) && ReadAttribute<fastgltf::math::fvec4>(
                    primitive,
                    "TANGENT",
                    fastgltf::AccessorType::Vec4,
                    count,
                    false,
                    [&](const auto& v, const size_t i)
                    {
                        vertices[i].Tangent = { v.x(), v.y(), v.z(), v.w() };
                    }
                ) && ReadAttribute<fastgltf::math::fvec2>(
                    primitive,
                    "TEXCOORD_0",
                    fastgltf::AccessorType::Vec2,
                    count,
                    false,
                    [&](const auto& v, const size_t i)
                    {
                        vertices[i].TexCoord = { v.x(), v.y() };
                    }
                ) && ReadAttribute<fastgltf::math::fvec2>(
                    primitive,
                    "TEXCOORD_1",
                    fastgltf::AccessorType::Vec2,
                    count,
                    false,
                    [&](const auto& v, const size_t i)
                    {
                        vertices[i].TexCoord1 = { v.x(), v.y() };
                    }
                );
                if (!attributesValid)
                {
                    ReportError("Primitive contains missing or mismatched attributes.");
                    return false;
                }
                auto& indices = output.Indices;
                if (primitive.indicesAccessor)
                {
                    if (*primitive.indicesAccessor >= m_Asset.accessors.size())
                    {
                        ReportError("Invalid index accessor.");
                        return false;
                    }
                    const auto& accessor = m_Asset.accessors[*primitive.indicesAccessor];
                    if (accessor.type != fastgltf::AccessorType::Scalar || accessor.normalized || accessor.count > UINT32_MAX ||
                        (accessor.componentType != fastgltf::ComponentType::UnsignedByte &&
                         accessor.componentType != fastgltf::ComponentType::UnsignedShort &&
                         accessor.componentType != fastgltf::ComponentType::UnsignedInt))
                    {
                        ReportError("Invalid index accessor size or type.");
                        return false;
                    }
                    indices.resize(accessor.count);
                    fastgltf::iterateAccessorWithIndex<uint32_t>(m_Asset, accessor,
                        [&](const uint32_t value, const size_t i) { indices[i] = value; });
                }
                else
                {
                    indices.resize(count);
                    std::iota(indices.begin(), indices.end(), 0u);
                }
                if (indices.empty() || indices.size() % 3 != 0 ||
                    std::ranges::any_of(indices, [&](const auto index) { return index >= vertices.size(); }))
                {
                    ReportError("Invalid triangle indices.");
                    return false;
                }
                for (const auto& vertex : vertices)
                {
                    if (!glm::all(glm::isfinite(vertex.Position)) || !glm::all(glm::isfinite(vertex.Normal)) ||
                        !glm::all(glm::isfinite(vertex.Tangent)) || !glm::all(glm::isfinite(vertex.TexCoord)) ||
                        !glm::all(glm::isfinite(vertex.TexCoord1)))
                    {
                        ReportError("Vertex attributes must be finite.");
                        return false;
                    }
                }
                GenerateSurfaceBasis(vertices, indices,
                    primitive.findAttribute("NORMAL") == primitive.attributes.end(),
                    primitive.findAttribute("TANGENT") == primitive.attributes.end());
                output.MaterialIndex = primitive.materialIndex ? uint32_t(*primitive.materialIndex + 1) : 0;
                output.Bounds = GetBounds(vertices);
                return true;
            }

            // Bake a node transform and preserve normals, tangent handedness, and triangle winding.
            static bool ApplyLocalTransform(SPrimitiveData& geometry, const glm::mat4& transform)
            {
                const auto linear = glm::mat3(transform);
                const auto determinant = glm::determinant(linear);
                if (!std::isfinite(determinant) || std::abs(determinant) < 1e-12f)
                {
                    ReportError("Node has a singular transform.");
                    return false;
                }
                const auto normalTransform = glm::transpose(glm::inverse(linear));
                for (auto& vertex : geometry.Vertices)
                {
                    vertex.Position = glm::vec3(transform * glm::vec4(vertex.Position, 1));
                    auto normal = normalTransform * vertex.Normal;
                    vertex.Normal = glm::dot(normal, normal) > 1e-12f ? glm::normalize(normal) : glm::vec3(0, 1, 0);
                    auto tangent = linear * glm::vec3(vertex.Tangent);
                    tangent -= vertex.Normal * glm::dot(vertex.Normal, tangent);
                    if (glm::dot(tangent, tangent) < 1e-12f)
                        tangent = glm::cross(vertex.Normal, std::abs(vertex.Normal.z) < 0.9f ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0));
                    vertex.Tangent = glm::vec4(glm::normalize(tangent), vertex.Tangent.w * (determinant < 0 ? -1.0f : 1.0f));
                    if (!glm::all(glm::isfinite(vertex.Position)) || !glm::all(glm::isfinite(vertex.Normal)) ||
                        !glm::all(glm::isfinite(vertex.Tangent)))
                    {
                        ReportError("Transformed vertices must be finite.");
                        return false;
                    }
                }
                if (determinant < 0)
                    for (size_t i = 0; i < geometry.Indices.size(); i += 3) std::swap(geometry.Indices[i + 1], geometry.Indices[i + 2]);
                geometry.Bounds = GetBounds(geometry.Vertices);
                return true;
            }

            // Find an accessor with the expected shape.
            const fastgltf::Accessor* Accessor(
                const size_t index,
                const fastgltf::AccessorType type,
                const size_t count
            ) const
            {
                if (index >= m_Asset.accessors.size()) return nullptr;

                const auto& accessor = m_Asset.accessors[index];
                if (accessor.type != type || accessor.count != count) return nullptr;

                return &accessor;
            }

            // Build a shared hierarchy from joints and their ancestors.
            bool LoadSkeleton(const std::vector<size_t>& nodes, Ref<const Skeleton> skeleton)
            {
                const auto& parents = m_Parents;
                const auto& order = m_NodeOrder;

                std::vector included(parents.size(), false);
                for (const auto nodeIndex : nodes)
                {
                    const auto& node = m_Asset.nodes[nodeIndex];
                    if (node.skinIndex)
                    {
                        if (*node.skinIndex >= m_Asset.skins.size())
                        {
                            ReportError("Invalid skin index.");
                            return false;
                        }

                        const auto& skin = m_Asset.skins[*node.skinIndex];

                        if (skin.joints.empty())
                        {
                            ReportError("Skin has no joints.");
                            return false;
                        }

                        std::unordered_set<size_t> joints;
                        for (const auto joint : skin.joints)
                        {
                            if (joint >= parents.size() || !joints.insert(joint).second)
                            {
                                ReportError("Skin joints must be valid and unique.");
                                return false;
                            }

                            for (auto i = int32_t(joint); i >= 0 && !included[i]; i = parents[i])
                                included[i] = true;
                        }
                    }
                    else
                    {
                        for (auto i = int32_t(nodeIndex); i >= 0 && !included[i]; i = parents[i])
                            included[i] = true;
                    }
                }

                std::vector<SBone> bones;
                m_NodeBones.resize(parents.size(), -1);

                std::unordered_map<std::string, size_t> nameCounts;
                for (const auto index : order)
                    if (included[index])
                        ++nameCounts[std::string(m_Asset.nodes[index].name)];

                std::unordered_set<std::string> usedNames;

                for (const auto index : order)
                {
                    if (!included[index]) continue;

                    const auto& node = m_Asset.nodes[index];

                    const auto* trs = std::get_if<fastgltf::TRS>(&node.transform);
                    if (!trs)
                    {
                        ReportError("Node transform could not be decomposed.");
                        return false;
                    }

                    std::string name(node.name);
                    if (name.empty() || nameCounts[name] > 1)
                        name += "#" + std::to_string(index);

                    while (!usedNames.insert(name).second)
                        name += "#";

                    const auto& t = trs->translation;
                    const auto& r = trs->rotation;
                    const auto& s = trs->scale;

                    m_NodeBones[index] = int32_t(bones.size());

                    bones.push_back({
                        .Name = std::move(name),
                        .ParentIndex = parents[index] < 0
                            ? -1
                            : m_NodeBones[parents[index]],
                        .ReferenceTransform = {
                            .Translation = { t.x(), t.y(), t.z() },
                            .Rotation = { r.w(), r.x(), r.y(), r.z() },
                            .Scale = { s.x(), s.y(), s.z() }
                        }
                    });
                }

                for (const auto& bone : bones)
                    m_Result.ReferencePose.LocalTransforms.push_back(bone.ReferenceTransform);

                auto meshSkeleton = Skeleton::Create(
                    m_Result.Name + "_Skeleton",
                    std::move(bones)
                );
                if (!meshSkeleton) return false;

                if (skeleton && !skeleton->IsCompatible(*meshSkeleton))
                {
                    ReportError("Provided shared skeleton does not match the source hierarchy.");
                    return false;
                }

                m_Result.Skeleton = skeleton
                    ? std::move(skeleton)
                    : std::move(meshSkeleton);

                return true;
            }

            // Append this node's skin palette or its rigid attachment binding.
            bool LoadBinding(const fastgltf::Node& node)
            {
                auto& data = m_Result;
                m_PaletteOffset = uint32_t(data.BoneMap.size());

                if (!node.skinIndex)
                {
                    const auto index = size_t(&node - m_Asset.nodes.data());
                    m_PaletteCount = 1;
                    data.BoneMap.push_back(uint32_t(m_NodeBones[index]));
                    data.InverseBindMatrices.emplace_back(1.0f);
                    return true;
                }

                const auto& skin = m_Asset.skins[*node.skinIndex];
                if (skin.joints.size() > UINT32_MAX - data.BoneMap.size())
                {
                    ReportError("Model exceeds the supported skin palette size.");
                    return false;
                }

                m_PaletteCount = uint32_t(skin.joints.size());

                for (const auto joint : skin.joints)
                    data.BoneMap.push_back(uint32_t(m_NodeBones[joint]));

                data.InverseBindMatrices.resize(data.BoneMap.size(), glm::mat4(1.0f));

                if (skin.inverseBindMatrices)
                {
                    const auto* accessor = Accessor(
                        *skin.inverseBindMatrices,
                        fastgltf::AccessorType::Mat4,
                        skin.joints.size()
                    );

                    if (!accessor || accessor->componentType != fastgltf::ComponentType::Float)
                    {
                        ReportError("Invalid inverse bind matrices.");
                        return false;
                    }

                    fastgltf::iterateAccessorWithIndex<fastgltf::math::fmat4x4>(
                        m_Asset,
                        *accessor,
                        [&](const auto& matrix, const size_t i)
                        {
                            data.InverseBindMatrices[m_PaletteOffset + i] = ToMatrix(matrix);
                        }
                    );
                }

                return true;
            }

            // Append one triangle section and its skin attributes.
            bool LoadPrimitive(const fastgltf::Primitive& primitive, const bool skinned)
            {
                if (!primitive.targets.empty() ||
                    primitive.findAttribute("JOINTS_1") != primitive.attributes.end() ||
                    primitive.findAttribute("WEIGHTS_1") != primitive.attributes.end())
                {
                    ReportError("Morph targets and additional influence sets are not supported.");
                    return false;
                }

                SPrimitiveData surface;
                if (!ReadPrimitive(primitive, surface)) return false;
                auto& data = m_Result;
                if (surface.Vertices.size() > UINT32_MAX - data.Vertices.size() ||
                    surface.Indices.size() > UINT32_MAX - data.Indices.size())
                {
                    ReportError("Model exceeds the supported geometry range.");
                    return false;
                }

                std::vector<SSkeletalMeshVertex> vertices(surface.Vertices.size());
                for (size_t i = 0; i < vertices.size(); ++i)
                {
                    const auto& source = surface.Vertices[i];
                    auto& vertex = vertices[i];
                    vertex.Position = source.Position;
                    vertex.Normal = source.Normal;
                    vertex.Tangent = source.Tangent;
                    vertex.TexCoord = source.TexCoord;
                    vertex.TexCoord1 = source.TexCoord1;
                }
                if (skinned)
                {
                    if (!ReadAttribute<fastgltf::math::uvec4>(primitive, "JOINTS_0", fastgltf::AccessorType::Vec4,
                            vertices.size(), true, [&](const auto& value, const size_t i)
                            { vertices[i].Joints = { value.x(), value.y(), value.z(), value.w() }; }) ||
                        !ReadAttribute<fastgltf::math::fvec4>(primitive, "WEIGHTS_0", fastgltf::AccessorType::Vec4,
                            vertices.size(), true, [&](const auto& value, const size_t i)
                            { vertices[i].Weights = { value.x(), value.y(), value.z(), value.w() }; }))
                    {
                        ReportError("Missing or mismatched skin attributes.");
                        return false;
                    }
                }
                for (auto& vertex : vertices)
                {
                    for (int i = 0; i < 4; ++i)
                    {
                        if (vertex.Weights[i] > 0 && vertex.Joints[i] >= m_PaletteCount)
                        {
                            ReportError("Vertex joint index exceeds its source skin palette.");
                            return false;
                        }
                        vertex.Joints[i] = vertex.Weights[i] > 0 ? vertex.Joints[i] + m_PaletteOffset : 0;
                    }
                }
                data.Sections.push_back({
                    .FirstIndex = uint32_t(data.Indices.size()),
                    .IndexCount = uint32_t(surface.Indices.size()),
                    .VertexOffset = uint32_t(data.Vertices.size()),
                    .MaterialIndex = surface.MaterialIndex,
                    .LocalBounds = surface.Bounds,
                });
                data.Vertices.insert(data.Vertices.end(), vertices.begin(), vertices.end());
                data.Indices.insert(data.Indices.end(), surface.Indices.begin(), surface.Indices.end());
                return true;
            }

            // Generate missing normals and UV tangents before skinning.
            static void GenerateSurfaceBasis(
                std::vector<SStaticMeshVertex>& vertices,
                const std::vector<uint32_t>& indices,
                const bool normalsMissing,
                const bool tangentsMissing
            )
            {
                std::vector<glm::vec3> normals(vertices.size()),
                    tangents(vertices.size()),
                    bitangents(vertices.size());

                for (size_t i = 0; i < indices.size(); i += 3)
                {
                    const auto& a = vertices[indices[i]];
                    const auto& b = vertices[indices[i + 1]];
                    const auto& c = vertices[indices[i + 2]];

                    const auto edge1 = b.Position - a.Position;
                    const auto edge2 = c.Position - a.Position;

                    const auto uv1 = b.TexCoord - a.TexCoord;
                    const auto uv2 = c.TexCoord - a.TexCoord;

                    const float determinant = uv1.x * uv2.y - uv1.y * uv2.x;

                    const auto normal = glm::cross(edge1, edge2);
                    const auto tangent = std::abs(determinant) > 1e-8f
                        ? (edge1 * uv2.y - edge2 * uv1.y) / determinant
                        : glm::vec3(0);
                    const auto bitangent = std::abs(determinant) > 1e-8f
                        ? (edge2 * uv1.x - edge1 * uv2.x) / determinant
                        : glm::vec3(0);

                    for (size_t k = 0; k < 3; ++k)
                    {
                        normals[indices[i + k]] += normal;
                        tangents[indices[i + k]] += tangent;
                        bitangents[indices[i + k]] += bitangent;
                    }
                }

                for (size_t i = 0; i < vertices.size(); ++i)
                {
                    auto& v = vertices[i];

                    if (normalsMissing)
                        v.Normal = glm::dot(normals[i], normals[i]) > 1e-12f
                            ? glm::normalize(normals[i])
                            : glm::vec3(0, 1, 0);

                    if (!tangentsMissing) continue;

                    auto tangent = tangents[i] - v.Normal *
                        glm::dot(v.Normal, tangents[i]);

                    if (glm::dot(tangent, tangent) < 1e-12f)
                        tangent = glm::cross(v.Normal, std::abs(v.Normal.z) < 0.9f
                            ? glm::vec3(0, 0, 1)
                            : glm::vec3(0, 1, 0));

                    const auto bitangent = glm::cross(v.Normal, tangent);
                    const auto mirrored = glm::dot(bitangent, bitangents[i]) < 0;
                    const float handedness = mirrored ? -1.0f : 1.0f;
                    v.Tangent = glm::vec4(glm::normalize(tangent), handedness);
                }
            }

            // Import independent sequences for channels affecting the selected hierarchy.
            bool LoadAnimations()
            {
                for (const auto& animation : m_Asset.animations)
                {
                    std::vector<SBoneAnimationTrack> tracks;
                    std::unordered_map<uint32_t, size_t> trackIndices;
                    std::unordered_set<uint64_t> channels;
                    float duration = 0;

                    for (const auto& channel : animation.channels)
                    {
                        if (!channel.nodeIndex || *channel.nodeIndex >= m_NodeBones.size())
                        {
                            ReportError("Animation channel has an invalid target node.");
                            return false;
                        }

                        const auto bone = m_NodeBones[*channel.nodeIndex];
                        if (bone < 0) continue;

                        if (channel.samplerIndex >= animation.samplers.size() ||
                            channel.path == fastgltf::AnimationPath::Weights)
                        {
                            ReportError("Unsupported animation channel.");
                            return false;
                        }

                        if (!channels.insert((uint64_t(bone) << 32) | uint32_t(channel.path)).second)
                        {
                            ReportError("Animation contains duplicate bone channels.");
                            return false;
                        }

                        const auto& sampler = animation.samplers[channel.samplerIndex];

                        if (sampler.interpolation != fastgltf::AnimationInterpolation::Linear &&
                            sampler.interpolation != fastgltf::AnimationInterpolation::Step)
                        {
                            ReportError("Cubic spline animation is not supported yet.");
                            return false;
                        }

                        if (sampler.inputAccessor >= m_Asset.accessors.size())
                        {
                            ReportError("Invalid animation input.");
                            return false;
                        }

                        const auto& times = m_Asset.accessors[sampler.inputAccessor];

                        if (times.type != fastgltf::AccessorType::Scalar ||
                            times.componentType != fastgltf::ComponentType::Float ||
                            times.count == 0)
                        {
                            ReportError("Invalid animation input accessor.");
                            return false;
                        }

                        const auto type = channel.path == fastgltf::AnimationPath::Rotation
                            ? fastgltf::AccessorType::Vec4
                            : fastgltf::AccessorType::Vec3;

                        const auto* values = Accessor(
                            sampler.outputAccessor,
                            type,
                            times.count
                        );

                        if (!values || values->componentType != fastgltf::ComponentType::Float)
                        {
                            ReportError("Invalid animation output.");
                            return false;
                        }

                        std::vector<float> keyTimes(times.count);
                        fastgltf::iterateAccessorWithIndex<float>(
                            m_Asset,
                            times,
                            [&](const float t, const size_t i) { keyTimes[i] = t; }
                        );

                        duration = std::max(duration, keyTimes.back());

                        if (!trackIndices.contains(uint32_t(bone)))
                        {
                            trackIndices[uint32_t(bone)] = tracks.size();
                            tracks.push_back({ .BoneIndex = uint32_t(bone) });
                        }

                        auto& track = tracks[trackIndices[uint32_t(bone)]];

                        const auto mode = sampler.interpolation == fastgltf::AnimationInterpolation::Step
                            ? EBoneInterpolation::Step
                            : EBoneInterpolation::Linear;

                        if (channel.path == fastgltf::AnimationPath::Rotation)
                        {
                            track.RotationInterpolation = mode;
                            fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(
                                m_Asset,
                                *values,
                                [&](const auto& q, const size_t i)
                                {
                                    track.Rotations.push_back({
                                        keyTimes[i],
                                        { q.w(), q.x(), q.y(), q.z() }
                                    });
                                }
                            );
                        }
                        else
                        {
                            auto& keys = channel.path == fastgltf::AnimationPath::Translation
                                ? track.Translations
                                : track.Scales;

                            if (channel.path == fastgltf::AnimationPath::Translation)
                                track.TranslationInterpolation = mode;
                            else
                                track.ScaleInterpolation = mode;

                            fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
                                m_Asset,
                                *values,
                                [&](const auto& v, const size_t i)
                                {
                                    keys.push_back({
                                        keyTimes[i],
                                        { v.x(), v.y(), v.z() }
                                    });
                                }
                            );
                        }
                    }

                    if (tracks.empty()) continue;

                    auto sequence = Sequence::Create(
                        std::string(animation.name),
                        m_Result.Skeleton,
                        duration,
                        std::move(tracks)
                    );
                    if (!sequence) return false;

                    m_Result.Animations.push_back(std::move(sequence));
                }
                return true;
            }

            SSkeletalMeshData m_Result;
            fastgltf::Asset& m_Asset;
            std::vector<int32_t> m_Parents;
            std::vector<size_t> m_NodeOrder;
            std::vector<glm::mat4> m_NodeTransforms;
            std::vector<int32_t> m_NodeBones;
            uint32_t m_PaletteOffset = 0;
            uint32_t m_PaletteCount = 0;
        };
    }

    std::optional<SStaticMeshData> GLTFMeshLoader::LoadStaticMesh(
        const GraphicsContext& context,
        const std::filesystem::path& path
    ) const
    {
        auto asset = Parse(path);
        if (!asset) return std::nullopt;

        auto mesh = Loader(*asset, path.stem().string())
            .LoadStaticMesh();
        if (!mesh) return std::nullopt;

        mesh->Materials = LoadGLTFMaterials(
            context,
            *asset,
            path.parent_path(),
            mesh->Name
        );

        return mesh;
    }

    std::optional<SSkeletalMeshData> GLTFMeshLoader::LoadSkeletalMesh(
        const GraphicsContext& context,
        const std::filesystem::path& path,
        Ref<const Skeleton> skeleton
    ) const
    {
        auto asset = Parse(path);
        if (!asset) return std::nullopt;

        auto mesh = Loader(*asset, path.stem().string())
            .LoadSkeletalMesh(std::move(skeleton));
        if (!mesh) return std::nullopt;

        mesh->Materials = LoadGLTFMaterials(
            context,
            *asset,
            path.parent_path(),
            mesh->Name
        );

        return mesh;
    }
}
