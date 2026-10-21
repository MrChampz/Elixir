#pragma once

#include <Engine/Animation/Skeleton.h>

namespace Elixir
{
    /** Stores immutable local-pose blend weights in skeleton bone order. */
    class ELIXIR_API BoneMask final
    {
    public:
        /**
         * @brief Keep shared mask assets immutable after creation.
         * @param other Mask that cannot be copied.
         */
        BoneMask(const BoneMask& other) = delete;

        /**
         * @brief Prevent replacing a validated mask used by existing layers.
         * @param other Mask that cannot replace this asset.
         */
        BoneMask& operator=(const BoneMask& other) = delete;

        /**
         * @brief Create a mask with one finite weight in [0, 1] per bone.
         * @param name Asset name.
         * @param skeleton Skeleton defining the weight indices.
         * @param weights Bone weights; zero preserves the underlying pose.
         * @return Null for invalid input.
         */
        static Ref<BoneMask> Create(
            std::string name,
            Ref<const Skeleton> skeleton,
            std::vector<float> weights
        );

        /**
         * @brief Create a mask selecting a bone and optionally all its descendants.
         * @param name Asset name.
         * @param skeleton Skeleton containing the selected bone.
         * @param boneName Name of the first selected bone.
         * @param weight Finite weight in [0, 1] for selected bones; others receive zero.
         * @param includeDescendants Select the complete subtree when true.
         * @return Null for an unknown bone or invalid input.
         */
        static Ref<BoneMask> FromBone(
            std::string name,
            Ref<const Skeleton> skeleton,
            std::string_view boneName,
            float weight = 1.0f,
            bool includeDescendants = true
        );

        /** @brief Get the asset name. */
        const std::string& GetName() const { return m_Name; }

        /** @brief Get the skeleton defining bone indices. */
        const Ref<const Skeleton>& GetSkeleton() const { return m_Skeleton; }

        /** @brief Get immutable weights in bone order. */
        const std::vector<float>& GetWeights() const { return m_Weights; }

        /** @brief Check whether at least one bone has positive weight. */
        bool HasInfluence() const { return m_HasInfluence; }

    private:
        /**
         * @brief Store a validated mask.
         * @param name Asset name.
         * @param skeleton Skeleton defining bone indices.
         * @param weights One validated weight per bone.
         */
        BoneMask(std::string name, Ref<const Skeleton> skeleton, std::vector<float> weights);

        std::string m_Name;
        Ref<const Skeleton> m_Skeleton;
        std::vector<float> m_Weights;
        bool m_HasInfluence = false;
    };
}
