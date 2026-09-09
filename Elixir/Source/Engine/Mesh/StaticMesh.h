#pragma once

#include <Engine/Core/Core.h>
#include <Engine/Graphics/Buffer.h>

namespace Elixir
{
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

    /** @brief Describes one drawable triangle range of a static mesh. */
    struct SStaticMeshSection
    {
        /** Vertex data used by this section. */
        Ref<VertexBuffer> Vertices;

        /** Triangle indices used by this section. */
        Ref<IndexBuffer> Indices;

        /** Number of indices in this section. */
        uint32_t IndexCount = 0;

        /** Slot of the material assigned by the source asset. */
        uint32_t MaterialSlot = std::numeric_limits<uint32_t>::max();

        /** Bound of this section in mesh-local space. */
        SStaticMeshBounds LocalBounds;
    };

    /** @brief Supplies immutable data for a new static mesh. */
    struct SStaticMeshCreateInfo
    {
        /** Optional display name of the mesh. */
        std::string Name;

        /** Sections that form the mesh. */
        std::vector<SStaticMeshSection> Sections;

        /** Bound of all sections in mesh-local space. */
        SStaticMeshBounds LocalBounds;
    };

    /**
     * @brief Stores reusable, non-deforming geometry.
     *
     * A static mesh contains no scene transform. Its sections correspond to source draw
     * primitives.
     */
    class ELIXIR_API StaticMesh final
    {
    public:
        /** Identifies an unassigned material slot. */
        static constexpr uint32_t NO_MATERIAL_SLOT = std::numeric_limits<uint32_t>::max();

        /**
         * @brief Create one static mesh from prepared GPU sections.
         * @param info Immutable geometry and metadata for the mesh.
         * @return A mesh, or nullptr when no valid section was supplied.
         */
        static Ref<StaticMesh> Create(SStaticMeshCreateInfo info);

        /** @brief Get the vertex layout used by every static mesh section. */
        static const BufferLayout& GetVertexLayout();

        /** @brief Get the display name supplied by the creator. */
        const std::string& GetName() const { return m_Name; }

        /** @brief Get the immutable sections of this mesh. */
        const std::vector<SStaticMeshSection>& GetSections() const { return m_Sections; }

        /** @brief Get the mesh bound in mesh-local space. */
        const SStaticMeshBounds& GetLocalBounds() const { return m_LocalBounds; }

    private:
        explicit StaticMesh(SStaticMeshCreateInfo info);

        std::string m_Name;
        std::vector<SStaticMeshSection> m_Sections;
        SStaticMeshBounds m_LocalBounds;
    };
}
