#pragma once

#include <Engine/Mesh/GeometryAllocation.h>

namespace Elixir
{
    namespace Materials { class Material; }
    using namespace Materials;

    struct SGeometry;

    /** @brief Stores the vertex attributes supported by static meshes. */
    struct SStaticMeshVertex
    {
        /** Vertex position in mesh-local units. */
        glm::vec3 Position{};

        /** Surface normal in mesh-local space. */
        glm::vec3 Normal{ 0.0f, 1.0f, 0.0f };

        /** Tangent xyz and bitangent handedness in w. */
        glm::vec4 Tangent{ 1.0f, 0.0f, 0.0f, 1.0f };

        /** First texture coordinate channel. */
        glm::vec2 TexCoord{};
    };

    /** @brief Defines an axis-aligned bound in mesh-local space. */
    struct SStaticMeshBounds
    {
        glm::vec3 Min{};
        glm::vec3 Max{};
    };

    /** @brief Describes one drawable range in a static mesh. */
    struct SStaticMeshSection
    {
        uint32_t FirstIndex = 0;
        uint32_t IndexCount = 0;
        uint32_t VertexOffset = 0;
        uint32_t MaterialIndex = 0;
        SStaticMeshBounds LocalBounds;
    };

    /** @brief Stores CPU geometry loaded from one mesh asset. */
    struct SStaticMeshData
    {
        std::string Name;
        std::vector<SStaticMeshVertex> Vertices;
        std::vector<uint32_t> Indices;
        std::vector<SStaticMeshSection> Sections;
        std::vector<Ref<Material>> Materials;
        SStaticMeshBounds LocalBounds;
    };

    /**
     * @brief Represents reusable, non-deforming geometry in the runtime.
     *
     * A static mesh references geometry stored by GeometryPool. It contains no scene
     * transform. Its sections correspond to source draw primitives.
     */
    class ELIXIR_API StaticMesh final
    {
    public:
        /**
         * @brief Create a mesh that references uploaded pooled geometry.
         * @param name Display name for the mesh.
         * @param geometry Allocation that owns the uploaded geometry range.
         * @param sections Draw ranges that belong to the geometry.
         * @param materials Materials referenced by the mesh sections.
         * @param localBounds Bound of the mesh in mesh-local space.
         * @return A mesh, or nullptr when geometry, sections, or materials are invalid.
         */
        static Ref<StaticMesh> Create(
            std::string name,
            GeometryAllocation geometry,
            std::vector<SStaticMeshSection> sections,
            std::vector<Ref<Material>> materials,
            SStaticMeshBounds localBounds
        );

        /** @brief Get the vertex layout used by every static mesh section. */
        static const BufferLayout& GetVertexLayout();

        /** @brief Get the display name supplied by the creator. */
        const std::string& GetName() const { return m_Name; }

        /** @brief Get metadata for the mesh geometry, or nullptr when it is no longer valid. */
        const SGeometry* GetGeometry() const { return m_Geometry.Get(); }

        /** @brief Get the immutable sections of this mesh. */
        const std::vector<SStaticMeshSection>& GetSections() const { return m_Sections; }

        /** @brief Get the materials referenced by mesh sections. */
        const std::vector<Ref<Material>>& GetMaterials() const { return m_Materials; }

        /** @brief Get the mesh bound in mesh-local space. */
        const SStaticMeshBounds& GetLocalBounds() const { return m_LocalBounds; }

    private:
        explicit StaticMesh(
            std::string name,
            GeometryAllocation geometry,
            std::vector<SStaticMeshSection> sections,
            std::vector<Ref<Material>> materials,
            const SStaticMeshBounds& localBounds
        );

        std::string m_Name;
        GeometryAllocation m_Geometry;
        std::vector<SStaticMeshSection> m_Sections;
        std::vector<Ref<Material>> m_Materials;
        SStaticMeshBounds m_LocalBounds;
    };
}
