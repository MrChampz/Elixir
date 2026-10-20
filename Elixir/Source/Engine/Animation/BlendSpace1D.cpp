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
            if (!std::isfinite(sample.Position) || !sample.Sequence ||
                (skeleton && skeleton != sample.Sequence->GetSkeleton() &&
                    !skeleton->IsCompatible(*sample.Sequence->GetSkeleton())))
            {
                EE_CORE_ERROR(
                    "BlendSpace1D requires finite positions and compatible animations."
                )
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

    bool BlendSpace1D::Sample(
        const float value,
        const float phase,
        std::vector<SBoneTransform>& output,
        const std::span<const SBoneTransform> referencePose
    ) const
    {
        return BlendSpace::Sample(
            std::span(&value, 1),
            phase,
            output,
            referencePose
        );
    }

    float BlendSpace1D::GetDuration(const float value) const
    {
        return BlendSpace::GetDuration(std::span(&value, 1));
    }

    void BlendSpace1D::FindSamples(
        const std::span<const float> values,
        std::pmr::vector<SContribution>& output
    ) const
    {
        const auto segment = FindSegment(values[0]);
        output = {
            { m_Samples[segment.Lower].Sequence.get(), 1.0f - segment.Weight },
            { m_Samples[segment.Upper].Sequence.get(), segment.Weight },
        };
    }

    BlendSpace1D::BlendSpace1D(std::string name, std::vector<SBlendSpace1DSample> samples)
      : BlendSpace(
            std::move(name),
            samples.front().Sequence->GetSkeleton(),
            1,
            FindSynchronizationSequence(samples)
        ),
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
}
