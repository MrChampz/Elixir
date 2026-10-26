#include "epch.h"
#include "AnimationPlayback.h"

#include <Engine/Animation/AnimationNodeQuery.h>
#include <Engine/Animation/AnimationLayer.h>
#include <Engine/Animation/AnimationNotifyTracker.h>

namespace Elixir
{
    AnimationPlayback::AnimationPlayback() = default;
    AnimationPlayback::~AnimationPlayback() = default;
    AnimationPlayback::AnimationPlayback(AnimationPlayback&& other) noexcept = default;
    AnimationPlayback& AnimationPlayback::operator=(AnimationPlayback&& other) noexcept = default;

    AnimationPlayback::AnimationPlayback(
        Ref<const AnimationNode> source,
        const std::span<const float> values,
        const bool looping
    ) : m_Looping(looping),
        m_Node(std::move(source))
    {
        if (!m_Node || !m_Node->IsValid() || !m_Node->IsValidPlayback(looping))
            return;

        m_Evaluator = m_Node->CreateEvaluator();

        if (!m_Evaluator || !SetInputs(values))
            m_Evaluator.reset();
    }

    AnimationPlayback::AnimationPlayback(const AnimationPlayback& other)
      : m_Phase(other.m_Phase),
        m_Looping(other.m_Looping),
        m_IncludeNotifyStart(other.m_IncludeNotifyStart),
        m_Node(other.m_Node),
        m_Inputs(other.m_Inputs),
        m_NotifiesEnabled(other.m_NotifiesEnabled)
    {
        m_Evaluator = other.m_Evaluator
            ? other.m_Evaluator->Clone()
            : nullptr;
    }

    AnimationPlayback& AnimationPlayback::operator=(const AnimationPlayback& other)
    {
        if (this != &other)
        {
            AnimationPlayback candidate(other);
            *this = std::move(candidate);
        }

        return *this;
    }

    bool AnimationPlayback::IsValid() const
    {
        return m_Node &&
            m_Evaluator &&
            std::isfinite(m_Phase) &&
            m_Phase >= 0 &&
            m_Phase <= 1;
    }

    SAnimationNodeState AnimationPlayback::GetEvaluationState() const
    {
        auto state = m_Evaluator
            ? m_Evaluator->GetState()
            : SAnimationNodeState{};

        if (m_Prepared)
            state.Sources = m_Evaluation.Sources;

        return state;
    }

    bool AnimationPlayback::CanAdvance() const
    {
        const auto state = GetEvaluationState();
        return m_Evaluator && (state.Duration > 0 || (m_Looping && state.Continuous));
    }

    float AnimationPlayback::GetTime() const
    {
        return float(m_Phase * GetDuration());
    }

    bool AnimationPlayback::IsFinished() const
    {
        if (!m_Evaluator) return false;

        const auto state = GetEvaluationState();

        if (state.Finished)
            return *state.Finished;

        return (!m_Looping && m_Phase >= 1) ||
            (state.Duration == 0 && (!m_Looping || !state.Continuous));
    }

    bool AnimationPlayback::SetInputs(const std::span<const float> values)
    {
        if (!m_Evaluator) return false;

        auto inputs = m_Inputs;
        if (!inputs.SetValues(values) ||
            !m_Evaluator->Configure(inputs.GetValues(), inputs.GetParameters()))
            return false;

        m_Inputs = std::move(inputs);
        m_Prepared = false;

        return true;
    }

    double AnimationPlayback::Advance(const float dt, const SLocalPose& referencePose) const
    {
        m_PositiveAdvance = dt > 0;
        m_Prepared = false;

        if (!std::isfinite(dt) || dt < 0)
            return std::numeric_limits<double>::quiet_NaN();

        if (!m_Evaluator) return 0;

        if (!m_Inputs.Refresh() ||
            !m_Evaluator->Configure(m_Inputs.GetValues(), m_Inputs.GetParameters()))
            return std::numeric_limits<double>::quiet_NaN();

        const auto duration = GetDuration();
        const auto end = duration > 0 ? m_Phase + double(dt) / duration : m_Phase;
        const auto phase = duration == 0
            ? (m_Looping ? m_Phase : 1)
            : (m_Looping ? std::fmod(end, 1.0) : std::min(end, 1.0));

        const SAnimationEvaluationContext context{
            phase,
            dt,
            true,
            m_NotifiesEnabled,
            referencePose,
            m_Looping
        };

        if (!m_Evaluator->Evaluate(context, m_Evaluation))
            return std::numeric_limits<double>::quiet_NaN();

        m_Prepared = true;
        return m_Evaluation.Phase;
    }

    void AnimationPlayback::Apply(const double phase)
    {
        if (!std::isfinite(phase) || phase < 0 || phase > 1)
        {
            EE_CORE_ERROR("Animation playback cannot publish an invalid phase.")
            return;
        }

        if (m_Evaluator)
            m_Evaluator->Apply(phase);

        m_Phase = phase;

        if (m_PositiveAdvance)
            m_IncludeNotifyStart = false;

        m_PositiveAdvance = false;
        m_Prepared = false;
    }

    bool AnimationPlayback::BindParameters(
        Ref<AnimationParameters> parameters,
        const std::span<const size_t> bindings
    )
    {
        if (!m_Evaluator) return false;

        auto inputs = m_Inputs;
        const auto indices = m_Node->GetInputCount() == 0
            ? std::span<const size_t>{}
            : bindings;

        if (!inputs.Bind(std::move(parameters), indices))
            return false;

        auto candidate = m_Evaluator->Clone();
        if (!candidate || !candidate->Configure(inputs.GetValues(), inputs.GetParameters()))
            return false;

        m_Evaluator = std::move(candidate);
        m_Inputs = std::move(inputs);
        m_Prepared = false;

        return true;
    }

    bool AnimationPlayback::IsTransitioning() const
    {
        return AnimationNodeQuery(GetEvaluationState()).IsTransitioning();
    }

    double AnimationPlayback::GetSeekPhase(const float seconds) const
    {
        const auto state = GetEvaluationState();

        if (state.PrimaryPlayback)
            return state.PrimaryPlayback->GetSeekPhase(seconds);

        if (state.Duration == 0)
            return 0;

        const auto phase = double(seconds) / state.Duration;

        if (!m_Looping)
            return std::clamp(phase, 0.0, 1.0);

        const auto wrapped = std::fmod(phase, 1.0);
        return wrapped < 0 ? wrapped + 1 : wrapped;
    }

    bool AnimationPlayback::SeekPhase(const double phase)
    {
        if (!m_Evaluator ||
            !std::isfinite(phase) ||
            phase < 0 ||
            phase > 1 ||
            !m_Evaluator->Reset(phase))
            return false;

        m_Phase = phase;
        m_IncludeNotifyStart = false;
        m_Prepared = false;

        return true;
    }

    std::string_view AnimationPlayback::GetCurrentState() const
    {
        return AnimationNodeQuery(GetEvaluationState()).GetCurrentState();
    }

    bool AnimationPlayback::Sample(
        const double phase,
        const SLocalPose& referencePose,
        SLocalPose& output
    ) const
    {
        if (!m_Evaluator || !std::isfinite(phase))
            return false;

        if (m_Prepared && phase == m_Evaluation.Phase)
        {
            output = m_Evaluation.Pose;
            return true;
        }

        SAnimationEvaluation evaluation;
        const SAnimationEvaluationContext context{
            phase,
            0,
            false,
            m_NotifiesEnabled,
            referencePose,
            m_Looping
        };

        if (!m_Evaluator->Evaluate(context, evaluation))
            return false;

        output = std::move(evaluation.Pose);
        return true;
    }

    SAnimationPlaybackInterval AnimationPlayback::GetPlaybackInterval(
        const double dt,
        const double offset
    ) const
    {
        const auto state = GetEvaluationState();

        if (const auto* selected = std::get_if<SAnimationSelectedSources>(&state.Sources))
        {
            if (!selected->Playback)
                return {};

            auto interval = selected->Playback->GetPlaybackInterval(dt, offset);
            interval.ResetActivations |= selected->ResetActivations;

            return interval;
        }

        if (const auto* layers = std::get_if<SAnimationLayerSources>(&state.Sources))
        {
            if (layers->Layers.empty())
                return {};

            const auto& player = layers->Layers.front()->GetPlayer();
            return player.GetPlayback().GetPlaybackInterval(dt, offset);
        }

        const auto& sources = std::get<SSequenceSources>(state.Sources);

        const auto phaseOffset = state.Duration > 0 ? offset / state.Duration : 0;
        const auto phaseDelta = state.Duration > 0 ? dt / state.Duration : 0;
        const auto phase = m_Phase + phaseOffset;

        return {
            .Sequence = sources.NotifySequence
                ? Ref<const Sequence>(m_Node, sources.NotifySequence)
                : nullptr,
            .Reference = sources.SynchronizationSequence
                ? Ref<const Sequence>(m_Node, sources.SynchronizationSequence)
                : nullptr,
            .StartPhase = phase,
            .EndPhase = phase + phaseDelta,
            .DurationSeconds = state.Duration,
            .FrameOffset = offset,
            .Looping = m_Looping,
            .IncludeStart = m_IncludeNotifyStart && offset == 0
        };
    }

    bool AnimationPlayback::ExtractRootMotion(
        const SRootMotionIntervalContext& interval,
        SRootMotionDelta& output
    ) const
    {
        if (!interval.IsValid())
        {
            EE_CORE_ERROR("Root motion requires a valid reference and interval.")
            return false;
        }

        if (!m_Evaluator || interval.DeltaSeconds== 0)
        {
            output = {};
            return true;
        }

        const auto state = GetEvaluationState();

        const SAnimationMotionContext context{
            state,
            interval,
            m_Phase,
            m_Looping
        };

        return RootMotion::Extract(context, output);
    }

    bool AnimationPlayback::EvaluateNotifies(
        const double dt,
        const std::string_view layerName,
        const AnimationNotifyTracker& tracker,
        SNotifyEvaluation& output,
        const double offset
    ) const
    {
        return !m_Evaluator || tracker.EvaluatePlayback(
            *this,
            dt,
            layerName,
            output,
            offset
        );
    }

    AnimationLayer* AnimationPlayback::GetLayer(const std::string_view name) const
    {
        return AnimationNodeQuery(GetEvaluationState()).GetLayer(name);
    }

    size_t AnimationPlayback::GetLayerCount() const
    {
        return AnimationNodeQuery(GetEvaluationState()).GetLayerCount();
    }

    void AnimationPlayback::SetNotifiesEnabled(const bool enabled)
    {
        m_NotifiesEnabled = enabled;
    }
}
