#include "epch.h"
#include "SkeletalMeshInstance.h"

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

    SkeletalMeshInstance::SkeletalMeshInstance(Ref<const SkeletalMesh> mesh)
        : m_Mesh(std::move(mesh))
    {
        if (!m_Mesh)
        {
            EE_CORE_ERROR("SkeletalMeshInstance requires a non-null mesh.")
            return;
        }

        Stop();
    }

    void SkeletalMeshInstance::Stop()
    {
        m_Playing = false;
        m_NotifyEvents.clear();
        m_Playback = {};
        m_Transition = {};

        m_LocalPose.clear();
        m_ModelPose.clear();

        if (!m_Mesh)
            return;

        m_LocalPose = m_Mesh->GetData().ReferencePose;
        m_Mesh->GetSkeleton()->BuildModelPose(m_LocalPose, m_ModelPose);
    }

    float SkeletalMeshInstance::GetDuration(const SPlayback& playback)
    {
        if (playback.Sequence)
            return playback.Sequence->GetDuration();

        return playback.BlendSpace
            ? playback.BlendSpace->GetDuration(playback.Values)
            : 0.0f;
    }

    float SkeletalMeshInstance::GetTime() const
    {
        return m_Playback.BlendSpace
            ? m_Playback.Time * GetDuration(m_Playback)
            : m_Playback.Time;
    }

    float SkeletalMeshInstance::GetNormalizedTime() const
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

    void SkeletalMeshInstance::Resume()
    {
        m_Playing = GetDuration(m_Playback) > 0.0f || IsTransitioning() ||
            (m_Playback.BlendSpace && m_Playback.Looping);
    }

    bool SkeletalMeshInstance::SamplePlayback(
        const SPlayback& playback,
        std::vector<SBoneTransform>& output,
        const double time
    ) const
    {
        if (playback.Sequence)
            return playback.Sequence->Sample(
                time,
                output,
                m_Mesh->GetData().ReferencePose
            );

        if (playback.BlendSpace)
            return playback.BlendSpace->Sample(
                playback.Values,
                time,
                output,
                m_Mesh->GetData().ReferencePose
            );

        return false;
    }

    double SkeletalMeshInstance::AdvancePlayback(const SPlayback& playback, const float dt)
    {
        const auto duration = GetDuration(playback);

        if (playback.BlendSpace)
        {
            const auto elapsed = duration > 0.0f ? double(dt) / duration : 0.0;
            return SampleTime(double(playback.Time) + elapsed, 1.0f, playback.Looping);
        }

        return SampleTime(double(playback.Time) + dt, duration, playback.Looping);
    }

    bool SkeletalMeshInstance::Evaluate(
        const SPlayback& playback,
        const SPlayback& source,
        const float weight,
        const double time,
        const double sourceTime
    )
    {
        std::vector<SBoneTransform> local;
        std::vector<glm::mat4> model;

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

        if (!m_Mesh->GetSkeleton()->BuildModelPose(local, model))
            return false;

        m_LocalPose = std::move(local);
        m_ModelPose = std::move(model);
        m_Playback = playback;
        m_Playback.Time = time;

        return true;
    }

    bool SkeletalMeshInstance::StartPlayback(SPlayback playback, const float fadeDuration)
    {
        if (!std::isfinite(fadeDuration) || fadeDuration < 0.0f)
        {
            EE_CORE_ERROR("Animation fade duration must be finite and nonnegative.")
            return false;
        }

        if (fadeDuration > 0.0f)
        {
            std::vector<SBoneTransform> local;
            std::vector<glm::mat4> model;

            if (!SamplePlayback(playback, local, playback.Time) ||
                !m_Mesh->GetSkeleton()->BuildModelPose(local, model))
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
            if (!Evaluate(playback, {}, 1.0f, playback.Time, 0.0f))
                return false;
            m_Transition = {};
        }

        m_NotifyEvents.clear();
        Resume();

        return true;
    }

    bool SkeletalMeshInstance::Play(Ref<const AnimationSequence> sequence, const bool looping)
    {
        return Play(std::move(sequence), 0.0f, looping);
    }

    bool SkeletalMeshInstance::Play(
        Ref<const AnimationSequence> sequence,
        const float fadeDuration,
        const bool looping
    )
    {
        if (!m_Mesh || !sequence || (m_Mesh->GetSkeleton() != sequence->GetSkeleton() &&
                !m_Mesh->GetSkeleton()->IsCompatible(*sequence->GetSkeleton())))
        {
            EE_CORE_ERROR(
                "SkeletalMeshInstance requires an animation with a compatible skeleton."
            )
            return false;
        }

        return StartPlayback(
            { .Sequence = std::move(sequence), .Looping = looping },
            fadeDuration
        );
    }

    bool SkeletalMeshInstance::Play(
        Ref<const BlendSpace> blendSpace,
        const std::span<const float> values,
        const float fadeDuration,
        const bool looping
    )
    {
        if (!m_Mesh || !blendSpace || !blendSpace->HasValidCoordinates(values) ||
            (m_Mesh->GetSkeleton() != blendSpace->GetSkeleton() &&
                !m_Mesh->GetSkeleton()->IsCompatible(*blendSpace->GetSkeleton())))
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

    bool SkeletalMeshInstance::SetBlendSpaceValue(const std::span<const float> values)
    {
        if (!m_Playback.BlendSpace || !m_Playback.BlendSpace->HasValidCoordinates(values))
        {
            EE_CORE_ERROR("Blend coordinates must match the active blend space and be finite.")
            return false;
        }

        std::ranges::copy(values, m_Playback.Values.begin());
        return true;
    }

    bool SkeletalMeshInstance::Seek(const float time)
    {
        if ((!m_Playback.Sequence && !m_Playback.BlendSpace) || !std::isfinite(time))
        {
            EE_CORE_ERROR("SkeletalMeshInstance requires playback and a finite seek time.")
            return false;
        }

        const auto duration = GetDuration(m_Playback);
        const auto sampled = SampleTime(time, duration, m_Playback.Looping);

        const auto targetTime = m_Playback.BlendSpace && duration > 0.0f
            ? sampled / duration
            : sampled;

        if (!Evaluate(m_Playback, {}, 1.0f, targetTime, 0.0f))
            return false;

        m_Transition = {};
        m_Playback.IncludeNotifyStart = false;
        m_NotifyEvents.clear();

        return true;
    }

    bool SkeletalMeshInstance::CollectNotifyEvents(
        const float dt,
        std::vector<SAnimationNotifyEvent>& output
    ) const
    {
        const auto duration = GetDuration(m_Playback);
        if (dt == 0.0f || duration == 0.0f)
            return true;

        const auto* source = m_Playback.Sequence.get();
        const AnimationSequence* reference = nullptr;

        if (m_Playback.BlendSpace)
        {
            source = m_Playback.BlendSpace->GetNotifySource(m_Playback.Values);
            reference = m_Playback.BlendSpace->GetSynchronizationSequence().get();
        }

        if (!source) return false;

        if (source->GetNotifies().empty())
            return true;

        const auto phase = m_Playback.BlendSpace
            ? m_Playback.Time
            : m_Playback.Time / duration;

        std::vector<SAnimationNotifyOccurrence> occurrences;

        const auto success = source->CollectNotifyOccurrences(
            phase,
            phase + double(dt) / duration,
            m_Playback.Looping,
            occurrences,
            reference,
            m_Playback.IncludeNotifyStart
        );

        if (!success) return false;

        if (occurrences.empty())
            return true;

        const Ref<const AnimationSequence> retained = m_Playback.Sequence
            ? m_Playback.Sequence
            : Ref<const AnimationSequence>(m_Playback.BlendSpace, source);

        output.reserve(occurrences.size());
        for (const auto& occurrence : occurrences)
            output.push_back({
                retained,
                occurrence.NotifyIndex,
                occurrence.PhaseOffset * duration
            });

        return true;
    }

    bool SkeletalMeshInstance::Update(const float dt)
    {
        if (!std::isfinite(dt) || dt < 0.0f)
        {
            EE_CORE_ERROR("SkeletalMeshInstance delta time must be finite and nonnegative.")
            return false;
        }

        if (!m_Playing)
        {
            m_NotifyEvents.clear();
            return true;
        }

        std::vector<SAnimationNotifyEvent> events;
        if (!CollectNotifyEvents(dt, events))
            return false;

        const auto time = AdvancePlayback(m_Playback, dt);
        const auto sourceTime = AdvancePlayback(m_Transition.Playback, dt);

        const auto elapsed = float(std::min(
            double(m_Transition.Elapsed) + dt,
            double(m_Transition.Duration)
        ));

        const auto weight = IsTransitioning()
            ? elapsed / m_Transition.Duration
            : 1.0f;

        if (!Evaluate(m_Playback, m_Transition.Playback, weight, time, sourceTime))
            return false;

        m_NotifyEvents = std::move(events);

        if (dt > 0.0f)
            m_Playback.IncludeNotifyStart = false;

        m_Transition.Playback.Time = sourceTime;
        m_Transition.Elapsed = elapsed;

        if (weight >= 1.0f)
            m_Transition = {};

        if (!IsTransitioning() && ((!m_Playback.Looping && GetNormalizedTime() >= 1.0f) ||
                (m_Playback.Sequence && GetDuration(m_Playback) == 0.0f)))
            m_Playing = false;

        return true;
    }

    bool SkeletalMeshInstance::SetLocalPose(const std::span<const SBoneTransform> pose)
    {
        if (!m_Mesh)
        {
            EE_CORE_ERROR("SkeletalMeshInstance requires a mesh before setting a pose.")
            return false;
        }

        std::vector<glm::mat4> model;
        if (!m_Mesh->GetSkeleton()->BuildModelPose(pose, model))
            return false;

        std::vector local(pose.begin(), pose.end());
        m_LocalPose = std::move(local);
        m_ModelPose = std::move(model);

        m_Playback = {};
        m_Transition = {};
        m_Playing = false;
        m_NotifyEvents.clear();

        return true;
    }
}
