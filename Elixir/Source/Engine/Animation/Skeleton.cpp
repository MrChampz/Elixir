#include "epch.h"
#include "Skeleton.h"

#include <glm/gtc/matrix_transform.hpp>

namespace Elixir
{
    /* SBoneTransform */

    SBoneTransform SBoneTransform::Blend(
        const SBoneTransform& from,
        const SBoneTransform& to,
        const float weight
    )
    {
        return {
            .Translation = glm::mix(from.Translation, to.Translation, weight),
            .Rotation = glm::normalize(glm::slerp(from.Rotation, to.Rotation, weight)),
            .Scale = glm::mix(from.Scale, to.Scale, weight),
        };
    }

    glm::mat4 SBoneTransform::ToMatrix() const
    {
        return glm::translate(glm::mat4(1.0f), Translation) *
            glm::mat4_cast(Rotation) *
            glm::scale(glm::mat4(1.0f), Scale);
    }

    bool SBoneTransform::IsValid() const
    {
        for (int i = 0; i < 3; ++i)
        {
            if (!std::isfinite(Translation[i]) ||
                !std::isfinite(Scale[i]) ||
                std::abs(Scale[i]) < 1e-8f)
                return false;
        }

        for (int i = 0; i < 4; ++i)
        {
            if (!std::isfinite(Rotation[i]))
                return false;
        }

        return std::abs(glm::dot(Rotation, Rotation) - 1.0f) < 1e-4f;
    }

    /* Skeleton */

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

    Skeleton::Skeleton(
        std::string name,
        std::vector<SBone> bones,
        std::vector<SSkeletonSocket> sockets
    )
      : m_Name(std::move(name)),
        m_Bones(std::move(bones)),
        m_Sockets(std::move(sockets)) {}

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

    int32_t Skeleton::FindBone(const std::string_view name) const
    {
        for (size_t i = 0; i < m_Bones.size(); ++i)
            if (m_Bones[i].Name == name)
                return int32_t(i);

        return -1;
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
        const std::span<const SBoneTransform> localPose,
        std::vector<glm::mat4>& output
    ) const
    {
        const auto invalidTransform = std::ranges::any_of(
            localPose,
            [](const auto& t) { return !t.IsValid(); }
        );

        if (localPose.size() != m_Bones.size() || invalidTransform)
        {
            EE_CORE_ERROR("Skeleton pose must contain one valid transform per bone.")
            return false;
        }

        std::vector<glm::mat4> result(localPose.size());

        for (size_t i = 0; i < localPose.size(); ++i)
        {
            const auto parent = m_Bones[i].ParentIndex;
            const auto parentTransform = parent < 0
                ? glm::mat4(1.0f)
                : result[parent];

            result[i] = parentTransform * localPose[i].ToMatrix();

            for (int c = 0; c < 4; ++c)
            {
                for (int r = 0; r < 4; ++r)
                {
                    if (!std::isfinite(result[i][c][r]))
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
}
