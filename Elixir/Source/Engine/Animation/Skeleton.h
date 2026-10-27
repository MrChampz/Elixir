#pragma once

#include <Engine/Animation/BoneTransform.h>
#include <Engine/Animation/SkeletonSocket.h>

namespace Elixir
{
    struct SLocalPose;
    struct SModelPose;

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
         * @brief Create a validated hierarchy and attachment points, or return null.
         * @param name Asset name.
         * @param bones Unique named bones with parents before children.
         * @param sockets Unique named attachment points referencing existing bone indices.
         */
        static Ref<Skeleton> Create(
            std::string name,
            std::vector<SBone> bones,
            std::vector<SSkeletonSocket> sockets = {}
        );

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
         * @brief Clone the hierarchy with replacement sockets, or return null on failure.
         * @param sockets Complete replacement list; empty removes all sockets in the clone.
         */
        Ref<Skeleton> WithSockets(std::vector<SSkeletonSocket> sockets) const;

        /** @brief Get immutable attachment points in their authored order. */
        const std::vector<SSkeletonSocket>& GetSockets() const { return m_Sockets; }

        /**
         * @brief Find a socket's index, or return -1 for an unknown name.
         * @param name Case-sensitive attachment name.
         */
        int32_t FindSocket(std::string_view name) const;

        /**
         * @brief Get an attachment matrix from a final pose in skeleton space.
         *
         * Multiply the result by the character world transform to place an attachment.
         * The pose includes animation layers and root-motion locking, without inverse binds.
         *
         * @param name Case-sensitive socket name in this skeleton.
         * @param modelPose Final model-space pose using this skeleton's bone order.
         * @return Empty for a missing socket, invalid pose, or nonfinite result.
         */
        std::optional<glm::mat4> GetSocketTransform(
            std::string_view name,
            const SModelPose& modelPose
        ) const;

        /**
         * @brief Read an attachment matrix using a cached socket index.
         * @param socketIndex Index in this skeleton's GetSockets().
         * @param modelPose Final model-space pose using this skeleton's bone order.
         * @return Empty for an invalid index, pose, or nonfinite result.
         */
        std::optional<glm::mat4> GetSocketTransform(
            size_t socketIndex,
            const SModelPose& modelPose
        ) const;

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

    private:
        /**
         * @brief Store a validated hierarchy and its sockets.
         * @param name Asset name.
         * @param bones Validated parent-before-child hierarchy.
         * @param sockets Validated attachment points.
         */
        Skeleton(
            std::string name,
            std::vector<SBone> bones,
            std::vector<SSkeletonSocket> sockets
        );

        std::string m_Name;
        std::vector<SBone> m_Bones;
        std::vector<SSkeletonSocket> m_Sockets;
    };
}
