#include "epch.h"
#include "Skeleton.h"

#include <Engine/Animation/Pose.h>

namespace Elixir
{
    Ref<Skeleton> Skeleton::Create(
        std::string name,
        std::vector<SBone> bones,
        std::vector<SSkeletonSocket> sockets
    )
    {
        if (bones.empty() || bones.size() > INT32_MAX)
        {
            EE_CORE_ERROR("Skeleton requires a nonempty hierarchy within the bone index limit.")
            return nullptr;
        }

        std::unordered_set<std::string> names;

        for (size_t i = 0; i < bones.size(); ++i)
        {
            const auto& bone = bones[i];
            if (bone.Name.empty() ||
                !names.insert(bone.Name).second ||
                bone.ParentIndex < -1 ||
                bone.ParentIndex >= int32_t(i) ||
                !bone.ReferenceTransform.IsValid())
            {
                EE_CORE_ERROR("Skeleton contains an invalid bone at index {}.", i)
                return nullptr;
            }
        }

        if (sockets.size() > INT32_MAX)
        {
            EE_CORE_ERROR("Skeleton socket count exceeds the supported index range.")
            return nullptr;
        }

        names.clear();
        for (const auto& socket : sockets)
        {
            if (socket.Name.empty() || names.contains(socket.Name))
            {
                EE_CORE_ERROR("Skeleton socket names must be nonempty and unique.")
                return nullptr;
            }

            if (socket.BoneIndex >= bones.size() || !socket.RelativeTransform.IsValid())
            {
                EE_CORE_ERROR("Skeleton socket '{}' has an invalid bone or transform.", socket.Name)
                return nullptr;
            }

            names.insert(socket.Name);
        }

        return Ref<Skeleton>(new Skeleton(
            std::move(name),
            std::move(bones),
            std::move(sockets)
        ));
    }

    int32_t Skeleton::FindBone(const std::string_view name) const
    {
        for (size_t i = 0; i < m_Bones.size(); ++i)
            if (m_Bones[i].Name == name)
                return int32_t(i);

        return -1;
    }

    Ref<Skeleton> Skeleton::WithSockets(std::vector<SSkeletonSocket> sockets) const
    {
        return Create(m_Name, m_Bones, std::move(sockets));
    }

    int32_t Skeleton::FindSocket(const std::string_view name) const
    {
        for (size_t i = 0; i < m_Sockets.size(); ++i)
            if (m_Sockets[i].Name == name)
                return int32_t(i);

        return -1;
    }

    std::optional<glm::mat4> Skeleton::GetSocketTransform(
        const std::string_view name,
        const SModelPose& modelPose
    ) const
    {
        const auto index = FindSocket(name);
        return index < 0 ? std::nullopt : GetSocketTransform(size_t(index), modelPose);
    }

    std::optional<glm::mat4> Skeleton::GetSocketTransform(
        const size_t socketIndex,
        const SModelPose& modelPose
    ) const
    {
        if (socketIndex >= m_Sockets.size() || !modelPose.IsValid(m_Bones.size()))
            return std::nullopt;

        const auto& socket = m_Sockets[socketIndex];
        const auto result =
            modelPose.Matrices[socket.BoneIndex] * socket.RelativeTransform.ToMatrix();

        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 4; ++r)
                if (!std::isfinite(result[c][r]))
                {
                    EE_CORE_ERROR("Socket '{}' exceeds the supported matrix range.", socket.Name)
                    return std::nullopt;
                }

        return result;
    }

    bool Skeleton::IsCompatible(const Skeleton& other) const
    {
        if (m_Bones.size() != other.m_Bones.size())
            return false;

        for (size_t i = 0; i < m_Bones.size(); ++i)
        {
            if (m_Bones[i].Name != other.m_Bones[i].Name ||
                m_Bones[i].ParentIndex != other.m_Bones[i].ParentIndex)
                return false;
        }

        return true;
    }

    bool Skeleton::BuildModelPose(
        const SLocalPose& localPose,
        SModelPose& output
    ) const
    {
        if (!localPose.IsValid(m_Bones.size()))
        {
            EE_CORE_ERROR("Skeleton pose must contain one valid transform per bone.")
            return false;
        }

        SModelPose result;
        result.Matrices.resize(localPose.LocalTransforms.size());

        for (size_t i = 0; i < localPose.LocalTransforms.size(); ++i)
        {
            const auto parent = m_Bones[i].ParentIndex;
            const auto parentTransform = parent < 0
                ? glm::mat4(1.0f)
                : result.Matrices[parent];

            result.Matrices[i] = parentTransform * localPose.LocalTransforms[i].ToMatrix();

            for (int c = 0; c < 4; ++c)
            {
                for (int r = 0; r < 4; ++r)
                {
                    if (!std::isfinite(result.Matrices[i][c][r]))
                    {
                        EE_CORE_ERROR("Skeleton pose exceeds the supported matrix range.")
                        return false;
                    }
                }
            }
        }

        output = std::move(result);

        return true;
    }

    Skeleton::Skeleton(
        std::string name,
        std::vector<SBone> bones,
        std::vector<SSkeletonSocket> sockets
    ) : m_Name(std::move(name)),
        m_Bones(std::move(bones)),
        m_Sockets(std::move(sockets)) {}
}
