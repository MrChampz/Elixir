#include "epch.h"
#include "SkeletalMeshInstance.h"

namespace Elixir
{
    namespace
    {
        /**
         * @brief Wrap or clamp a playback time to the sequence duration.
         * @param time Playback time in seconds.
         * @param duration Sequence duration in seconds.
         * @param looping Wrap the time instead of clamping it.
         */
        float SampleTime(const double time, const float duration, const bool looping)
        {
            if (duration == 0.0f) return 0.0f;
            if (!looping) return float(std::clamp(time, 0.0, double(duration)));

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
        m_Time = 0;

        m_Sequence.reset();
        m_Transition = {};
        m_LocalPose.clear();
        m_ModelPose.clear();

        if (!m_Mesh) return;

        m_LocalPose = m_Mesh->GetData().ReferencePose;
        m_Mesh->GetSkeleton()->BuildModelPose(m_LocalPose, m_ModelPose);
    }

    bool SkeletalMeshInstance::Evaluate(
        const Ref<const AnimationSequence>& sequence,
        const float time,
        const float sourceTime,
        const float weight
    )
    {
        std::vector<SBoneTransform> local;
        std::vector<glm::mat4> model;

        if (!sequence->Sample(time, local, m_Mesh->GetData().ReferencePose)) return false;

        if (weight < 1.0f)
        {
            std::vector<SBoneTransform> sampledSource;
            const auto* source = &m_Transition.Pose;
            if (m_Transition.Sequence)
            {
                const auto success = m_Transition.Sequence->Sample(
                    sourceTime,
                    sampledSource,
                    m_Mesh->GetData().ReferencePose
                );
                if (!success) return false;

                source = &sampledSource;
            }

            for (size_t i = 0; i < local.size(); ++i)
            {
                const auto& from = (*source)[i];
                auto& to = local[i];

                to.Translation = glm::mix(from.Translation, to.Translation, weight);
                to.Scale = glm::mix(from.Scale, to.Scale, weight);
                to.Rotation = glm::normalize(glm::slerp(from.Rotation, to.Rotation, weight));
            }
        }

        if (!m_Mesh->GetSkeleton()->BuildModelPose(local, model))
            return false;

        m_LocalPose = std::move(local);
        m_ModelPose = std::move(model);
        m_Time = time;

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
        if (!std::isfinite(fadeDuration) || fadeDuration < 0.0f)
        {
            EE_CORE_ERROR("Animation fade duration must be finite and nonnegative.")
            return false;
        }

        if (!m_Mesh || !sequence ||
            !m_Mesh->GetSkeleton()->IsCompatible(*sequence->GetSkeleton()))
        {
            EE_CORE_ERROR("SkeletalMeshInstance requires an animation with a compatible skeleton.")
            return false;
        }

        if (fadeDuration > 0.0f)
        {
            // Validate the target without changing the visible pose or playback state.
            std::vector<SBoneTransform> local;
            std::vector<glm::mat4> model;

            const auto sampled = sequence->Sample(0.0f, local, m_Mesh->GetData().ReferencePose);
            if (!sampled || !m_Mesh->GetSkeleton()->BuildModelPose(local, model))
                return false;

            STransition transition;
            transition.Duration = fadeDuration;
            transition.Time = m_Time;
            transition.Looping = m_Looping;

            if (m_Sequence && !IsTransitioning())
                transition.Sequence = m_Sequence;
            else
                transition.Pose = m_LocalPose;

            m_Transition = std::move(transition);
            m_Time = 0.0f;
        }
        else
        {
            if (!Evaluate(sequence, 0.0f)) return false;
            m_Transition = {};
        }

        m_Sequence = std::move(sequence);
        m_Looping = looping;
        m_Playing = m_Sequence->GetDuration() > 0 || IsTransitioning();

        return true;
    }

    bool SkeletalMeshInstance::Seek(const float time)
    {
        if (!m_Sequence || !std::isfinite(time))
        {
            EE_CORE_ERROR("SkeletalMeshInstance requires a sequence and a finite seek time.")
            return false;
        }

        const auto sampled = SampleTime(time, m_Sequence->GetDuration(), m_Looping);
        if (!Evaluate(m_Sequence, sampled)) return false;

        m_Transition = {};
        return true;
    }

    bool SkeletalMeshInstance::Update(const float deltaSeconds)
    {
        if (!std::isfinite(deltaSeconds) || deltaSeconds < 0)
        {
            EE_CORE_ERROR("SkeletalMeshInstance delta time must be finite and nonnegative.")
            return false;
        }

        if (!m_Playing || !m_Sequence) return true;

        const double next = double(m_Time) + deltaSeconds;
        const float duration = m_Sequence->GetDuration();
        const auto time = SampleTime(next, duration, m_Looping);

        const auto elapsed = float(std::min(
            double(m_Transition.Elapsed) + deltaSeconds,
            double(m_Transition.Duration)
        ));

        const auto weight = IsTransitioning()
            ? elapsed / m_Transition.Duration
            : 1.0f;

        const auto sourceTime = m_Transition.Sequence
            ? SampleTime(
                double(m_Transition.Time) + deltaSeconds,
                m_Transition.Sequence->GetDuration(),
                m_Transition.Looping
            )
            : 0.0f;

        if (!Evaluate(m_Sequence, time, sourceTime, weight)) return false;

        if (weight >= 1.0f)
            m_Transition = {};
        else
        {
            m_Transition.Time = sourceTime;
            m_Transition.Elapsed = elapsed;
        }

        if (!IsTransitioning() && (duration == 0.0f || (!m_Looping && next >= duration)))
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
        if (!m_Mesh->GetSkeleton()->BuildModelPose(pose, model)) return false;

        std::vector local(pose.begin(), pose.end());
        m_LocalPose = std::move(local);
        m_ModelPose = std::move(model);
        m_Sequence.reset();
        m_Transition = {};
        m_Playing = false;
        m_Time = 0;

        return true;
    }
}
