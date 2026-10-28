#include "epch.h"
#include "BlendSpaceEvaluator.h"

namespace Elixir
{
    BlendSpaceEvaluator::BlendSpaceEvaluator(const BlendSpace& source)
        : m_Source(&source) {}

    Scope<AnimationEvaluator> BlendSpaceEvaluator::Clone() const
    {
        return CreateScope<BlendSpaceEvaluator>(*this);
    }

    bool BlendSpaceEvaluator::Configure(
        const std::span<const float> values,
        Ref<AnimationParameters>
    )
    {
        return SetInputs(values);
    }

    bool BlendSpaceEvaluator::Evaluate(
        const SAnimationEvaluationContext& context,
        SAnimationEvaluation& output
    )
    {
        output.Phase = context.Phase;
        output.Sources = GetState().Sources;

        return Sample(
            context.Phase,
            context.ReferencePose,
            output.Pose,
            output.Curves
        );
    }

    SAnimationNodeState BlendSpaceEvaluator::GetState() const
    {
        return {
            .Duration = m_Duration,
            .Continuous = true,
            .Sources = SSequenceSources{
                m_Samples,
                m_NotifySource,
                m_Source->GetSynchronizationSequence().get()
            }
        };
    }

    bool BlendSpaceEvaluator::Sample(
        const double phase,
        const SLocalPose& referencePose,
        SLocalPose& output,
        AnimationCurveValues& curves
    ) const
    {
        SLocalPose pose, next;
        AnimationCurveValues result, nextCurves;
        const auto* synchronization = m_Source->GetSynchronizationSequence().get();

        float total = 0;
        for (const auto& sample : m_Samples)
        {
            if (sample.Weight == 0) continue;

            const auto time = synchronization
                ? sample.Sequence->GetSynchronizedTime(float(phase), *synchronization)
                : float(phase * sample.Sequence->GetDuration());

            if (!sample.Sequence->Sample(time, next, referencePose) ||
                !sample.Sequence->SampleCurves(time, nextCurves))
                return false;

            if (total == 0)
            {
                pose = next;
                result = nextCurves;
            }
            else
            {
                if (pose.LocalTransforms.size() != next.LocalTransforms.size())
                    return false;

                const auto weight = sample.Weight / (total + sample.Weight);
                if (!result.Blend(nextCurves, weight))
                    return false;

                for (size_t bone = 0; bone < pose.LocalTransforms.size(); ++bone)
                {
                    pose.LocalTransforms[bone] = SBoneTransform::Blend(
                        pose.LocalTransforms[bone],
                        next.LocalTransforms[bone],
                        weight
                    );
                }
            }

            total += sample.Weight;
        }

        if (total <= 0) return false;

        output = std::move(pose);
        curves = std::move(result);

        return true;
    }

    bool BlendSpaceEvaluator::SetInputs(const std::span<const float> values)
    {
        if (!m_Samples.empty() && std::ranges::equal(values, m_Values))
            return true;

        std::vector<SSequenceContribution> samples;
        if (!m_Source->CalculateContributions(values, samples))
            return false;

        double duration = 0;
        float dominant = -1;
        const Sequence* notify = nullptr;

        for (const auto& sample : samples)
        {
            duration += double(sample.Sequence->GetDuration()) * sample.Weight;

            if (sample.Weight > dominant)
            {
                dominant = sample.Weight;
                notify = sample.Sequence;
            }
        }

        m_Values.assign(values.begin(), values.end());
        m_Samples = std::move(samples);
        m_Duration = float(duration);
        m_NotifySource = notify;

        return true;
    }
}
