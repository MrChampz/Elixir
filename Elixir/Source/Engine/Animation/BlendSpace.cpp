#include "epch.h"
#include "BlendSpace.h"

#include <Engine/Animation/Pose.h>
#include <Engine/Animation/AnimationEvaluation.h>

#include <memory_resource>

namespace Elixir
{
    BlendSpace::~BlendSpace() = default;

    bool BlendSpace::CalculateContributions(
        const std::span<const float> values,
        std::vector<SSequenceContribution>& output
    ) const
    {
        if (!HasValidCoordinates(values))
            return false;

        std::pmr::vector<SSequenceContribution> samples;
        FindSamples(values, samples);

        float total = 0;
        for (const auto& sample : samples)
        {
            if (!sample.Sequence ||
                !std::isfinite(sample.Weight) ||
                sample.Weight < 0 ||
                !sample.Sequence->IsCompatibleWith(m_Skeleton))
            {
                EE_CORE_ERROR("Blend space produced an invalid source contribution.")
                return false;
            }

            total += sample.Weight;
        }

        if (!std::isfinite(total) || total <= 0)
            return false;

        std::vector<SSequenceContribution> resolved;
        resolved.reserve(samples.size());

        for (const auto& sample : samples)
        {
            if (sample.Weight > 0)
                resolved.push_back({
                    sample.Sequence,
                    sample.Weight / total
                });
        }

        output = std::move(resolved);
        return true;
    }

    bool BlendSpace::Sample(
        const std::span<const float> values,
        const float phase,
        SLocalPose& output,
        const SLocalPose& referencePose
    ) const
    {
        if (!HasValidCoordinates(values) || !std::isfinite(phase))
        {
            EE_CORE_ERROR("Blend-space coordinates and phase must be valid and finite.")
            return false;
        }

        // Keep common sample counts on the stack; larger spaces use the allocator fallback.
        std::array<std::byte, 256> storage;
        std::pmr::monotonic_buffer_resource resource(storage.data(), storage.size());

        std::pmr::vector<SSequenceContribution> samples(&resource);
        FindSamples(values, samples);

        SLocalPose pose, next;

        double total = 0;
        for (const auto& sample : samples)
        {
            if (!sample.Sequence || !std::isfinite(sample.Weight) || sample.Weight < 0)
            {
                EE_CORE_ERROR("Blend space produced an invalid contribution.")
                return false;
            }

            if (sample.Weight == 0) continue;

            const auto time = m_SynchronizationSequence
                ? sample.Sequence->GetSynchronizedTime(phase, *m_SynchronizationSequence)
                : std::clamp(phase, 0.0f, 1.0f) * sample.Sequence->GetDuration();

            const auto sampled = sample.Sequence->Sample(
                time,
                next,
                referencePose
            );

            if (!sampled) return false;

            if (total == 0)
            {
                pose = next;
            }
            else
            {
                if (pose.LocalTransforms.size() != next.LocalTransforms.size())
                {
                    EE_CORE_ERROR("Blend-space samples must have matching pose sizes.")
                    return false;
                }

                const auto weight = float(sample.Weight / (total + sample.Weight));

                for (size_t bone = 0; bone < pose.LocalTransforms.size(); ++bone)
                    pose.LocalTransforms[bone] = SBoneTransform::Blend(
                        pose.LocalTransforms[bone],
                        next.LocalTransforms[bone],
                        weight
                    );
            }

            total += sample.Weight;
        }

        if (total <= 0)
        {
            EE_CORE_ERROR("Blend space requires a positive total sample weight.")
            return false;
        }

        for (const auto& transform : pose.LocalTransforms)
        {
            if (!transform.IsValid())
            {
                EE_CORE_ERROR("Blend space produced a singular or invalid transform.")
                return false;
            }
        }

        output = std::move(pose);
        return true;
    }

    bool BlendSpace::IsCompatibleWith(const Ref<const Skeleton>& skeleton) const
    {
        return skeleton && m_Skeleton &&
            (skeleton == m_Skeleton || skeleton->IsCompatible(*m_Skeleton));
    }

    bool BlendSpace::ExtractRootMotion(
        const std::span<const float> values,
        double startPhase,
        double endPhase,
        const bool looping,
        const SBoneTransform& reference,
        SRootMotionDelta& output,
        const uint32_t boneIndex
    ) const
    {
        if (!HasValidCoordinates(values) ||
            !std::isfinite(startPhase) ||
            !std::isfinite(endPhase) ||
            endPhase < startPhase ||
            !reference.IsValid())
        {
            EE_CORE_ERROR("Blend-space root motion requires valid coordinates and phases.")
            return false;
        }

        if (!looping)
        {
            startPhase = std::clamp(startPhase, 0.0, 1.0);
            endPhase = std::clamp(endPhase, 0.0, 1.0);
        }

        std::array<std::byte, 256> storage;
        std::pmr::monotonic_buffer_resource resource(storage.data(), storage.size());

        std::pmr::vector<SSequenceContribution> samples(&resource);
        FindSamples(values, samples);

        SRootMotionDelta result;
        double total = 0;

        for (const auto& sample : samples)
        {
            if (!sample.Sequence || !std::isfinite(sample.Weight) || sample.Weight < 0)
            {
                EE_CORE_ERROR("Blend space produced an invalid root-motion contribution.")
                return false;
            }

            if (sample.Weight == 0) continue;

            const auto start = m_SynchronizationSequence
                ? sample.Sequence->GetSynchronizedPhase(startPhase, *m_SynchronizationSequence)
                : startPhase;

            const auto end = m_SynchronizationSequence
                ? sample.Sequence->GetSynchronizedPhase(endPhase, *m_SynchronizationSequence)
                : endPhase;

            SRootMotionDelta delta;
            const auto success = sample.Sequence->ExtractRootMotion(
                start,
                end,
                true,
                reference,
                delta,
                boneIndex
            );
            if (!success) return false;

            result = total == 0
                ? delta
                : SRootMotionDelta::Blend(
                    result,
                    delta,
                    float(sample.Weight / (total + sample.Weight))
                  );

            total += sample.Weight;
        }

        if (total <= 0 || !result.IsValid())
        {
            EE_CORE_ERROR("Blend-space root motion requires a positive finite contribution.")
            return false;
        }

        output = result;
        return true;
    }

    float BlendSpace::GetDuration(const std::span<const float> values) const
    {
        if (!HasValidCoordinates(values))
        {
            EE_CORE_ERROR("Blend-space coordinates must match the axes and be finite.")
            return 0.0f;
        }

        // Keep common sample counts on the stack; larger spaces use the allocator fallback.
        std::array<std::byte, 256> storage;
        std::pmr::monotonic_buffer_resource resource(storage.data(), storage.size());

        std::pmr::vector<SSequenceContribution> samples(&resource);
        FindSamples(values, samples);

        double duration = 0, total = 0, max = 0;

        for (const auto& sample : samples)
        {
            if (!sample.Sequence || !std::isfinite(sample.Weight) || sample.Weight < 0)
            {
                EE_CORE_ERROR("Blend space produced an invalid contribution.")
                return 0.0f;
            }

            const auto seconds = double(sample.Sequence->GetDuration());
            duration += seconds * sample.Weight;
            total += sample.Weight;

            max = std::max(max, seconds);
        }

        if (total <= 0)
        {
            EE_CORE_ERROR("Blend space requires a positive total sample weight.")
            return 0.0f;
        }

        return float(std::min(duration / total, max));
    }

    bool BlendSpace::HasValidCoordinates(const std::span<const float> values) const
    {
        const auto allValuesFinite = std::ranges::all_of(
            values,
            [](const float value) { return std::isfinite(value); }
        );
        return values.size() == m_DimensionCount && allValuesFinite;
    }

    const Sequence* BlendSpace::GetNotifySource(
        const std::span<const float> values
    ) const
    {
        if (!HasValidCoordinates(values))
        {
            EE_CORE_ERROR("Notify source coordinates must match the blend-space axes.")
            return nullptr;
        }

        std::array<std::byte, 256> storage;
        std::pmr::monotonic_buffer_resource resource(storage.data(), storage.size());

        std::pmr::vector<SSequenceContribution> samples(&resource);
        FindSamples(values, samples);

        const Sequence* source = nullptr;
        float weight = 0.0f;

        for (const auto& sample : samples)
        {
            if (!sample.Sequence || !std::isfinite(sample.Weight) || sample.Weight < 0.0f)
            {
                EE_CORE_ERROR("Blend space produced an invalid notify contribution.")
                return nullptr;
            }
            if (sample.Weight > weight)
            {
                source = sample.Sequence;
                weight = sample.Weight;
            }
        }

        if (!source)
            EE_CORE_ERROR("Blend space requires a positive notify source weight.")

        return source;
    }

    BlendSpace::BlendSpace(
        std::string name,
        Ref<const Skeleton> skeleton,
        const size_t dimensionCount,
        Ref<const Sequence> synchronizationSequence
    ) : m_Name(std::move(name)),
        m_Skeleton(std::move(skeleton)),
        m_DimensionCount(dimensionCount),
        m_SynchronizationSequence(std::move(synchronizationSequence)) {}
}
