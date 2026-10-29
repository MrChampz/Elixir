#include "epch.h"
#include "AnimationPlayer.h"

namespace Elixir
{
    AnimationPlayer::AnimationPlayer(
        std::string name,
        Ref<const Skeleton> skeleton,
        const SLocalPose& referencePose
    ) : m_Name(std::move(name)),
        m_Skeleton(std::move(skeleton)),
        m_ReferencePose(referencePose)
    {
        if (m_Name.empty() ||
            !m_Skeleton ||
            !referencePose.IsValid(m_Skeleton->GetBones().size()))
        {
            EE_CORE_ERROR("AnimationPlayer requires a name, skeleton, and reference pose.")
            m_Skeleton.reset();
            return;
        }

        Stop();
    }

    bool AnimationPlayer::Evaluate(
        const float dt,
        SAnimationPlayerEvaluation& output,
        const bool paused,
        const bool notifiesEnabled
    )
    {
        if (paused)
        {
            output.Sample = GetSample();
            output.PlaybackSample = m_Sample;
            output.Montage.Cursor = m_Montage.GetCursor();
            output.Advance = false;
            output.DeltaSeconds = 0;
            output.ConsumeInitialNotifies = false;
            m_Notifies.Begin(output.Notifies);
            return true;
        }

        const auto sourceNotifiesEnabled =
            notifiesEnabled && m_NotifiesEnabled && !m_Montage.GetAsset();

        m_Playback.SetNotifiesEnabled(sourceNotifiesEnabled);
        m_Transition.Playback.SetNotifiesEnabled(false);

        if (!EvaluatePlayback(dt, output))
            return false;

        if (m_Montage.GetAsset())
        {
            if (!m_Montage.Evaluate(dt, output.Montage))
                return false;

            output.PlaybackSample = output.Sample;

            const auto sampled = m_Montage.Sample(
                output.Montage.Cursor,
                output.Sample,
                m_ReferencePose
            );

            if (!sampled) return false;
        }

        return EvaluateNotifyEvents(dt, output, !notifiesEnabled);
    }

    void AnimationPlayer::Apply(SAnimationPlayerEvaluation evaluation)
    {
        if (evaluation.Advance && evaluation.DeltaSeconds > 0)
        {
            m_PreviousSample = m_Sample;
            m_PoseDeltaSeconds = evaluation.DeltaSeconds;
        }

        m_Notifies.Apply({
            std::move(evaluation.Notifies.Active),
            {},
            evaluation.Notifies.NextActivationId
        });

        if (m_Montage.GetAsset())
        {
            m_Sample = std::move(evaluation.PlaybackSample);
            m_MontageSample = std::move(evaluation.Sample);

            m_Montage.Apply(evaluation.Montage.Cursor);

            if (!m_Montage.IsActive())
                m_MontageSample = {};
        }
        else
        {
            m_Sample = std::move(evaluation.Sample);
        }

        m_NotifyEvents = std::move(evaluation.Notifies.Events);

        if (!evaluation.Advance)
            return;

        if (evaluation.ConsumeInitialNotifies)
            m_Playback.SuppressInitialNotifies();

        m_Playback.Apply(evaluation.PlaybackPhase);
        m_Transition.Playback.Apply(evaluation.TransitionSourcePhase);
        m_Transition.Elapsed = evaluation.TransitionElapsed;

        if (m_Transition.Elapsed >= m_Transition.Duration)
            m_Transition = {};

        if (!IsTransitioning() && m_Playback.IsFinished())
            m_Playing = false;
    }

    bool AnimationPlayer::Play(Ref<const AnimationNode> source, const bool looping)
    {
        return Play(std::move(source), 0.0f, looping);
    }

    bool AnimationPlayer::Play(
        Ref<const AnimationNode> source,
        const float fadeDuration,
        const bool looping,
        const EAnimationTransitionMode mode
    )
    {
        if (!source || !source->IsCompatibleWith(m_Skeleton))
        {
            EE_CORE_ERROR(
                "AnimationPlayer requires an animation with a compatible skeleton."
            )
            return false;
        }

        std::vector<float> values(source->GetInputCount(), 0);

        return StartPlayback(
            AnimationPlayback(std::move(source), values, looping),
            fadeDuration,
            mode
        );
    }

    bool AnimationPlayer::Play(
        Ref<const AnimationNode> source,
        const std::span<const float> values,
        const float fadeDuration,
        const bool looping,
        const EAnimationTransitionMode mode
    )
    {
        if (!source || !source->IsCompatibleWith(m_Skeleton))
            return false;

        return StartPlayback(
            AnimationPlayback(std::move(source), values, looping),
            fadeDuration,
            mode
        );
    }

    void AnimationPlayer::Resume()
    {
        m_Montage.Resume();
        m_Playing = m_Playback.CanAdvance() || IsTransitioning();
    }

    void AnimationPlayer::Stop()
    {
        m_Notifies.Cancel(m_Name);
        m_Montage = {};
        m_MontageSample = {};
        m_Playing = false;
        m_Assigned = false;
        m_NotifyEvents.clear();
        m_Playback = {};
        m_Transition = {};
        m_Sample = { m_ReferencePose };

        ResetPoseHistory();
    }

    bool AnimationPlayer::Seek(const float time)
    {
        if (!m_Playback.IsValid() || !std::isfinite(time))
        {
            EE_CORE_ERROR("AnimationPlayer requires playback and a finite seek time.")
            return false;
        }

        const auto targetTime = m_Playback.GetSeekPhase(time);

        auto candidate = m_Playback;
        if (!candidate.SeekPhase(targetTime))
            return false;

        SAnimationSample sample;
        if (!candidate.Sample(targetTime, m_ReferencePose, sample))
            return false;

        m_Playback = std::move(candidate);
        m_Sample = std::move(sample);

        ResetPoseHistory();

        if (!m_Montage.GetAsset())
            m_Notifies.Cancel(m_Name);

        m_Transition = {};
        m_Playback.SuppressInitialNotifies();
        m_NotifyEvents.clear();

        return true;
    }

    bool AnimationPlayer::PlayMontage(
        Ref<const AnimationMontage> montage,
        const std::string_view startSection
    )
    {
        if (!montage || !montage->GetSequence()->IsCompatibleWith(m_Skeleton))
            return false;

        SAnimationSample sample;

        const auto success = m_Montage.Play(
            std::move(montage),
            startSection,
            GetSample(),
            m_Sample,
            m_ReferencePose,
            sample
        );

        if (!success) return false;

        m_MontageSample = std::move(sample);
        m_Notifies.Cancel(m_Name);
        m_NotifyEvents.clear();

        return true;
    }

    bool AnimationPlayer::SetNextMontageSection(
        const std::string_view from,
        const std::string_view to
    )
    {
        return m_Montage.SetNextSection(from, to);
    }

    bool AnimationPlayer::JumpToMontageSection(const std::string_view name)
    {
        SAnimationSample sample;

        const auto jumped = m_Montage.Jump(
            name,
            m_Sample,
            m_ReferencePose,
            sample
        );

        if (!jumped) return false;

        m_MontageSample = std::move(sample);
        m_Notifies.Cancel(m_Name);
        m_NotifyEvents.clear();

        return true;
    }

    std::string_view AnimationPlayer::GetCurrentMontageSection() const
    {
        return m_Montage.GetCurrentSection();
    }

    bool AnimationPlayer::StopMontage(const std::optional<float> blendOut)
    {
        if (!m_Montage.Stop(blendOut))
            return false;

        m_Notifies.Cancel(m_Name);

        if (!m_Montage.IsActive())
        {
            m_MontageSample = {};
            m_NotifyEvents.clear();
        }

        return true;
    }

    void AnimationPlayer::CancelNotifies()
    {
        m_Notifies.Cancel(m_Name);
    }

    bool AnimationPlayer::SetSourceInputs(const std::span<const float> values)
    {
        return m_Playback.SetInputs(values);
    }

    bool AnimationPlayer::SetLocalPose(const SLocalPose& pose)
    {
        if (!m_Skeleton)
        {
            EE_CORE_ERROR("AnimationPlayer requires a skeleton before setting a pose.")
            return false;
        }

        if (!pose.IsValid(m_ReferencePose.LocalTransforms.size()))
        {
            EE_CORE_ERROR("AnimationPlayer requires one valid transform per bone.")
            return false;
        }

        m_Notifies.Cancel(m_Name);

        SLocalPose local = pose;
        m_Sample = { std::move(local) };

        ResetPoseHistory();

        m_Montage = {};
        m_MontageSample = {};

        m_Playback = {};
        m_Transition = {};
        m_Playing = false;
        m_Assigned = true;
        m_NotifyEvents.clear();

        return true;
    }

    float AnimationPlayer::GetNormalizedTime() const
    {
        if (!m_Playback.IsValid())
            return 0;

        return m_Playback.GetDuration() == 0 && m_Playback.IsFinished()
            ? 1
            : m_Playback.GetPhase();
    }

    bool AnimationPlayer::BindParameters(
        Ref<AnimationParameters> parameters,
        const std::span<const std::string> bindings
    )
    {
        if (!parameters || !parameters->IsValid())
            return false;

        std::vector<size_t> resolved;
        for (const auto& name : bindings)
        {
            const auto index = parameters->Find(name);

            if (!index || !parameters->GetFloat(name))
                return false;

            resolved.push_back(*index);
        }

        if (m_Playback.IsValid() && !m_Playback.BindParameters(parameters, resolved))
            return false;

        m_Parameters = std::move(parameters);
        m_InputBindings = std::move(resolved);

        return true;
    }

    bool AnimationPlayer::ExtractRootMotion(
        const SRootMotionIntervalContext& interval,
        SRootMotionDelta& output,
        const SAnimationPlayerEvaluation& evaluation
    ) const
    {
        return RootMotion::Extract(
            {
                m_Playback,
                m_Transition,
                m_Montage,
                evaluation.Montage,
                interval,
                m_Playing
            },
            output
        );
    }

    bool AnimationPlayer::ExtractRootMotion(
        const SRootMotionIntervalContext& interval,
        SRootMotionDelta& output
    ) const
    {
        if (!interval.IsValid())
        {
            EE_CORE_ERROR("Layer root motion requires a valid reference and interval.")
            return false;
        }

        SAnimationPlayerEvaluation evaluation;
        if (m_Montage.GetAsset())
        {
            const auto end = interval.OffsetSeconds + interval.DeltaSeconds;
            if (!std::isfinite(end) || !m_Montage.Evaluate(end, evaluation.Montage))
                return false;
        }

        return ExtractRootMotion(interval, output, evaluation);
    }

    void AnimationPlayer::SetNotifiesEnabled(const bool enabled)
    {
        if (!enabled)
            CancelNotifies();

        m_NotifiesEnabled = enabled;
    }

    bool AnimationPlayer::EvaluatePlayback(
        const float dt,
        SAnimationPlayerEvaluation& output
    ) const
    {
        if (!std::isfinite(dt) || dt < 0.0f || !m_Skeleton)
        {
            EE_CORE_ERROR("Animation playback requires a valid layer and delta time.")
            return false;
        }

        output.Advance = m_Playing;
        output.DeltaSeconds = dt;
        output.ConsumeInitialNotifies = dt > 0.0f && m_Playing;

        if (!m_Playing)
        {
            output.Sample = m_Sample;
            return true;
        }

        output.PlaybackPhase = m_Playback.Advance(dt, m_ReferencePose);
        output.TransitionSourcePhase = m_Transition.Playback.Advance(dt, m_ReferencePose);
        output.TransitionElapsed = float(std::min(
            double(m_Transition.Elapsed) + dt,
            double(m_Transition.Duration)
        ));

        const auto weight = m_Transition.IsActive()
            ? output.TransitionElapsed / m_Transition.Duration
            : 1.0f;

        SAnimationSample sample;

        const auto success = m_Transition.Sample(
            m_Playback,
            m_Transition.Playback,
            weight,
            output.PlaybackPhase,
            output.TransitionSourcePhase,
            sample,
            m_ReferencePose
        );

        if (!success) return false;

        output.Sample = std::move(sample);

        return true;
    }

    bool AnimationPlayer::StartPlayback(
        AnimationPlayback playback,
        const float fadeDuration,
        const EAnimationTransitionMode mode
    )
    {
        if (!playback.IsValid() || !std::isfinite(fadeDuration) || fadeDuration < 0.0f)
        {
            EE_CORE_ERROR("Animation playback requires a valid source and a finite nonnegative fade duration.")
            return false;
        }

        if (!playback.BindParameters(m_Parameters, m_InputBindings))
            return false;

        if (fadeDuration > 0.0f)
        {
            SLocalPose local;

            if (!playback.Sample(playback.GetPhase(), m_ReferencePose, local))
                return false;

            AnimationTransition transition;
            transition.Duration = fadeDuration;
            transition.Mode = mode;

            if (mode == EAnimationTransitionMode::Inertialization)
            {
                const auto captured = transition.CaptureInertialization(
                    playback,
                    m_Sample,
                    m_Playing ? &m_PreviousSample : nullptr,
                    m_PoseDeltaSeconds,
                    m_ReferencePose
                );
                if (!captured) return false;
            }
            else if (!IsTransitioning() && m_Playback.IsValid())
            {
                transition.Playback = m_Playback;
            }
            else
            {
                transition.SourceSample = m_Sample;
            }

            m_Transition = std::move(transition);
            m_Playback = std::move(playback);
        }
        else
        {
            const auto success = SetPlayback(
                playback,
                {},
                1.0f,
                playback.GetPhase(),
                0.0f
            );

            if (!success) return false;

            m_Transition = {};
        }

        if (!m_Montage.GetAsset())
            m_Notifies.Cancel(m_Name);

        m_NotifyEvents.clear();
        m_Assigned = true;
        Resume();

        return true;
    }

    bool AnimationPlayer::SetPlayback(
        const AnimationPlayback& playback,
        const AnimationPlayback& source,
        const float weight,
        const double time,
        const double sourceTime
    )
    {
        SAnimationSample sample;

        const auto sampled = m_Transition.Sample(
            playback,
            source,
            weight,
            time,
            sourceTime,
            sample,
            m_ReferencePose
        );

        if (!sampled) return false;

        m_Sample = std::move(sample);

        ResetPoseHistory();

        m_Playback = playback;
        m_Playback.Apply(time);

        return true;
    }

    void AnimationPlayer::ResetPoseHistory()
    {
        m_PreviousSample = {};
        m_PoseDeltaSeconds = 0;
    }

    bool AnimationPlayer::EvaluateNotifyEvents(
        const float dt,
        SAnimationPlayerEvaluation& evaluation,
        const bool resetActivations
    ) const
    {
        auto& output = evaluation.Notifies;
        m_Notifies.Begin(output);

        if (resetActivations)
            AnimationNotifyTracker::End(output, 0, true, m_Name);

        if (!m_NotifiesEnabled)
        {
            AnimationNotifyTracker::End(output, 0, true, m_Name);
        }
        else if (!m_Montage.GetAsset())
        {
            if (m_Playing &&
                !m_Playback.EvaluateNotifies(dt, m_Name, m_Notifies, output))
                return false;
        }
        else
        {
            const auto& montage = evaluation.Montage;
            for (size_t i = 0; i < montage.Intervals.size(); ++i)
            {
                const auto& interval = montage.Intervals[i];
                const auto playbackInterval = m_Montage.GetPlaybackInterval(interval);

                const auto success = m_Notifies.Evaluate(
                    playbackInterval,
                    m_Name,
                    output,
                    !m_Montage.IsStopping()
                );

                if (!success) return false;

                const auto end = interval.Start.Time + interval.Duration;

                const auto nextIndex = i + 1;
                const auto nextTime = nextIndex < montage.Intervals.size()
                    ? montage.Intervals[nextIndex].Start.Time
                    : montage.Cursor.Time;

                if (end != nextTime)
                {
                    AnimationNotifyTracker::End(
                        output,
                        interval.TimeOffset + interval.Duration,
                        true,
                        m_Name
                    );
                }
            }

            if (montage.Cursor.Finished)
            {
                AnimationNotifyTracker::End(output, montage.ActiveDuration, false, m_Name);
                const auto remaining = double(dt) - montage.ActiveDuration;

                if (m_Playing && remaining > 0)
                {
                    const auto success = m_Playback.EvaluateNotifies(
                        remaining,
                        m_Name,
                        m_Notifies,
                        output,
                        montage.ActiveDuration
                    );

                    if (!success) return false;
                }
            }
        }

        const auto emittedCount = output.Events.size() - m_Notifies.GetPendingEnds().size();
        if (emittedCount > Sequence::MaxNotifyEventsPerUpdate)
        {
            EE_CORE_ERROR("Animation update exceeded the notify event capacity.")
            return false;
        }

        std::ranges::stable_sort(output.Events, [](const auto& first, const auto& second)
        {
            if (first.TimeOffset != second.TimeOffset)
                return first.TimeOffset < second.TimeOffset;

            const auto priority = [](const auto type)
            {
                return type == EAnimationNotifyEventType::Update
                    ? 0
                    : type == EAnimationNotifyEventType::End
                        ? 1
                        : 2;
            };

            return priority(first.Type) < priority(second.Type);
        });

        return true;
    }
}
