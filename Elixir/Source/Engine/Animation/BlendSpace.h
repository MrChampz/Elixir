#pragma once

#include <Engine/Animation/Pose.h>
#include <Engine/Animation/Sequence.h>

namespace Elixir
{
    struct SSequenceContribution;

    /** Samples synchronized animation cycles using an arbitrary number of numeric axes. */
    class ELIXIR_API BlendSpace
    {
        friend class BlendSpaceEvaluator;

    public:
        virtual ~BlendSpace();

        /**
         * @brief Calculate and validate source contributions once for subsequent sampling.
         * @param values Finite input coordinates matching the space's dimension count.
         * @param output Receives normalized positive contributions on success.
         */
        bool CalculateContributions(
            std::span<const float> values,
            std::vector<SSequenceContribution>& output
        ) const;

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
            SLocalPose& output,
            const SLocalPose& referencePose = {}
        ) const;

        /**
         * @brief Sample weighted scalar tracks on the same marker timeline as the pose.
         * @param values One finite coordinate per axis.
         * @param phase Finite cycle phase, clamped to [0, 1].
         * @param output Receives blended values; unchanged on failure. Missing curves use zero.
         */
        bool SampleCurves(
            std::span<const float> values,
            float phase,
            AnimationCurveValues& output
        ) const;

        /**
        * @brief Check whether this asset uses a compatible skeleton.
        * @param skeleton Skeleton whose bone layout will receive sampled poses.
        */
        bool IsCompatibleWith(const Ref<const Skeleton>& skeleton) const;

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
        const Sequence* GetNotifySource(std::span<const float> values) const;

        /** @brief Get the fixed marker reference, or null for normalized sampling. */
        const Ref<const Sequence>& GetSynchronizationSequence() const
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
            Ref<const Sequence> synchronizationSequence = nullptr
        );

        /**
         * @brief Select the first marked sequence in a stable sample order.
         * @tparam TSamples Container of samples exposing a Sequence reference.
         * @param samples Validated samples with non-null sequences.
         * @return The fixed reference, or null when no clip has markers.
         */
        template<typename TSamples>
        static Ref<const Sequence> FindSynchronizationSequence(
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
            std::pmr::vector<SSequenceContribution>& output
        ) const = 0;

    private:
        /**
         * @brief Resolve and validate weighted clips for direct or cached sampling.
         * @param values Finite coordinates matching the asset dimensions.
         * @param output Candidate contributions; discard them on failure.
         */
        bool ResolveSamples(
            std::span<const float> values,
            std::pmr::vector<SSequenceContribution>& output
        ) const;

        /**
         * @brief Map a shared blend-space phase to one sequence's sampling time.
         * @param sequence Contributing clip compatible with this blend space.
         * @param phase Finite cycle phase, clamped to [0, 1].
         */
        float GetSampleTime(const Sequence& sequence, double phase) const;

        std::string m_Name;
        Ref<const Skeleton> m_Skeleton;
        size_t m_DimensionCount;
        Ref<const Sequence> m_SynchronizationSequence;
    };
}
