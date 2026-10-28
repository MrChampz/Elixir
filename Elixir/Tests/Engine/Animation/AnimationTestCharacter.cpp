#include <Engine/Animation/Pose.h>
#include "AnimationTestNodes.h"
#include "AnimationTestCharacter.h"

namespace Elixir
{
    Ref<AnimationTestCharacter> AnimationTestCharacter::Create(Ref<const SkeletalMesh> mesh)
    {
        if (!mesh)
        {
            EE_CORE_ERROR("Cannot create a AnimationTestCharacter with invalid mesh!")
            return nullptr;
        }

        auto instance = Ref<AnimationTestCharacter>(new AnimationTestCharacter(std::move(mesh)));
        return instance->m_Animation ? instance : nullptr;
    }

    bool AnimationTestCharacter::Play(Ref<const AnimationNode> source, const bool looping)
    {
        return m_Animation->Play(MakeNode(std::move(source)), looping);
    }

    bool AnimationTestCharacter::ReplaceAnimation(Ref<Animation> animation)
    {
        if (!animation || !m_Mesh->GetSkeleton()->IsCompatible(*animation->GetSkeleton()))
        {
            EE_CORE_ERROR("Skeletal mesh requires an animation with a compatible skeleton.")
            return false;
        }
        m_Animation = std::move(animation);
        return true;
    }

    bool AnimationTestCharacter::Play(Ref<const AnimationNode> source, const float fadeDuration, const bool looping)
    {
        return m_Animation->Play(MakeNode(std::move(source)), fadeDuration, looping);
    }

    void AnimationTestCharacter::Stop()
    {
        m_Animation->Stop();
    }

    void AnimationTestCharacter::Resume()
    {
        m_Animation->Resume();
    }

    bool AnimationTestCharacter::Seek(const float time)
    {
        return m_Animation->Seek(time);
    }

    bool AnimationTestCharacter::Update(const float dt)
    {
        return m_Animation->Update(dt);
    }

    float AnimationTestCharacter::GetTime() const
    {
        return m_Animation->GetTime();
    }

    float AnimationTestCharacter::GetNormalizedTime() const
    {
        return m_Animation->GetNormalizedTime();
    }

    bool AnimationTestCharacter::IsPlaying() const
    {
        return m_Animation->IsPlaying();
    }

    bool AnimationTestCharacter::SetInputs(const std::span<const float> inputs)
    {
        return m_Animation->SetInputs(inputs);
    }

    bool AnimationTestCharacter::SetRootMotionEnabled(const bool enabled, const std::string_view boneName)
    {
        return m_Animation->SetRootMotionEnabled(enabled, boneName);
    }

    bool AnimationTestCharacter::SetLocalPose(const SLocalPose& pose)
    {
        return m_Animation->SetLocalPose(pose);
    }

    bool AnimationTestCharacter::AddLayer(std::string name, Ref<const BoneMask> mask, const float weight)
    {
        return m_Animation->AddLayer(std::move(name), std::move(mask), weight);
    }

    AnimationLayer* AnimationTestCharacter::GetLayer(const std::string_view name)
    {
        return m_Animation->GetLayer(name);
    }

    const AnimationLayer* AnimationTestCharacter::GetLayer(const std::string_view name) const
    {
        return m_Animation->GetLayer(name);
    }

    bool AnimationTestCharacter::SetLayerWeight(const std::string_view name, const float weight)
    {
        return m_Animation->SetLayerWeight(name, weight);
    }

    std::optional<float> AnimationTestCharacter::GetLayerWeight(const std::string_view name) const
    {
        return m_Animation->GetLayerWeight(name);
    }

    bool AnimationTestCharacter::RemoveLayer(const std::string_view name)
    {
        return m_Animation->RemoveLayer(name);
    }

    void AnimationTestCharacter::ClearLayers()
    {
        m_Animation->ClearLayers();
    }

    AnimationTestCharacter::AnimationTestCharacter(Ref<const SkeletalMesh> mesh)
        : m_Mesh(std::move(mesh)),
          m_Animation(Animation::Create(
              m_Mesh->GetSkeleton(),
              m_Mesh->GetData().ReferencePose
          )) {}
}
