#include "epch.h"
#include "Skeleton.h"

#include <Engine/Animation/Pose.h>

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

    bool SBoneTransform::IsNearlyEqual(const SBoneTransform& other) const
    {
        constexpr auto tolerance = glm::vec3(BoneTransformTolerance);

        const auto translationDelta = glm::abs(Translation - other.Translation);
        const auto scaleDelta = glm::abs(Scale - other.Scale);
        const auto rotationAlignment = std::abs(glm::dot(Rotation, other.Rotation));

        const auto translationChanged =
            glm::any(glm::greaterThan(translationDelta, tolerance));
        const auto scaleChanged = glm::any(glm::greaterThan(scaleDelta, tolerance));
        const auto rotationChanged = rotationAlignment < 1.0f - BoneTransformTolerance;

        return !translationChanged && !scaleChanged && !rotationChanged;
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

    Ref<Skeleton> Skeleton::Create(std::string name, std::vector<SBone> bones)
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

        return Ref<Skeleton>(new Skeleton(
            std::move(name),
            std::move(bones)
        ));
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

    int32_t Skeleton::FindBone(const std::string_view name) const
    {
        for (size_t i = 0; i < m_Bones.size(); ++i)
            if (m_Bones[i].Name == name)
                return int32_t(i);

        return -1;
    }

    Skeleton::Skeleton(std::string name, std::vector<SBone> bones)
      : m_Name(std::move(name)),
        m_Bones(std::move(bones)) {}
}
