#include "epch.h"
#include "BlendSpace.h"

#include <memory_resource>

namespace Elixir
{
    BlendSpace::BlendSpace(
        std::string name,
        Ref<const Skeleton> skeleton,
        const size_t dimensionCount,
        Ref<const AnimationSequence> synchronizationSequence
    ) : m_Name(std::move(name)),
        m_Skeleton(std::move(skeleton)),
        m_DimensionCount(dimensionCount),
        m_SynchronizationSequence(std::move(synchronizationSequence)) {}

    BlendSpace::~BlendSpace() = default;

    bool BlendSpace::HasValidCoordinates(const std::span<const float> values) const
    {
        const auto allValuesFinite = std::ranges::all_of(
            values,
            [](const float value) { return std::isfinite(value); }
        );
        return values.size() == m_DimensionCount && allValuesFinite;
    }

    const AnimationSequence* BlendSpace::GetNotifySource(
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

        std::pmr::vector<SContribution> samples(&resource);
        FindSamples(values, samples);

        const AnimationSequence* source = nullptr;
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

        std::pmr::vector<SContribution> samples(&resource);
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

    bool BlendSpace::Sample(
        const std::span<const float> values,
        const float phase,
        std::vector<SBoneTransform>& output,
        const std::span<const SBoneTransform> referencePose
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

        std::pmr::vector<SContribution> samples(&resource);
        FindSamples(values, samples);

        std::vector<SBoneTransform> pose, next;

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
                pose = next;
            else
            {
                if (pose.size() != next.size())
                {
                    EE_CORE_ERROR("Blend-space samples must have matching pose sizes.")
                    return false;
                }

                const auto weight = float(sample.Weight / (total + sample.Weight));

                for (size_t bone = 0; bone < pose.size(); ++bone)
                    pose[bone] = SBoneTransform::Blend(pose[bone], next[bone], weight);
            }

            total += sample.Weight;
        }

        if (total <= 0)
        {
            EE_CORE_ERROR("Blend space requires a positive total sample weight.")
            return false;
        }

        for (const auto& transform : pose)
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
}
