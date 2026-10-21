#include "epch.h"
#include "BoneMask.h"

namespace Elixir
{
    Ref<BoneMask> BoneMask::Create(
        std::string name,
        Ref<const Skeleton> skeleton,
        std::vector<float> weights
    )
    {
        const auto invalidWeight = std::ranges::any_of(weights, [](const float weight)
        {
            return !std::isfinite(weight) || weight < 0.0f || weight > 1.0f;
        });

        if (name.empty() ||
            !skeleton ||
            weights.size() != skeleton->GetBones().size() ||
            invalidWeight)
        {
            EE_CORE_ERROR("BoneMask requires a name, skeleton, and valid weights for every bone.")
            return nullptr;
        }

        return Ref<BoneMask>(new BoneMask(
            std::move(name),
            std::move(skeleton),
            std::move(weights)
        ));
    }

    Ref<BoneMask> BoneMask::FromBone(
        std::string name,
        Ref<const Skeleton> skeleton,
        const std::string_view boneName,
        const float weight,
        const bool includeDescendants
    )
    {
        const auto root = skeleton ? skeleton->FindBone(boneName) : -1;

        if (root < 0)
        {
            EE_CORE_ERROR("BoneMask requires an existing bone named '{}'.", boneName)
            return nullptr;
        }

        const auto& bones = skeleton->GetBones();
        std::vector weights(bones.size(), 0.0f);

        weights[size_t(root)] = weight;

        if (includeDescendants)
        {
            for (size_t i = size_t(root) + 1; i < bones.size(); ++i)
                if (bones[i].ParentIndex >= 0)
                    weights[i] = weights[size_t(bones[i].ParentIndex)];
        }

        return Create(
            std::move(name),
            std::move(skeleton),
            std::move(weights)
        );
    }

    BoneMask::BoneMask(
        std::string name,
        Ref<const Skeleton> skeleton,
        std::vector<float> weights
    ) : m_Name(std::move(name)),
        m_Skeleton(std::move(skeleton)),
        m_Weights(std::move(weights))
    {
        m_HasInfluence = std::ranges::any_of(
            m_Weights,
            [](const float weight) { return weight > 0.0f; }
        );
    }
}
