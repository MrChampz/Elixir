#include "epch.h"
#include <Engine/Animation/Pose.h>
#include "AnimationTransition.h"

namespace Elixir
{
    bool AnimationTransition::Sample(
        const AnimationPlayback& playback,
        const AnimationPlayback& source,
        const float weight,
        const double time,
        const double sourceTime,
        SLocalPose& output,
        const SLocalPose& referencePose
    ) const
    {
        if (!std::isfinite(weight) || weight < 0 || weight > 1)
            return false;

        SLocalPose local;
        if (!playback.Sample(time, referencePose, local))
            return false;

        if (weight < 1.0f)
        {
            const auto* pose = &Pose;

            SLocalPose sampled;
            if (source.IsValid())
            {
                if (!source.Sample(sourceTime, referencePose, sampled))
                    return false;
                pose = &sampled;
            }

            if (pose->LocalTransforms.size() != local.LocalTransforms.size())
            {
                EE_CORE_ERROR("Animation transition requires matching source and target poses.")
                return false;
            }

            auto curves = pose->Curves;
            if (!curves.Blend(local.Curves, weight)) return false;
            local.Curves = std::move(curves);

            for (size_t i = 0; i < local.LocalTransforms.size(); ++i)
                local.LocalTransforms[i] = SBoneTransform::Blend(
                    pose->LocalTransforms[i],
                    local.LocalTransforms[i],
                    weight
                );
        }

        const auto invalidBone = std::ranges::any_of(
            local.LocalTransforms,
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

    bool AnimationTransition::ExtractRootMotion(
        const AnimationPlayback& playback,
        const SRootMotionIntervalContext& interval,
        SRootMotionDelta& output
    ) const
    {
        if (!interval.IsValid() || !IsValid())
        {
            EE_CORE_ERROR("Root motion requires valid transition timing and interval.")
            return false;
        }

        if (interval.DeltaSeconds == 0)
        {
            output = {};
            return true;
        }

        SRootMotionDelta target;
        if (!playback.ExtractRootMotion(interval, target))
            return false;

        if (!IsActive())
        {
            output = target;
            return true;
        }

        SRootMotionDelta source;
        if (!Playback.ExtractRootMotion(interval, source))
            return false;

        const auto elapsed = std::min(Elapsed + interval.OffsetSeconds, Duration);
        const auto remaining = Duration - elapsed;

        const auto fadeTime = std::min(interval.DeltaSeconds, remaining);
        const auto fadeFraction = fadeTime / interval.DeltaSeconds;

        const auto startWeight = elapsed / Duration;
        const auto endWeight = (elapsed + fadeTime) / Duration;

        const auto weight = 1.0f - fadeFraction * (1.0f - (startWeight + endWeight) * 0.5f);

        const auto result = SRootMotionDelta::Blend(source, target, weight);
        if (!result.IsValid())
        {
            EE_CORE_ERROR("Animation transition produced invalid root motion.")
            return false;
        }

        output = result;
        return true;
    }

    bool AnimationTransition::IsValid() const
    {
        return std::isfinite(Duration) &&
            Duration >= 0 &&
            std::isfinite(Elapsed) &&
            Elapsed >= 0 &&
            Elapsed <= Duration &&
            (!IsActive() || Playback.IsValid() || !Pose.LocalTransforms.empty());
    }
}
