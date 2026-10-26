#include "epch.h"
#include <Engine/Animation/Pose.h>
#include "AnimationLayer.h"

namespace Elixir
{
    bool AnimationLayer::Play(Ref<const AnimationNode> source, const bool looping)
    {
        return Play(std::move(source), 0.0f, looping);
    }

    bool AnimationLayer::Play(
        Ref<const AnimationNode> source,
        const float fadeDuration,
        const bool looping
    )
    {
        if (!source || !source->IsCompatibleWith(m_Skeleton))
        {
            EE_CORE_ERROR(
                "AnimationLayer requires an animation with a compatible skeleton."
            )
            return false;
        }

        std::vector<float> values(source->GetInputCount(), 0);
        return StartPlayback(
            AnimationPlayback(std::move(source), values, looping),
            fadeDuration
        );
    }

    bool AnimationLayer::Play(
        Ref<const AnimationNode> source,
        const std::span<const float> values,
        const float fadeDuration,
        const bool looping
    )
    {
        if (!source || !source->IsCompatibleWith(m_Skeleton))
            return false;

        return StartPlayback(
            AnimationPlayback(std::move(source), values, looping),
            fadeDuration
        );
    }

    void AnimationLayer::Resume()
    {
        m_Montage.Resume();
        m_Playing = m_Playback.CanAdvance() || IsTransitioning();
    }

    void AnimationLayer::Stop()
    {
        m_Notifies.Cancel(m_Name);
        m_Montage = {};
        m_MontagePose.LocalTransforms.clear();
        m_Playing = false;
        m_Assigned = false;
        m_NotifyEvents.clear();
        m_Playback = {};
        m_Transition = {};
        m_LocalPose = m_ReferencePose;
    }

    bool AnimationLayer::Seek(const float time)
    {
        if (!m_Playback.IsValid() || !std::isfinite(time))
        {
            EE_CORE_ERROR("AnimationLayer requires playback and a finite seek time.")
            return false;
        }

        const auto targetTime = m_Playback.GetSeekPhase(time);

        auto candidate = m_Playback;
        if (!candidate.SeekPhase(targetTime))
            return false;

        SLocalPose pose;
        if (!candidate.Sample(targetTime, m_ReferencePose, pose))
            return false;

        m_Playback = std::move(candidate);
        m_LocalPose = std::move(pose);

        if (!m_Montage.GetAsset())
            m_Notifies.Cancel(m_Name);

        m_Transition = {};
        m_Playback.SuppressInitialNotifies();
        m_NotifyEvents.clear();

        return true;
    }

    bool AnimationLayer::PlayMontage(
        Ref<const AnimationMontage> montage,
        const std::string_view startSection
    )
    {
        if (!montage || !montage->GetSequence()->IsCompatibleWith(m_Skeleton))
            return false;

        SLocalPose pose;
        const auto success = m_Montage.Play(
            std::move(montage),
            startSection,
            GetLocalPose(),
            m_LocalPose,
            m_ReferencePose,
            pose
        );

        if (!success) return false;

        m_Notifies.Cancel(m_Name);
        m_MontagePose = std::move(pose);
        m_NotifyEvents.clear();

        return true;
    }

    bool AnimationLayer::SetNextMontageSection(
        const std::string_view from,
        const std::string_view to
    )
    {
        return m_Montage.SetNextSection(from, to);
    }

    bool AnimationLayer::JumpToMontageSection(const std::string_view name)
    {
        SLocalPose pose;
        if (!m_Montage.Jump(name, m_LocalPose, m_ReferencePose, pose))
            return false;

        m_Notifies.Cancel(m_Name);
        m_MontagePose = std::move(pose);
        m_NotifyEvents.clear();

        return true;
    }

    std::string_view AnimationLayer::GetCurrentMontageSection() const
    {
        return m_Montage.GetCurrentSection();
    }

    bool AnimationLayer::StopMontage(const std::optional<float> blendOut)
    {
        if (!m_Montage.Stop(blendOut))
            return false;

        m_Notifies.Cancel(m_Name);

        if (!m_Montage.IsActive())
        {
            m_MontagePose.LocalTransforms.clear();
            m_NotifyEvents.clear();
        }

        return true;
    }

    bool AnimationLayer::SetWeight(const float weight)
    {
        if (!std::isfinite(weight) ||
            weight < 0.0f ||
            weight > 1.0f ||
            (m_Base && weight != 1.0f))
        {
            EE_CORE_ERROR("Layer weight must be in [0, 1]; the base layer requires one.")
            return false;
        }

        if (weight == 0)
            m_Notifies.Cancel(m_Name);

        m_Weight = weight;
        return true;
    }

    bool AnimationLayer::SetBlendMode(const EAnimationLayerBlendMode mode)
    {
        if (m_Base && mode != EAnimationLayerBlendMode::Override)
        {
            EE_CORE_ERROR("Base animation layer requires Override blending.")
            return false;
        }

        m_BlendMode = mode;
        return true;
    }

    bool AnimationLayer::SetAdditiveReferencePose(const SLocalPose& pose)
    {
        if (!pose.LocalTransforms.empty() &&
            !pose.IsValid(m_ReferencePose.LocalTransforms.size()))
        {
            EE_CORE_ERROR("An additive reference requires one valid local transform per bone.")
            return false;
        }

        SLocalPose reference = pose;
        m_AdditiveReferencePose = std::move(reference);

        return true;
    }

    bool AnimationLayer::SetSourceInputs(const std::span<const float> values)
    {
        return m_Playback.SetInputs(values);
    }

    bool AnimationLayer::SetLocalPose(const SLocalPose& pose)
    {
        if (!m_Skeleton)
        {
            EE_CORE_ERROR("AnimationLayer requires a skeleton before setting a pose.")
            return false;
        }

        if (!pose.IsValid(m_ReferencePose.LocalTransforms.size()))
        {
            EE_CORE_ERROR("AnimationLayer requires one valid transform per bone.")
            return false;
        }

        m_Notifies.Cancel(m_Name);

        SLocalPose local = pose;
        m_LocalPose = std::move(local);

        m_Montage = {};
        m_MontagePose.LocalTransforms.clear();

        m_Playback = {};
        m_Transition = {};
        m_Playing = false;
        m_Assigned = true;
        m_NotifyEvents.clear();

        return true;
    }

    float AnimationLayer::GetNormalizedTime() const
    {
        if (!m_Playback.IsValid())
            return 0;

        return m_Playback.GetDuration() == 0 && m_Playback.IsFinished()
            ? 1
            : m_Playback.GetPhase();
    }

    bool AnimationLayer::BindParameters(
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


    bool AnimationLayer::EvaluatePlayback(
        const float dt,
        SAnimationLayerEvaluation& output
    ) const
    {
        if (!std::isfinite(dt) || dt < 0.0f || !m_Skeleton)
        {
            EE_CORE_ERROR("Animation playback requires a valid layer and delta time.")
            return false;
        }

        output.Advance = m_Playing;
        output.ConsumeInitialNotifies = dt > 0.0f && m_Playing;

        if (!m_Playing)
        {
            output.LocalPose = m_LocalPose;
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

        return m_Transition.Sample(
            m_Playback,
            m_Transition.Playback,
            weight,
            output.PlaybackPhase,
            output.TransitionSourcePhase,
            output.LocalPose,
            m_ReferencePose
        );
    }

    AnimationLayer::AnimationLayer(
        std::string name,
        Ref<const BoneMask> mask,
        const float weight,
        const SLocalPose& referencePose
    ) : m_Name(std::move(name)),
        m_Weight(weight),
        m_Mask(std::move(mask))
    {
        m_Skeleton = m_Mask ? m_Mask->GetSkeleton() : nullptr;

        if (m_Name.empty() ||
            !m_Skeleton ||
            !std::isfinite(weight) ||
            weight < 0.0f ||
            weight > 1.0f ||
            !referencePose.IsValid(m_Skeleton->GetBones().size()))
        {
            EE_CORE_ERROR("AnimationLayer requires a name, mask, weight, and reference pose.")
            m_Skeleton.reset();
            return;
        }

        m_ReferencePose = referencePose;
        Stop();
    }

    bool AnimationLayer::BlendPose(SLocalPose& output, const SLocalPose& pose) const
    {
        if (!HasPose() || m_Weight == 0 || !m_Mask->HasInfluence())
            return true;

        return output.Blend(
            pose,
            m_Mask->GetWeights(),
            m_Weight,
            m_BlendMode,
            GetAdditiveReferencePose()
        );
    }

    bool AnimationLayer::Evaluate(
        const float dt,
        SAnimationLayerEvaluation& output,
        const bool paused,
        const bool notifiesEnabled
    ) const
    {
        if (paused)
        {
            output.LocalPose = GetLocalPose();
            output.PlaybackPose = m_LocalPose;
            output.Montage.Cursor = m_Montage.GetCursor();
            output.Advance = false;
            output.ConsumeInitialNotifies = false;
            m_Notifies.Begin(output.Notifies);
            return true;
        }

        if (notifiesEnabled &&
            m_Weight > 0 &&
            m_Mask->HasInfluence() &&
            !m_Montage.GetAsset())
            m_Playback.SetNotifiesEnabled(true);

        m_Transition.Playback.SetNotifiesEnabled(false);

        if (!EvaluatePlayback(dt, output))
            return false;

        if (m_Montage.GetAsset())
        {
            if (!m_Montage.Evaluate(dt, output.Montage))
                return false;

            output.PlaybackPose = output.LocalPose;

            const auto sampled = m_Montage.Sample(
                output.Montage.Cursor,
                output.LocalPose,
                m_ReferencePose
            );

            if (!sampled) return false;
        }

        return EvaluateNotifyEvents(dt, output, !notifiesEnabled);
    }

    void AnimationLayer::Apply(SAnimationLayerEvaluation evaluation)
    {
        m_Notifies.Apply({
            std::move(evaluation.Notifies.Active),
            {},
            evaluation.Notifies.NextActivationId
        });

        if (m_Montage.GetAsset())
        {
            m_LocalPose = std::move(evaluation.PlaybackPose);
            m_MontagePose = std::move(evaluation.LocalPose);

            m_Montage.Apply(evaluation.Montage.Cursor);

            if (!m_Montage.IsActive())
                m_MontagePose.LocalTransforms.clear();
        }
        else
        {
            m_LocalPose = std::move(evaluation.LocalPose);
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

    bool AnimationLayer::StartPlayback(AnimationPlayback playback, const float fadeDuration)
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

            if (!IsTransitioning() && m_Playback.IsValid())
                transition.Playback = m_Playback;
            else
                transition.Pose = m_LocalPose;

            m_Transition = std::move(transition);
            m_Playback = std::move(playback);
        }
        else
        {
            const auto success = SetPlayback(playback, {}, 1.0f, playback.GetPhase(), 0.0f);
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

    bool AnimationLayer::SetPlayback(
        const AnimationPlayback& playback,
        const AnimationPlayback& source,
        const float weight,
        const double time,
        const double sourceTime
    )
    {
        SLocalPose local;

        const auto sampled = m_Transition.Sample(
            playback,
            source,
            weight,
            time,
            sourceTime,
            local,
            m_ReferencePose
        );

        if (!sampled) return false;

        m_LocalPose = std::move(local);

        m_Playback = playback;
        m_Playback.Apply(time);

        return true;
    }

    bool AnimationLayer::ExtractRootMotion(
        const SRootMotionIntervalContext& interval,
        SRootMotionDelta& output,
        const SAnimationLayerEvaluation& evaluation
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

    bool AnimationLayer::EvaluateNotifyEvents(
        const float dt,
        SAnimationLayerEvaluation& evaluation,
        const bool resetActivations
    ) const
    {
        auto& output = evaluation.Notifies;
        m_Notifies.Begin(output);

        if (resetActivations)
            AnimationNotifyTracker::End(output, 0, true, m_Name);

        if (m_Weight == 0 || !m_Mask->HasInfluence())
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
                const auto traversal = m_Montage.GetTraversal(interval);

                const auto success = m_Notifies.Evaluate(
                    traversal,
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
