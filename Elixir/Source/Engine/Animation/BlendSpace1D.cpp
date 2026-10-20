#include "epch.h"
#include "BlendSpace1D.h"

namespace Elixir
{
    Ref<BlendSpace1D> BlendSpace1D::Create(
        std::string name,
        std::vector<SBlendSpace1DSample> samples
    )
    {
        if (name.empty() || samples.empty())
        {
            EE_CORE_ERROR("BlendSpace1D requires a name and at least one sample.")
            return nullptr;
        }

        Ref<const Skeleton> skeleton;
        for (const auto& sample : samples)
        {
            if (!std::isfinite(sample.Position) ||
                !sample.Sequence ||
                (skeleton && skeleton != sample.Sequence->GetSkeleton() &&
                    !skeleton->IsCompatible(*sample.Sequence->GetSkeleton())))
            {
                EE_CORE_ERROR("BlendSpace1D requires finite positions and compatible animations.")
                return nullptr;
            }

            skeleton = sample.Sequence->GetSkeleton();
        }

        std::ranges::sort(samples, {}, &SBlendSpace1DSample::Position);

        for (size_t i = 1; i < samples.size(); ++i)
        {
            if (samples[i - 1].Position == samples[i].Position)
            {
                EE_CORE_ERROR("BlendSpace1D sample positions must be unique.")
                return nullptr;
            }
        }

        return Ref<BlendSpace1D>(new BlendSpace1D(
            std::move(name),
            std::move(samples)
        ));
    }

    BlendSpace1D::BlendSpace1D(std::string name, std::vector<SBlendSpace1DSample> samples)
        : m_Name(std::move(name)),
          m_Skeleton(samples.front().Sequence->GetSkeleton()),
          m_Samples(std::move(samples)) {}

    BlendSpace1D::SSegment BlendSpace1D::FindSegment(const float value) const
    {
        const auto upper = std::ranges::upper_bound(
            m_Samples,
            value,
            {},
            &SBlendSpace1DSample::Position
        );

        if (upper == m_Samples.begin())
            return { 0, 0, 0.0f };

        if (upper == m_Samples.end())
            return { m_Samples.size() - 1, m_Samples.size() - 1, 0.0f };

        const auto index = size_t(upper - m_Samples.begin());
        const auto lowerPosition = double(m_Samples[index - 1].Position);

        const auto weight = float((double(value) - lowerPosition) /
            (double(upper->Position) - lowerPosition));

        return { index - 1, index, weight };
    }

    float BlendSpace1D::GetDuration(const float value) const
    {
        if (!std::isfinite(value))
        {
            EE_CORE_ERROR("BlendSpace1D coordinate must be finite.")
            return 0.0f;
        }

        const auto segment = FindSegment(value);
        const auto lower = double(m_Samples[segment.Lower].Sequence->GetDuration());
        const auto upper = double(m_Samples[segment.Upper].Sequence->GetDuration());

        return float(lower + (upper - lower) * segment.Weight);
    }

    bool BlendSpace1D::Sample(
        const float value,
        const float phase,
        std::vector<SBoneTransform>& output,
        const std::span<const SBoneTransform> referencePose
    ) const
    {
        if (!std::isfinite(value) || !std::isfinite(phase))
        {
            EE_CORE_ERROR("BlendSpace1D coordinate and phase must be finite.")
            return false;
        }

        const auto segment = FindSegment(value);
        const auto& lower = m_Samples[segment.Lower].Sequence;
        const auto normalized = std::clamp(phase, 0.0f, 1.0f);

        std::vector<SBoneTransform> pose;
        if (!lower->Sample(normalized * lower->GetDuration(), pose, referencePose))
            return false;

        if (segment.Weight > 0.0f)
        {
            const auto& upper = m_Samples[segment.Upper].Sequence;

            std::vector<SBoneTransform> target;
            if (!upper->Sample(normalized * upper->GetDuration(), target, referencePose))
                return false;

            for (size_t i = 0; i < pose.size(); ++i)
            {
                pose[i] = SBoneTransform::Blend(pose[i], target[i], segment.Weight);
                if (!pose[i].IsValid())
                {
                    EE_CORE_ERROR("BlendSpace1D produced a singular or invalid transform.")
                    return false;
                }
            }
        }

        output = std::move(pose);
        return true;
    }
}
