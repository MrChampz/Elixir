#include "epch.h"
#include "StateMachineEvaluator.h"

#include <Engine/Animation/Pose.h>

namespace Elixir
{
    StateMachineEvaluator::StateMachineEvaluator(const StateMachine& source)
      : m_Source(&source),
        m_State(source.GetInitialState())
    {
        const auto& state = source.GetCreateInfo().States[m_State];
        std::vector<float> values(state.Node->GetInputCount(), 0);
        m_Playback = AnimationPlayback(state.Node, values, state.Looping);
    }

    Scope<AnimationNodeEvaluator> StateMachineEvaluator::Clone() const
    {
        auto copy = CreateScope<StateMachineEvaluator>(*this);
        copy->m_NextPlayback.reset();
        copy->m_NextTransition.reset();
        copy->m_Prepared = false;
        return copy;
    }

    bool StateMachineEvaluator::Configure(
        const std::span<const float> values,
        Ref<AnimationParameters> parameters
    )
    {
        if (!values.empty()) return false;

        if (parameters == m_Parameters) return true;

        if (!parameters || !m_Source->CanBind(*parameters, {}))
            return false;

        std::vector<std::vector<size_t>> conditions;

        for (const auto& transition : m_Source->GetCreateInfo().Transitions)
        {
            auto& indices = conditions.emplace_back();
            indices.reserve(transition.Conditions.size());

            for (const auto& condition : transition.Conditions)
            {
                const auto index = parameters->Find(condition.Parameter);
                if (!index) return false;

                indices.push_back(*index);
            }
        }

        m_Parameters = std::move(parameters);
        m_ConditionBindings = std::move(conditions);

        return CreatePlayback(m_State, m_Playback);
    }

    bool StateMachineEvaluator::Evaluate(
        const SAnimationEvaluationContext& context,
        SAnimationEvaluation& output
    )
    {
        output.Phase = context.Advance
            ? Prepare(context)
            : context.Phase;

        if (!std::isfinite(output.Phase))
            return false;

        output.Sources = GetState().Sources;

        return Sample(output.Phase, context.ReferencePose, output.Pose);
    }



    void StateMachineEvaluator::Apply(const double phase)
    {
        if (!m_Prepared) return;

        if (m_NextPlayback)
            m_Playback = std::move(*m_NextPlayback);

        if (m_NextTransition)
            m_Transition = std::move(*m_NextTransition);

        m_Playback.Apply(m_NextPhase);
        m_Transition.Playback.Apply(m_OutgoingPhase);
        m_Transition.Elapsed = m_NextElapsed;

        if (m_Transition.Elapsed >= m_Transition.Duration)
            m_Transition = {};

        m_State = m_NextState;

        std::swap(m_VisiblePose, m_NextPose);

        m_NextPlayback.reset();
        m_NextTransition.reset();

        m_Prepared = false;
    }

    bool StateMachineEvaluator::Reset(const double phase)
    {
        if (!m_Playback.SeekPhase(phase))
            return false;

        m_Transition = {};
        m_NextPlayback.reset();
        m_NextTransition.reset();
        m_Prepared = false;

        return true;
    }

    SAnimationNodeState StateMachineEvaluator::GetState() const
    {
        return {
            .Duration = m_Playback.GetDuration(),
            .Continuous = true,
            .Finished = false,
            .Sources = SAnimationSelectedSources{
                &Target(), &Transition(), m_NextPlayback.has_value()
            },
            .PrimaryPlayback = &m_Playback,
            .StateName = m_Source->GetCreateInfo().States[m_State].Name,
            .Transitioning = m_Transition.IsActive()
        };
    }

    double StateMachineEvaluator::Prepare(const SAnimationEvaluationContext& context)
    {
        const auto dt = context.DeltaSeconds;
        const auto& referencePose = context.ReferencePose;

        m_Prepared = false;
        m_NextPlayback.reset();
        m_NextTransition.reset();
        m_NextState = m_State;

        const auto& info = m_Source->GetCreateInfo();
        for (const auto index : m_Source->GetTransitions(m_State))
        {
            const auto& transition = info.Transitions[index];
            if (transition.WaitForCompletion &&
                (!m_Playback.IsFinished() || m_Transition.IsActive()))
                continue;

            if (!Matches(index)) continue;

            m_NextState = m_Source->GetTarget(index);
            AnimationPlayback next;

            if (!CreatePlayback(m_NextState, next))
                return std::numeric_limits<double>::quiet_NaN();

            AnimationTransition fade;
            fade.Duration = transition.FadeDuration;

            if (fade.IsActive())
            {
                if (!m_Transition.IsActive())
                    fade.Playback = m_Playback;
                else
                    fade.Pose = m_VisiblePose;
            }

            m_NextPlayback = std::move(next);
            m_NextTransition = std::move(fade);

            break;
        }

        auto& target = m_NextPlayback ? *m_NextPlayback : m_Playback;
        auto& fade = m_NextTransition ? *m_NextTransition : m_Transition;

        target.SetNotifiesEnabled(context.NotifiesEnabled);
        fade.Playback.SetNotifiesEnabled(false);

        m_NextPhase = target.Advance(dt, referencePose);
        m_OutgoingPhase = fade.Playback.Advance(dt, referencePose);
        m_NextElapsed = std::min(fade.Elapsed + dt, fade.Duration);

        const auto weight = fade.IsActive()
            ? m_NextElapsed / fade.Duration
            : 1;

        const auto sampled = fade.Sample(
            target,
            fade.Playback,
            weight,
            m_NextPhase,
            m_OutgoingPhase,
            m_NextPose,
            referencePose
        );

        if (!sampled)
            return std::numeric_limits<double>::quiet_NaN();

        m_Prepared = true;

        return m_NextPhase;
    }

    bool StateMachineEvaluator::Sample(
        const double phase,
        const SLocalPose& referencePose,
        SLocalPose& output
    ) const
    {
        if (!std::isfinite(phase)) return false;

        if (m_Prepared)
        {
            output = m_NextPose;
            return true;
        }

        return m_Playback.Sample(phase, referencePose, output);
    }

    bool StateMachineEvaluator::CreatePlayback(
        const size_t stateIndex,
        AnimationPlayback& output
    ) const
    {
        const auto& state = m_Source->GetCreateInfo().States[stateIndex];

        std::vector<float> values;
        std::vector<size_t> bindings;

        for (const auto& name : state.InputParameters)
        {
            const auto value = m_Parameters
                ? m_Parameters->GetFloat(name)
                : std::nullopt;

            const auto index = m_Parameters
                ? m_Parameters->Find(name)
                : std::nullopt;

            if (!value || !index) return false;

            values.push_back(*value);
            bindings.push_back(*index);
        }

        AnimationPlayback playback(state.Node, values, state.Looping);
        if (!playback.IsValid() || !playback.BindParameters(m_Parameters, bindings))
            return false;

        output = std::move(playback);
        return true;
    }


    bool StateMachineEvaluator::Matches(const size_t transition) const
    {
        const auto& conditions =
            m_Source->GetCreateInfo().Transitions[transition].Conditions;

        if (conditions.empty()) return true;

        if (!m_Parameters || transition >= m_ConditionBindings.size())
            return false;

        const auto& bindings = m_ConditionBindings[transition];
        if (bindings.size() != conditions.size()) return false;

        for (size_t i = 0; i < conditions.size(); ++i)
            if (!m_Parameters->Matches(bindings[i], conditions[i]))
                return false;

        return true;
    }

    const AnimationPlayback& StateMachineEvaluator::Target() const
    {
        return m_NextPlayback ? *m_NextPlayback : m_Playback;
    }

    const AnimationTransition& StateMachineEvaluator::Transition() const
    {
        return m_NextTransition ? *m_NextTransition : m_Transition;
    }
}
