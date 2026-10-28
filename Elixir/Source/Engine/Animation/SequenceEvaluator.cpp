#include "epch.h"
#include "SequenceEvaluator.h"

namespace Elixir
{
    SequenceEvaluator::SequenceEvaluator(const Sequence& source)
      : m_Source(&source),
        m_Contribution{ &source, 1 } {}

    Scope<AnimationEvaluator> SequenceEvaluator::Clone() const
    {
        return CreateScope<SequenceEvaluator>(*this);
    }

    bool SequenceEvaluator::Configure(
        const std::span<const float> values,
        Ref<AnimationParameters>
    )
    {
        return values.empty();
    }

    bool SequenceEvaluator::Evaluate(
        const SAnimationEvaluationContext& context,
        SAnimationEvaluation& output
    )
    {
        output.Phase = context.Phase;
        output.Sources = GetState().Sources;

        const auto time = float(context.Phase * m_Source->GetDuration());

        const auto sampled = m_Source->Sample(
            time,
            output.Pose,
            context.ReferencePose
        );

        const auto curvesSampled = m_Source->SampleCurves(time, output.Curves);

        return sampled && curvesSampled;
    }

    SAnimationNodeState SequenceEvaluator::GetState() const
    {
        return {
            .Duration = m_Source->GetDuration(),
            .Sources = SSequenceSources{
                std::span(&m_Contribution, 1),
                m_Source,
                nullptr
            }
        };
    }
}
