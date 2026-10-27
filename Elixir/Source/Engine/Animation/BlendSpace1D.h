#pragma once

#include <Engine/Animation/BlendSpace.h>

namespace Elixir
{
    /** Places an animation at a coordinate along a blend-space axis. */
    struct SBlendSpace1DSample
    {
        /** Finite coordinate, such as a character's movement speed. */
        float Position = 0.0f;
        Ref<const AnimationSequence> Sequence;
    };

    /** Stores immutable animation samples blended along one numeric axis. */
    class ELIXIR_API BlendSpace1D final : public BlendSpace
    {
    public:
        BlendSpace1D(const BlendSpace1D&) = delete;
        BlendSpace1D& operator=(const BlendSpace1D&) = delete;

        /**
         * @brief Create a blend space with unique coordinates and compatible animations.
         * @param name Asset name.
         * @param samples At least one sample; creation sorts samples by position.
         * @return Null if a name, coordinate, or animation is invalid.
         */
        static Ref<BlendSpace1D> Create(
            std::string name,
            std::vector<SBlendSpace1DSample> samples
        );

        /**
         * @brief Sample the nearest animations at the same normalized cycle phase.
         *
         * Blend the two neighboring samples and clamp coordinates outside the sample range.
         * Authored cycles should place matching foot contacts at matching normalized phases.
         * Leave output unchanged if sampling produces an invalid transform.
         *
         * @param value Finite coordinate along the blend axis.
         * @param phase Finite normalized cycle time, clamped to [0, 1].
         * @param output Full parent-relative pose to receive the result.
         * @param referencePose Optional mesh defaults for channels without animation keys.
         */
        bool Sample(
            float value,
            float phase,
            std::vector<SBoneTransform>& output,
            std::span<const SBoneTransform> referencePose = {}
        ) const;

        /**
         * @brief Get the interpolated cycle duration at a coordinate, in seconds.
         * @param value Finite coordinate, clamped to the sample range.
         * @return Zero for static samples or invalid input.
         */
        float GetDuration(float value) const;

        /** @brief Get samples in ascending coordinate order. */
        const std::vector<SBlendSpace1DSample>& GetSamples() const { return m_Samples; }

        using BlendSpace::GetDuration;
        using BlendSpace::Sample;

    protected:
        /**
         * @brief Resolve weighted clips for validated coordinates.
         * @param values One finite coordinate per axis.
         * @param output Receives clips and their nonnegative weights.
         */
        void FindSamples(
            std::span<const float> values,
            std::pmr::vector<SContribution>& output
        ) const override;

    private:
        struct SSegment
        {
            size_t Lower;
            size_t Upper;
            float Weight;
        };

        /**
         * @brief Store validated samples in coordinate order.
         * @param name Asset name.
         * @param samples Nonempty sorted samples with compatible skeletons.
         */
        BlendSpace1D(std::string name, std::vector<SBlendSpace1DSample> samples);

        /**
         * @brief Locate the samples surrounding a validated coordinate.
         * @param value Finite coordinate along the blend axis.
         */
        SSegment FindSegment(float value) const;

        std::vector<SBlendSpace1DSample> m_Samples;
    };
}
