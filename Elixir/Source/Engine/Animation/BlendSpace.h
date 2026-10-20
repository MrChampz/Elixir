#pragma once

#include <Engine/Animation/AnimationSequence.h>

namespace Elixir
{
    /** Samples synchronized animation cycles using an arbitrary number of numeric axes. */
    class ELIXIR_API BlendSpace
    {
    public:
        virtual ~BlendSpace();

        /**
         * @brief Sample weighted local poses at a shared normalized cycle phase.
         * @param values One finite coordinate per axis, in axis order.
         * @param phase Finite cycle phase, clamped to [0, 1].
         * @param output Receives the pose; unchanged on failure.
         * @param referencePose Optional defaults for channels without animation keys.
         */
        bool Sample(
            std::span<const float> values,
            float phase,
            std::vector<SBoneTransform>& output,
            std::span<const SBoneTransform> referencePose = {}
        ) const;

        /**
         * @brief Get the weighted cycle duration in seconds, or zero on failure.
         * @param values One finite coordinate per axis, in axis order.
         */
        float GetDuration(std::span<const float> values) const;

        /**
         * @brief Check that coordinates match the axis count and contain finite values.
         * @param values Coordinates to validate without evaluating sample weights.
         */
        bool HasValidCoordinates(std::span<const float> values) const;

        /** @brief Get the number of coordinates required by this asset. */
        size_t GetDimensionCount() const { return m_DimensionCount; }

        /** @brief Get the asset name. */
        const std::string& GetName() const { return m_Name; }

        /** @brief Get the compatible skeleton shared by all contributing animations. */
        const Ref<const Skeleton>& GetSkeleton() const { return m_Skeleton; }

    protected:
        /** Borrows a sample animation and gives its nonnegative contribution. */
        struct SContribution
        {
            const AnimationSequence* Sequence;
            float Weight;
        };

        /**
         * @brief Store metadata validated by the concrete asset factory.
         * @param name Asset name.
         * @param skeleton Compatible skeleton shared by all samples.
         * @param dimensionCount Positive number of input coordinates.
         */
        BlendSpace(std::string name, Ref<const Skeleton> skeleton, size_t dimensionCount);

        /**
         * @brief Resolve contributing samples using the concrete asset's geometry.
         * @param values Validated coordinates, one per axis.
         * @param output Receives borrowed sequences with finite nonnegative weights.
         * @pre Contributions must contain at least one positive weight and compatible clips.
         */
        virtual void FindSamples(
            std::span<const float> values,
            std::pmr::vector<SContribution>& output
        ) const = 0;

    private:
        std::string m_Name;
        Ref<const Skeleton> m_Skeleton;
        size_t m_DimensionCount;
    };
}
