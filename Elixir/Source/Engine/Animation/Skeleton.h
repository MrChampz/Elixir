#pragma once

#include <Engine/Core/Core.h>

#include <glm/gtc/quaternion.hpp>

namespace Elixir
{
    struct SLocalPose;
    struct SModelPose;

    /** @brief Stores a bone transform relative to its parent. */
    struct ELIXIR_API SBoneTransform
    {
        static constexpr float BoneTransformTolerance = 1e-5f;

        glm::vec3 Translation{};
        glm::quat Rotation{ 1.0f, 0.0f, 0.0f, 0.0f };
        glm::vec3 Scale{ 1.0f };

        /**
         * @brief Interpolate local translation, scale, and shortest-path rotation.
         * @param from Source transform with a unit quaternion.
         * @param to Target transform with a unit quaternion.
         * @param weight Target weight in [0, 1].
         * @return Mixed transform; opposite scale signs can produce zero scale.
         */
        static SBoneTransform Blend(
            const SBoneTransform& from,
            const SBoneTransform& to,
            float weight
        );

        /** @brief Compose translation, rotation, and scale into a matrix. */
        glm::mat4 ToMatrix() const;

        /**
         * @brief Check whether another transform differs only within the comparison tolerance.
         * @param other Transform to compare with this transform.
         * @return True when translation, scale, and rotation are nearly equal.
         * @pre Both transforms are valid.
         */
        bool IsNearlyEqual(const SBoneTransform& other) const;

        /** @brief Check for finite values, a unit quaternion, and nonzero scale. */
        bool IsValid() const;
    };

    /** @brief Defines one bone in a shared skeleton. */
    struct SBone
    {
        std::string Name;

        /** Index of an earlier bone, or -1 for a root. */
        int32_t ParentIndex = -1;
        SBoneTransform ReferenceTransform;
    };

    /** @brief Stores an immutable bone hierarchy shared by meshes and animations. */
    class ELIXIR_API Skeleton final
    {
    public:
        Skeleton(const Skeleton&) = delete;
        Skeleton& operator=(const Skeleton&) = delete;

        /**
         * @brief Create a skeleton from a validated bone hierarchy.
         * @param name Asset display name.
         * @param bones Nonempty bones with unique names, valid transforms, and parents before
         * children.
         * @return The skeleton, or null for invalid input.
         */
        static Ref<Skeleton> Create(std::string name, std::vector<SBone> bones);

        /** @brief Get the asset name. */
        const std::string& GetName() const { return m_Name; }

        /** @brief Get bones in parent-before-child order. */
        const std::vector<SBone>& GetBones() const { return m_Bones; }

        /**
         * @brief Find a bone by name.
         * @param name Bone name to find.
         * @return The bone index, or -1 when absent.
         */
        int32_t FindBone(std::string_view name) const;

        /**
         * @brief Compare bone names and parent indices for animation sharing.
         * @param other Skeleton to compare with this skeleton.
         */
        bool IsCompatible(const Skeleton& other) const;

        /**
         * @brief Compose a local pose into skeleton-space matrices.
         * @param localPose One valid parent-relative transform per bone.
         * @param output Receives one matrix per bone on success.
         * @return False for an invalid pose or result; output remains unchanged on failure.
         */
        bool BuildModelPose(
            const SLocalPose& localPose,
            SModelPose& output
        ) const;

        /** @brief Compare bone names and parent indices for direct animation sharing. */
        bool IsCompatible(const Skeleton& other) const;

        /** @brief Find a bone by name, or return -1. */
        int32_t FindBone(std::string_view name) const;

        /** @brief Get the asset name. */
        const std::string& GetName() const { return m_Name; }

        /** @brief Get bones in parent-before-child order. */
        const std::vector<SBone>& GetBones() const { return m_Bones; }

    private:
        /**
         * @brief Store a validated bone hierarchy.
         * @param name Asset display name.
         * @param bones Nonempty bones with unique names, valid transforms, and parents before
         * children.
         */
        Skeleton(std::string name, std::vector<SBone> bones);

        std::string m_Name;
        std::vector<SBone> m_Bones;
    };
}
