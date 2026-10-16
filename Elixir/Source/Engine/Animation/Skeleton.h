#pragma once

#include <Engine/Core/Core.h>

#include <glm/gtc/quaternion.hpp>

namespace Elixir
{
    /** @brief Stores a bone transform relative to its parent. */
    struct ELIXIR_API SBoneTransform
    {
        glm::vec3 Translation{};
        glm::quat Rotation{ 1.0f, 0.0f, 0.0f, 0.0f };
        glm::vec3 Scale{ 1.0f };

        /** @brief Compose translation, rotation, and scale into a matrix. */
        glm::mat4 ToMatrix() const;

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

        /** @brief Create a hierarchy with unique names and parents before children, or return null. */
        static Ref<Skeleton> Create(std::string name, std::vector<SBone> bones);

        /** @brief Get the asset name. */
        const std::string& GetName() const { return m_Name; }

        /** @brief Get bones in parent-before-child order. */
        const std::vector<SBone>& GetBones() const { return m_Bones; }

        /** @brief Find a bone by name, or return -1. */
        int32_t FindBone(std::string_view name) const;

        /** @brief Compare bone names and parent indices for direct animation sharing. */
        bool IsCompatible(const Skeleton& other) const;

        /**
         * @brief Compose a local pose into skeleton-space matrices; leave output unchanged
         * on failure.
         */
        bool BuildModelPose(
            std::span<const SBoneTransform> localPose,
            std::vector<glm::mat4>& output
        ) const;

    private:
        Skeleton(std::string name, std::vector<SBone> bones);

        std::string m_Name;
        std::vector<SBone> m_Bones;
    };
}
