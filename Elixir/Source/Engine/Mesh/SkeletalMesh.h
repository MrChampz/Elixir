#pragma once

#include <Engine/Animation/AnimationSequence.h>
#include <Engine/Mesh/Mesh.h>

namespace Elixir
{
    struct SStaticMeshVertex;

    /** @brief Stores mesh attributes and up to four skin influences. */
    struct SSkeletalMeshVertex
    {
        /** Vertex position in mesh-local units. */
        glm::vec3 Position{};

        /** Surface normal in mesh-local space. */
        glm::vec3 Normal{ 0.0f, 1.0f, 0.0f };

        /** Tangent xyz and bitangent handedness in w. */
        glm::vec4 Tangent{ 1.0f, 0.0f, 0.0f, 1.0f };

        /** First texture coordinate channel. */
        glm::vec2 TexCoord{};

        /** Second texture coordinate channel. */
        glm::vec2 TexCoord1{};

        /** Indices into the mesh bone palette, rather than directly into Skeleton. */
        glm::uvec4 Joints{};

        /** Nonnegative weights; creation normalizes each vertex to a sum of one. */
        glm::vec4 Weights{ 1.0f, 0.0f, 0.0f, 0.0f };
    };

    /** @brief Stores source geometry and mesh-specific skin binding data. */
    struct SSkeletalMeshData
    {
        std::string Name;

        Ref<const Skeleton> Skeleton;

        /**
         * Mesh-specific local reference pose; an empty input uses the shared skeleton
         * reference pose.
         */
        std::vector<SBoneTransform> ReferencePose;

        std::vector<SSkeletalMeshVertex> Vertices;

        /** Section-local triangle indices, interpreted with each section's VertexOffset. */
        std::vector<uint32_t> Indices;

        std::vector<SMeshSection> Sections;

        std::vector<Ref<Materials::Material>> Materials;

        /** Maps mesh palette indices to shared skeleton bone indices. */
        std::vector<uint32_t> BoneMap;

        /** Maps source vertex positions to each palette bone's bind space. */
        std::vector<glm::mat4> InverseBindMatrices;

        /** Separate animation assets imported with this model. */
        std::vector<Ref<const AnimationSequence>> Animations;
    };

    /** @brief Stores immutable skinned geometry independently from its runtime pose. */
    class ELIXIR_API SkeletalMesh final
    {
    public:
        SkeletalMesh(const SkeletalMesh&) = delete;
        SkeletalMesh& operator=(const SkeletalMesh&) = delete;

        /**
         * @brief Validate geometry, sections, materials, and skin bindings.
         * @param data Source geometry and skin bindings; weights are normalized on success.
         * @return The mesh, or null for invalid input.
         */
        static Ref<SkeletalMesh> Create(SSkeletalMeshData data);

        /** @brief Get the asset display name. */
        const std::string& GetName() const { return m_Data.Name; }

        /** @brief Get all drawable sections of the model. */
        const std::vector<SMeshSection>& GetSections() const { return m_Data.Sections; }

        /** @brief Get the materials referenced by the model sections. */
        const std::vector<Ref<Materials::Material>>& GetMaterials() const
        {
            return m_Data.Materials;
        }

        /** @brief Get the complete model bounds in its reference pose. */
        const SMeshBounds& GetLocalBounds() const { return m_LocalBounds; }

        /** @brief Get the separate animation assets associated with this model. */
        const std::vector<Ref<const AnimationSequence>>& GetAnimations() const
        {
            return m_Data.Animations;
        }

        /** @brief Get the immutable source data shared by all mesh instances. */
        const SSkeletalMeshData& GetData() const { return m_Data; }

        /** @brief Get the shared skeleton that defines the mesh's bone hierarchy. */
        const Ref<const Skeleton>& GetSkeleton() const { return m_Data.Skeleton; }

        /**
         * @brief Deform source vertices into skeleton space for a model pose.
         * @param modelPose One skeleton-space matrix per bone.
         * @param output Receives deformed vertices only when every result is valid.
         * @return False for an invalid pose, skin transform, or deformed vertex; output
         *     remains unchanged.
         */
        bool SkinVertices(
            std::span<const glm::mat4> modelPose,
            std::vector<SStaticMeshVertex>& output
        ) const;

    private:
        /**
         * @brief Store validated geometry and build its reference bounds.
         * @param data Validated source geometry and skin bindings.
         */
        explicit SkeletalMesh(SSkeletalMeshData data);

        SSkeletalMeshData m_Data;
        SMeshBounds m_LocalBounds;
    };
}
