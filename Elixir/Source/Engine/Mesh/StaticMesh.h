#pragma once

#include <Engine/Mesh/GeometryAllocation.h>

namespace Elixir
{
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
        uint32_t MaterialSlot = std::numeric_limits<uint32_t>::max();
        SStaticMeshBounds LocalBounds;
    };

    /** @brief Stores CPU geometry loaded from one mesh asset. */
    struct SStaticMeshData
    {
        std::string Name;
        std::vector<SStaticMeshVertex> Vertices;
        std::vector<uint32_t> Indices;
        std::vector<SStaticMeshSection> Sections;
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
        /** Identifies an unassigned material slot. */
        static constexpr uint32_t NO_MATERIAL_SLOT = std::numeric_limits<uint32_t>::max();

        /**
         * @brief Create a mesh that references uploaded pooled geometry.
         * @param name Display name for the mesh.
         * @param geometry Allocation that owns the uploaded geometry range.
         * @param sections Draw ranges that belong to the geometry.
         * @param localBounds Bound of the mesh in mesh-local space.
         * @return A mesh, or nullptr when geometry is invalid or sections are empty.
         */
        static Ref<StaticMesh> Create(
            std::string name,
            GeometryAllocation geometry,
            std::vector<SStaticMeshSection> sections,
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

        /** @brief Get the mesh bound in mesh-local space. */
        const SStaticMeshBounds& GetLocalBounds() const { return m_LocalBounds; }

    private:
        explicit StaticMesh(
            std::string name,
            GeometryAllocation geometry,
            std::vector<SStaticMeshSection> sections,
            const SStaticMeshBounds& localBounds
        );

        std::string m_Name;
        GeometryAllocation m_Geometry;
        std::vector<SStaticMeshSection> m_Sections;
        SStaticMeshBounds m_LocalBounds;
    };
}
