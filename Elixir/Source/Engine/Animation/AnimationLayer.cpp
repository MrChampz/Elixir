#include "epch.h"
#include "AnimationLayer.h"

namespace Elixir
{
    AnimationLayer::AnimationLayer(
        std::string name,
        Ref<const BoneMask> mask,
        const float weight,
        const SLocalPose& referencePose
    ) : m_Name(std::move(name)),
        m_Weight(weight),
        m_Mask(std::move(mask)),
        m_Player(m_Name, m_Mask->GetSkeleton(), referencePose)
    {
        const auto notifiesEnabled = m_Weight > 0 && m_Mask && m_Mask->HasInfluence();
        m_Player.SetNotifiesEnabled(notifiesEnabled);
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

        m_Player.SetNotifiesEnabled(weight > 0 && m_Mask->HasInfluence());
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
            !pose.IsValid(m_Player.GetReferencePose().LocalTransforms.size()))
        {
            EE_CORE_ERROR("An additive reference requires one valid local transform per bone.")
            return false;
        }

        SLocalPose reference = pose;
        m_AdditiveReferencePose = std::move(reference);

        return true;
    }

    bool AnimationLayer::BlendPose(SLocalPose& output, const SLocalPose& pose) const
    {
        if (!m_Player.HasPose() || m_Weight == 0 || !m_Mask->HasInfluence())
            return true;

        return output.Blend(
            pose,
            m_Mask->GetWeights(),
            m_Weight,
            m_BlendMode,
            GetAdditiveReferencePose()
        );
    }

    bool AnimationLayer::BlendCurves(
        AnimationCurveValues& output,
        const AnimationCurveValues& curves
    ) const
    {
        if (!m_Player.HasPose() || m_Weight == 0 || !m_Mask->HasInfluence())
            return true;

        return output.ApplyLayer(
            curves,
            m_Weight,
            m_BlendMode == EAnimationLayerBlendMode::Additive
        );
    }
}
