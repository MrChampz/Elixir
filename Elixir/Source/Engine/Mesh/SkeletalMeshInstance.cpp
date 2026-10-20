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
        float SampleTime(const double time, const float duration, const bool looping)
        {
            if (duration == 0.0f)
                return 0.0f;
            if (!looping)
                return float(std::clamp(time, 0.0, double(duration)));

            const auto wrapped = std::fmod(time, double(duration));
            return float(wrapped < 0.0 ? wrapped + duration : wrapped);
        }
    }

    SkeletalMeshInstance::SkeletalMeshInstance(Ref<const SkeletalMesh> mesh)
    {
        SetMesh(std::move(mesh));
    }

    void SkeletalMeshInstance::SetMesh(Ref<const SkeletalMesh> mesh)
    {
        m_Mesh = std::move(mesh);
        Stop();
    }

    void SkeletalMeshInstance::Stop()
    {
        m_Playing = false;
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

        if (playback.Space2D)
            return playback.Space2D->GetDuration(playback.Value2D);

        return playback.BlendSpace
            ? playback.BlendSpace->GetDuration(playback.Value)
            : 0.0f;
    }

    float SkeletalMeshInstance::GetTime() const
    {
        return (m_Playback.BlendSpace || m_Playback.Space2D)
           ? m_Playback.Time * GetDuration(m_Playback)
           : m_Playback.Time;
    }

    float SkeletalMeshInstance::GetNormalizedTime() const
    {
        if (!m_Playback.Sequence && !m_Playback.BlendSpace && !m_Playback.Space2D)
            return 0.0f;

        const auto duration = GetDuration(m_Playback);

        if (m_Playback.BlendSpace || m_Playback.Space2D)
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
            ((m_Playback.BlendSpace || m_Playback.Space2D) && m_Playback.Looping);
    }

    bool SkeletalMeshInstance::SamplePlayback(
        const SPlayback& playback,
        std::vector<SBoneTransform>& output
    ) const
    {
        if (playback.Sequence)
            return playback.Sequence->Sample(
                playback.Time,
                output,
                m_Mesh->GetData().ReferencePose
            );

        if (playback.BlendSpace)
            return playback.BlendSpace->Sample(
                playback.Value,
                playback.Time,
                output,
                m_Mesh->GetData().ReferencePose
            );

        if (playback.Space2D)
            return playback.Space2D->Sample(
                playback.Value2D,
                playback.Time,
                output,
                m_Mesh->GetData().ReferencePose
            );

        return false;
    }

    SkeletalMeshInstance::SPlayback SkeletalMeshInstance::AdvancePlayback(
        SPlayback playback,
        const float dt
    ) const
    {
        const auto duration = GetDuration(playback);
        if (playback.BlendSpace || playback.Space2D)
        {
            const auto elapsed = duration > 0.0f
                ? double(dt) / duration
                : 0.0;

            playback.Time = SampleTime(
                double(playback.Time) + elapsed,
                1.0f,
                playback.Looping
            );
        }
        else
        {
            playback.Time = SampleTime(
                double(playback.Time) + dt,
                duration,
                playback.Looping
            );
        }

        return playback;
    }

    bool SkeletalMeshInstance::Evaluate(
        const SPlayback& playback,
        const SPlayback& source,
        const float weight
    )
    {
        std::vector<SBoneTransform> local;
        std::vector<glm::mat4> model;

        if (!SamplePlayback(playback, local))
            return false;

        if (weight < 1.0f)
        {
            const auto* pose = &m_Transition.Pose;

            std::vector<SBoneTransform> sampled;
            if (source.Sequence || source.BlendSpace || source.Space2D)
            {
                if (!SamplePlayback(source, sampled))
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

            if (!SamplePlayback(playback, local) ||
                !m_Mesh->GetSkeleton()->BuildModelPose(local, model))
                return false;

            STransition transition;
            transition.Duration = fadeDuration;

            if (!IsTransitioning() &&
                (m_Playback.Sequence || m_Playback.BlendSpace || m_Playback.Space2D))
                transition.Playback = m_Playback;
            else
                transition.Pose = m_LocalPose;

            m_Transition = std::move(transition);
            m_Playback = std::move(playback);
        }
        else
        {
            if (!Evaluate(playback, {}, 1.0f))
                return false;
            m_Transition = {};
        }

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
        Ref<const BlendSpace1D> blendSpace,
        const float value,
        const float fadeDuration,
        const bool looping
    )
    {
        if (!m_Mesh || !blendSpace || !std::isfinite(value) ||
            (m_Mesh->GetSkeleton() != blendSpace->GetSkeleton() &&
                !m_Mesh->GetSkeleton()->IsCompatible(*blendSpace->GetSkeleton())))
        {
            EE_CORE_ERROR(
                "Blend-space playback requires a compatible mesh and finite coordinate."
            )
            return false;
        }

        return StartPlayback(
            { .BlendSpace = std::move(blendSpace), .Value = value, .Looping = looping },
            fadeDuration
        );
    }

    bool SkeletalMeshInstance::SetBlendSpaceValue(const float value)
    {
        if (!m_Playback.BlendSpace || !std::isfinite(value))
        {
            EE_CORE_ERROR(
                "Blend coordinate requires active blend-space playback and a finite value."
            )
            return false;
        }

        m_Playback.Value = value;
        return true;
    }

    bool SkeletalMeshInstance::Play(
        Ref<const BlendSpace2D> blendSpace,
        const glm::vec2 value,
        const float fadeDuration,
        const bool looping
    )
    {
        if (!m_Mesh || !blendSpace || !std::isfinite(value.x) || !std::isfinite(value.y) ||
            (m_Mesh->GetSkeleton() != blendSpace->GetSkeleton() &&
                !m_Mesh->GetSkeleton()->IsCompatible(*blendSpace->GetSkeleton())))
        {
            EE_CORE_ERROR(
                "Blend-space playback requires a compatible mesh and finite coordinates."
            )
            return false;
        }

        return StartPlayback(
            { .Space2D = std::move(blendSpace), .Value2D = value, .Looping = looping },
            fadeDuration
        );
    }

    bool SkeletalMeshInstance::SetBlendSpaceValue(const glm::vec2 value)
    {
        if (!m_Playback.Space2D || !std::isfinite(value.x) || !std::isfinite(value.y))
        {
            EE_CORE_ERROR("Two-axis coordinates require active 2D playback and finite values.")
            return false;
        }

        m_Playback.Value2D = value;
        return true;
    }

    bool SkeletalMeshInstance::Seek(const float time)
    {
        if ((!m_Playback.Sequence && !m_Playback.BlendSpace && !m_Playback.Space2D) ||
            !std::isfinite(time))
        {
            EE_CORE_ERROR("SkeletalMeshInstance requires playback and a finite seek time.")
            return false;
        }

        auto playback = m_Playback;
        const auto duration = GetDuration(playback);
        const auto sampled = SampleTime(time, duration, playback.Looping);

        playback.Time = (playback.BlendSpace || playback.Space2D) && duration > 0.0f
            ? sampled / duration
            : sampled;

        if (!Evaluate(playback, {}, 1.0f))
            return false;

        m_Transition = {};
        return true;
    }

    bool SkeletalMeshInstance::Update(const float dt)
    {
        if (!std::isfinite(dt) || dt < 0.0f)
        {
            EE_CORE_ERROR("SkeletalMeshInstance delta time must be finite and nonnegative.")
            return false;
        }

        if (!m_Playing) return true;

        const auto playback = AdvancePlayback(m_Playback, dt);
        const auto source = AdvancePlayback(m_Transition.Playback, dt);

        const auto elapsed = float(std::min(
            double(m_Transition.Elapsed) + dt,
            double(m_Transition.Duration)
        ));

        const auto weight = IsTransitioning()
            ? elapsed / m_Transition.Duration
            : 1.0f;

        if (!Evaluate(playback, source, weight))
            return false;

        m_Transition.Playback = source;
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

        return true;
    }
}
