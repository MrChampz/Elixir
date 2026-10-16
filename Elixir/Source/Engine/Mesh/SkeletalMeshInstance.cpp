#include "epch.h"
#include "SkeletalMeshInstance.h"

namespace Elixir
{
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
        m_LocalPose.clear();
        m_ModelPose.clear();

        if (!m_Mesh) return;

        m_LocalPose = m_Mesh->GetData().ReferencePose;
        m_Mesh->GetSkeleton()->BuildModelPose(m_LocalPose, m_ModelPose);
    }

    bool SkeletalMeshInstance::Evaluate(
        const Ref<const AnimationSequence>& sequence,
        const float time
    )
    {
        std::vector<SBoneTransform> local;
        std::vector<glm::mat4> model;

        if (!sequence->Sample(time, local, m_Mesh->GetData().ReferencePose) ||
            !m_Mesh->GetSkeleton()->BuildModelPose(local, model))
            return false;

        m_LocalPose = std::move(local);
        m_ModelPose = std::move(model);
        m_Time = time;

        return true;
    }

    bool SkeletalMeshInstance::Play(Ref<const AnimationSequence> sequence, const bool looping)
    {
        if (!m_Mesh ||
            !sequence ||
            !m_Mesh->GetSkeleton()->IsCompatible(*sequence->GetSkeleton()))
        {
            EE_CORE_ERROR("SkeletalMeshInstance requires an animation with a compatible skeleton.")
            return false;
        }

        if (!Evaluate(sequence, 0)) return false;

        m_Sequence = std::move(sequence);
        m_Looping = looping;
        m_Playing = m_Sequence->GetDuration() > 0;

        return true;
    }

    bool SkeletalMeshInstance::Seek(const float time)
    {
        if (!m_Sequence || !std::isfinite(time))
        {
            EE_CORE_ERROR("SkeletalMeshInstance requires a sequence and a finite seek time.")
            return false;
        }

        const float duration = m_Sequence->GetDuration();
        float sampled = duration == 0 ? 0 : std::clamp(time, 0.0f, duration);

        if (m_Looping && duration > 0)
        {
            sampled = std::fmod(time, duration);
            if (sampled < 0) sampled += duration;
        }

        return Evaluate(m_Sequence, sampled);
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
        const float time = m_Looping && duration > 0
            ? float(std::fmod(next, double(duration)))
            : float(std::min(next, double(duration)));

        if (!Evaluate(m_Sequence, time)) return false;

        if (!m_Looping && next >= duration)
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
        m_Playing = false;
        m_Time = 0;

        return true;
    }
}
