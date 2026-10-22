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
         * @brief Sample weighted local poses at a shared cycle phase.
         *
         * Matching marker tracks align contacts using a fixed reference sequence. Clips with
         * missing or incompatible tracks retain normalized time. Changing weights never
         * changes the reference timeline. Marker intervals wrap across the cycle boundary.
         *
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
         * @brief Blend root movement from contributing clips on the marker timeline.
         * @param values One finite coordinate per axis.
         * @param startPhase Finite start cycle phase.
         * @param endPhase Finite unwrapped forward end phase.
         * @param looping Accumulate cycles; otherwise clamp the reference phase to [0, 1].
         * @param reference Mesh-specific local reference transform of the selected bone.
         * @param output Receives blended parent-local movement; unchanged on failure.
         * @param boneIndex Bone to extract; its ancestors must remain in the reference pose.
         */
        bool ExtractRootMotion(
            std::span<const float> values,
            double startPhase,
            double endPhase,
            bool looping,
            const SBoneTransform& reference,
            SRootMotionDelta& output,
            uint32_t boneIndex = 0
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

        /**
         * @brief Select the highest-weight clip as the sole notify source.
         *
         * Equal weights retain the concrete sample order. A dominant clip without notifies
         * suppresses events rather than falling back to another contributing clip.
         *
         * @param values One finite coordinate per axis.
         * @return Borrowed clip, or null for invalid coordinates or contributions.
         * @pre Keep this blend space alive while using the returned pointer.
         */
        const AnimationSequence* GetNotifySource(std::span<const float> values) const;

        /** @brief Get the fixed marker reference, or null for normalized sampling. */
        const Ref<const AnimationSequence>& GetSynchronizationSequence() const
        {
            return m_SynchronizationSequence;
        }

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
         * @param synchronizationSequence Fixed marker reference; null uses normalized time.
         */
        BlendSpace(
            std::string name,
            Ref<const Skeleton> skeleton,
            size_t dimensionCount,
            Ref<const AnimationSequence> synchronizationSequence = nullptr
        );

        /**
         * @brief Select the first marked sequence in a stable sample order.
         * @tparam TSamples Container of samples exposing a Sequence reference.
         * @param samples Validated samples with non-null sequences.
         * @return The fixed reference, or null when no clip has markers.
         */
        template<typename TSamples>
        static Ref<const AnimationSequence> FindSynchronizationSequence(
            const TSamples& samples
        )
        {
            for (const auto& sample : samples)
            {
                if (!sample.Sequence->GetSyncMarkers().empty())
                    return sample.Sequence;
            }
            return nullptr;
        }

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
        Ref<const AnimationSequence> m_SynchronizationSequence;
    };
}
