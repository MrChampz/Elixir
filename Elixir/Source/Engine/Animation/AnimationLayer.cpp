#include "epch.h"
#include "AnimationLayer.h"

namespace Elixir
{
    namespace
    {
        /**
         * @brief Wrap or clamp a playback time to the sequence duration.
         * @param time Playback time in seconds or normalized phase.
         * @param duration Duration in the same unit as time.
         * @param looping Wrap the time instead of clamping it.
         */
        double SampleTime(const double time, const float duration, const bool looping)
        {
            if (duration == 0.0f)
                return 0.0f;

            if (!looping)
                return std::clamp(time, 0.0, double(duration));

            const auto wrapped = std::fmod(time, double(duration));
            return wrapped < 0.0 ? wrapped + duration : wrapped;
        }
    }

    bool AnimationLayer::SetBlendMode(const EAnimationLayerBlendMode mode)
    {
        if ((mode != EAnimationLayerBlendMode::Override &&
             mode != EAnimationLayerBlendMode::Additive) ||
            (m_Base && mode != EAnimationLayerBlendMode::Override))
        {
            EE_CORE_ERROR("Animation layer blend mode is invalid; Base requires Override.")
            return false;
        }

        m_BlendMode = mode;
        return true;
    }

    bool AnimationLayer::SetAdditiveReferencePose(const std::span<const SBoneTransform> pose)
    {
        const auto invalidBone = std::ranges::any_of(
            pose,
            [](const auto& bone) { return !bone.IsValid(); }
        );

        if ((!pose.empty() && pose.size() != m_ReferencePose.size()) || invalidBone)
        {
            EE_CORE_ERROR("An additive reference requires one valid local transform per bone.")
            return false;
        }

        std::vector reference(pose.begin(), pose.end());
        m_AdditiveReferencePose = std::move(reference);

        return true;
    }

    bool AnimationLayer::BlendPose(
        const std::span<SBoneTransform> output,
        const std::span<const SBoneTransform> pose
    ) const
    {
        if (!HasPose() || m_Weight == 0.0f || !m_Mask->HasInfluence())
            return true;

        const auto& weights = m_Mask->GetWeights();
        const auto& reference = GetAdditiveReferencePose();

        if (output.size() != weights.size() || pose.size() != output.size())
        {
            EE_CORE_ERROR("Layer composition requires one transform per skeleton bone.")
            return false;
        }

        for (size_t bone = 0; bone < output.size(); ++bone)
        {
            const auto weight = m_Weight * weights[bone];
            if (weight == 0.0f)
                continue;

            if (m_BlendMode == EAnimationLayerBlendMode::Override)
            {
                output[bone] = SBoneTransform::Blend(output[bone], pose[bone], weight);
            }
            else
            {
                const auto& sample = pose[bone];
                const auto& origin = reference[bone];

                auto rotation = sample.Rotation * glm::inverse(origin.Rotation);
                rotation = glm::normalize(rotation);
                rotation = glm::slerp(glm::identity<glm::quat>(), rotation, weight);
                rotation = glm::normalize(rotation);

                auto scale = sample.Scale / origin.Scale;
                scale = glm::mix(glm::vec3(1.0f), scale, weight);

                output[bone].Translation += (sample.Translation - origin.Translation) * weight;
                output[bone].Rotation = glm::normalize(rotation * output[bone].Rotation);
                output[bone].Scale *= scale;
            }

            if (!output[bone].IsValid())
            {
                EE_CORE_ERROR("Animation layer composition produced an invalid transform.")
                return false;
            }
        }

        return true;
    }

    bool AnimationLayer::Play(Ref<const AnimationSequence> sequence, const bool looping)
    {
        return Play(std::move(sequence), 0.0f, looping);
    }

    bool AnimationLayer::Play(
        Ref<const AnimationSequence> sequence,
        const float fadeDuration,
        const bool looping
    )
    {
        if (!m_Skeleton || !sequence || (m_Skeleton != sequence->GetSkeleton() &&
                !m_Skeleton->IsCompatible(*sequence->GetSkeleton())))
        {
            EE_CORE_ERROR(
                "AnimationLayer requires an animation with a compatible skeleton."
            )
            return false;
        }

        return StartPlayback(
            {
                .Sequence = std::move(sequence),
                .Looping = looping
            },
            fadeDuration
        );
    }

    bool AnimationLayer::Play(
        Ref<const BlendSpace> blendSpace,
        const std::span<const float> values,
        const float fadeDuration,
        const bool looping
    )
    {
        if (!m_Skeleton || !blendSpace || !blendSpace->HasValidCoordinates(values) ||
            (m_Skeleton != blendSpace->GetSkeleton() &&
                !m_Skeleton->IsCompatible(*blendSpace->GetSkeleton())))
        {
            EE_CORE_ERROR(
                "Blend-space playback requires compatible assets and valid coordinates."
            )
            return false;
        }

        return StartPlayback(
            {
                .BlendSpace = std::move(blendSpace),
                .Values = std::vector(values.begin(), values.end()),
                .Looping = looping
            },
            fadeDuration
        );
    }

    void AnimationLayer::Resume()
    {
        m_Montage.Playing = bool(m_Montage.Montage);
        m_Playing = GetDuration(m_Playback) > 0.0f || IsTransitioning() ||
            (m_Playback.BlendSpace && m_Playback.Looping);
    }

    void AnimationLayer::Stop()
    {
        m_Montage = {};
        m_MontagePose.clear();
        m_Playing = false;
        m_Assigned = false;
        m_NotifyEvents.clear();
        m_Playback = {};
        m_Transition = {};

        m_LocalPose = m_ReferencePose;
    }

    bool AnimationLayer::Seek(const float time)
    {
        if ((!m_Playback.Sequence && !m_Playback.BlendSpace) || !std::isfinite(time))
        {
            EE_CORE_ERROR("AnimationLayer requires playback and a finite seek time.")
            return false;
        }

        const auto duration = GetDuration(m_Playback);
        const auto sampled = SampleTime(time, duration, m_Playback.Looping);

        const auto targetTime = m_Playback.BlendSpace && duration > 0.0f
            ? sampled / duration
            : sampled;

        if (!SetPlayback(m_Playback, {}, 1.0f, targetTime, 0.0f))
            return false;

        m_Transition = {};
        m_Playback.IncludeNotifyStart = false;
        m_NotifyEvents.clear();

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

        m_Weight = weight;
        return true;
    }

    bool AnimationLayer::SetBlendSpaceValue(const std::span<const float> values)
    {
        if (!m_Playback.BlendSpace || !m_Playback.BlendSpace->HasValidCoordinates(values))
        {
            EE_CORE_ERROR("Blend coordinates must match the active blend space and be finite.")
            return false;
        }

        std::ranges::copy(values, m_Playback.Values.begin());
        return true;
    }

    bool AnimationLayer::SetLocalPose(const std::span<const SBoneTransform> pose)
    {
        if (!m_Skeleton)
        {
            EE_CORE_ERROR("AnimationLayer requires a skeleton before setting a pose.")
            return false;
        }

        const auto invalidBone = std::ranges::any_of(
            pose,
            [](const auto& bone) { return !bone.IsValid(); }
        );

        if (pose.size() != m_ReferencePose.size() || invalidBone)
        {
            EE_CORE_ERROR("AnimationLayer requires one valid transform per bone.")
            return false;
        }

        std::vector local(pose.begin(), pose.end());
        m_LocalPose = std::move(local);
        m_Montage = {};
        m_MontagePose.clear();

        m_Playback = {};
        m_Transition = {};
        m_Playing = false;
        m_Assigned = true;
        m_NotifyEvents.clear();

        return true;
    }

    float AnimationLayer::GetTime() const
    {
        return m_Playback.BlendSpace
            ? m_Playback.Time * GetDuration(m_Playback)
            : m_Playback.Time;
    }

    float AnimationLayer::GetNormalizedTime() const
    {
        if (!m_Playback.Sequence && !m_Playback.BlendSpace)
            return 0.0f;

        const auto duration = GetDuration(m_Playback);

        if (m_Playback.BlendSpace)
            return duration == 0.0f && !m_Playback.Looping
                ? 1.0f
                : m_Playback.Time;

        return duration == 0.0f
            ? 1.0f
            : m_Playback.Time / duration;
    }

    AnimationLayer::AnimationLayer(
        std::string name,
        Ref<const BoneMask> mask,
        const float weight,
        const std::span<const SBoneTransform> referencePose
    ) : m_Name(std::move(name)),
        m_Mask(std::move(mask)),
        m_Weight(weight)
    {
        m_Skeleton = m_Mask ? m_Mask->GetSkeleton() : nullptr;

        const auto invalidBone = std::ranges::any_of(
            referencePose,
            [](const auto& bone) { return !bone.IsValid(); }
        );

        if (m_Name.empty() ||
            !m_Skeleton ||
            !std::isfinite(weight) ||
            weight < 0.0f ||
            weight > 1.0f ||
            referencePose.size() != m_Skeleton->GetBones().size() ||
            invalidBone)
        {
            EE_CORE_ERROR("AnimationLayer requires a name, mask, weight, and reference pose.")
            m_Skeleton.reset();
            return;
        }

        m_ReferencePose.assign(referencePose.begin(), referencePose.end());
        Stop();
    }

    bool AnimationLayer::EvaluatePlayback(const float dt, SLayerEvaluation& output) const
    {
        if (!std::isfinite(dt) || dt < 0.0f || !m_Skeleton)
        {
            EE_CORE_ERROR("Animation playback requires a valid layer and delta time.")
            return false;
        }

        output.NotifyEvents.clear();
        output.Advance = m_Playing;
        output.ConsumeInitialNotifies = dt > 0.0f && m_Playing;

        const auto emitNotifies = m_Weight > 0.0f && m_Mask->HasInfluence();
        if (emitNotifies && m_Playing && !m_Montage.Montage &&
            !CollectPlaybackNotifyEvents(m_Playback, dt, output.NotifyEvents))
            return false;

        if (!m_Playing)
        {
            output.LocalPose = m_LocalPose;
            return true;
        }

        output.PlaybackTime = AdvancePlayback(m_Playback, dt);
        output.TransitionSourceTime = AdvancePlayback(m_Transition.Playback, dt);
        output.TransitionElapsed = float(std::min(
            double(m_Transition.Elapsed) + dt,
            double(m_Transition.Duration)
        ));

        const auto weight = IsTransitioning()
            ? output.TransitionElapsed / m_Transition.Duration
            : 1.0f;

        return SamplePose(
            m_Playback,
            m_Transition.Playback,
            weight,
            output.PlaybackTime,
            output.TransitionSourceTime,
            output.LocalPose
        );
    }

    void AnimationLayer::Apply(SLayerEvaluation evaluation)
    {
        if (m_Montage.Montage)
        {
            m_LocalPose = std::move(evaluation.PlaybackPose);
            m_MontagePose = std::move(evaluation.LocalPose);
            m_Montage.Cursor = evaluation.Montage.Cursor;
            if (m_Montage.Cursor.Finished)
            {
                m_Montage = {};
                m_MontagePose.clear();
            }
        }
        else
        {
            m_LocalPose = std::move(evaluation.LocalPose);
        }

        m_NotifyEvents = std::move(evaluation.NotifyEvents);

        if (!evaluation.Advance)
            return;

        if (evaluation.ConsumeInitialNotifies)
            m_Playback.IncludeNotifyStart = false;

        m_Playback.Time = evaluation.PlaybackTime;
        m_Transition.Playback.Time = evaluation.TransitionSourceTime;
        m_Transition.Elapsed = evaluation.TransitionElapsed;

        if (m_Transition.Elapsed >= m_Transition.Duration)
            m_Transition = {};

        const auto duration = GetDuration(m_Playback);

        const auto completed = m_Playback.BlendSpace
            ? m_Playback.Time >= 1.0
            : m_Playback.Time >= double(duration);

        if (!IsTransitioning() &&
            ((!m_Playback.Looping && completed) || (m_Playback.Sequence && duration == 0.0f)))
            m_Playing = false;
    }

    bool AnimationLayer::SamplePose(
        const SPlayback& playback,
        const SPlayback& source,
        const float weight,
        const double time,
        const double sourceTime,
        std::vector<SBoneTransform>& output
    ) const
    {
        std::vector<SBoneTransform> local;

        if (!SamplePlayback(playback, local, time))
            return false;

        if (weight < 1.0f)
        {
            const auto* pose = &m_Transition.Pose;

            std::vector<SBoneTransform> sampled;
            if (source.Sequence || source.BlendSpace)
            {
                if (!SamplePlayback(source, sampled, sourceTime))
                    return false;
                pose = &sampled;
            }

            for (size_t i = 0; i < local.size(); ++i)
                local[i] = SBoneTransform::Blend((*pose)[i], local[i], weight);
        }

        const auto invalidBone = std::ranges::any_of(
            local,
            [](const auto& bone) { return !bone.IsValid(); }
        );

        if (invalidBone)
        {
            EE_CORE_ERROR("Animation blending produced an invalid local transform.")
            return false;
        }

        output = std::move(local);
        return true;
    }

    bool AnimationLayer::StartPlayback(SPlayback playback, const float fadeDuration)
    {
        if (!std::isfinite(fadeDuration) || fadeDuration < 0.0f)
        {
            EE_CORE_ERROR("Animation fade duration must be finite and nonnegative.")
            return false;
        }

        if (fadeDuration > 0.0f)
        {
            std::vector<SBoneTransform> local;

            if (!SamplePlayback(playback, local, playback.Time))
                return false;

            STransition transition;
            transition.Duration = fadeDuration;

            if (!IsTransitioning() && (m_Playback.Sequence || m_Playback.BlendSpace))
                transition.Playback = m_Playback;
            else
                transition.Pose = m_LocalPose;

            m_Transition = std::move(transition);
            m_Playback = std::move(playback);
        }
        else
        {
            if (!SetPlayback(playback, {}, 1.0f, playback.Time, 0.0f))
                return false;
            m_Transition = {};
        }

        m_NotifyEvents.clear();
        m_Assigned = true;
        Resume();

        return true;
    }

    bool AnimationLayer::SamplePlayback(
        const SPlayback& playback,
        std::vector<SBoneTransform>& output,
        const double time
    ) const
    {
        if (playback.Sequence)
            return playback.Sequence->Sample(
                time,
                output,
                m_ReferencePose
            );

        if (playback.BlendSpace)
            return playback.BlendSpace->Sample(
                playback.Values,
                time,
                output,
                m_ReferencePose
            );

        return false;
    }

    double AnimationLayer::AdvancePlayback(const SPlayback& playback, const float dt)
    {
        const auto duration = GetDuration(playback);

        if (playback.BlendSpace)
        {
            const auto elapsed = duration > 0.0f ? double(dt) / duration : 0.0;
            return SampleTime(double(playback.Time) + elapsed, 1.0f, playback.Looping);
        }

        return SampleTime(double(playback.Time) + dt, duration, playback.Looping);
    }

    bool AnimationLayer::SetPlayback(
        const SPlayback& playback,
        const SPlayback& source,
        const float weight,
        const double time,
        const double sourceTime
    )
    {
        std::vector<SBoneTransform> local;

        if (!SamplePose(playback, source, weight, time, sourceTime, local))
            return false;

        m_LocalPose = std::move(local);
        m_Playback = playback;
        m_Playback.Time = time;

        return true;
    }

    float AnimationLayer::GetDuration(const SPlayback& playback)
    {
        if (playback.Sequence)
            return playback.Sequence->GetDuration();

        return playback.BlendSpace
            ? playback.BlendSpace->GetDuration(playback.Values)
            : 0.0f;
    }

    bool AnimationLayer::ExtractPlaybackRootMotion(
        const SPlayback& playback,
        const float dt,
        const uint32_t boneIndex,
        SRootMotionDelta& output,
        const float offset
    ) const
    {
        const auto duration = GetDuration(playback);
        if (dt == 0 || duration == 0 || (!playback.Sequence && !playback.BlendSpace))
        {
            output = {};
            return true;
        }

        auto phase = (playback.BlendSpace
            ? playback.Time
            : playback.Time / duration);
        phase += double(offset) / duration;

        const auto end = phase + double(dt) / duration;

        if (playback.Sequence)
            return playback.Sequence->ExtractRootMotion(
                phase,
                end,
                playback.Looping,
                m_ReferencePose[boneIndex],
                output,
                boneIndex
            );

        return playback.BlendSpace->ExtractRootMotion(
            playback.Values,
            phase,
            end,
            playback.Looping,
            m_ReferencePose[boneIndex],
            output,
            boneIndex
        );
    }

    bool AnimationLayer::ExtractSourceRootMotion(
        const float dt,
        const uint32_t boneIndex,
        SRootMotionDelta& output,
        const float offset
    ) const
    {
        if (!m_Playing || dt == 0)
        {
            output = {};
            return true;
        }

        SRootMotionDelta target;
        if (!ExtractPlaybackRootMotion(m_Playback, dt, boneIndex, target, offset))
            return false;

        if (!IsTransitioning())
        {
            output = target;
            return true;
        }

        SRootMotionDelta source;
        if (!ExtractPlaybackRootMotion(m_Transition.Playback, dt, boneIndex, source, offset))
            return false;

        const auto elapsed = std::min(m_Transition.Elapsed + offset, m_Transition.Duration);
        const auto remaining = m_Transition.Duration - elapsed;
        const auto fadeTime = std::min(dt, remaining);
        const auto startWeight = elapsed / m_Transition.Duration;
        const auto endWeight = (elapsed + fadeTime) / m_Transition.Duration;
        const auto weight = 1.0f - fadeTime / dt * (1.0f - (startWeight + endWeight) * 0.5f);

        const auto result = SRootMotionDelta::Blend(source, target, weight);
        if (!result.IsValid())
        {
            EE_CORE_ERROR("Animation transition produced invalid root motion.")
            return false;
        }

        output = result;
        return true;
    }

    bool AnimationLayer::CollectPlaybackNotifyEvents(
        const SPlayback& playback,
        const float dt,
        std::vector<SAnimationNotifyEvent>& output
    ) const
    {
        const auto duration = GetDuration(playback);
        if (dt == 0.0f || duration == 0.0f)
            return true;

        const auto* source = playback.Sequence.get();
        const AnimationSequence* reference = nullptr;

        if (playback.BlendSpace)
        {
            source = playback.BlendSpace->GetNotifySource(playback.Values);
            reference = playback.BlendSpace->GetSynchronizationSequence().get();
        }

        if (!source) return false;

        if (source->GetNotifies().empty())
            return true;

        const auto phase = playback.BlendSpace
            ? playback.Time
            : playback.Time / duration;

        std::vector<SAnimationNotifyOccurrence> occurrences;

        const auto success = source->CollectNotifyOccurrences(
            phase,
            phase + double(dt) / duration,
            playback.Looping,
            occurrences,
            reference,
            playback.IncludeNotifyStart
        );

        if (!success) return false;

        if (occurrences.empty())
            return true;

        const Ref<const AnimationSequence> retained = playback.Sequence
            ? playback.Sequence
            : Ref<const AnimationSequence>(playback.BlendSpace, source);

        output.reserve(occurrences.size());
        for (const auto& occurrence : occurrences)
            output.push_back({
                retained,
                occurrence.NotifyIndex,
                occurrence.PhaseOffset * duration,
                m_Name
            });

        return true;
    }
}
