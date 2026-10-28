#include "epch.h"
#include "AnimationTransition.h"

#include <Engine/Animation/Pose.h>

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
        SAnimationEvaluation evaluation;
        if (!Sample(playback, source, weight, time, sourceTime, evaluation, referencePose))
            return false;

        output = std::move(evaluation.Pose);
        return true;
    }

    bool AnimationTransition::Sample(
        const AnimationPlayback& playback,
        const AnimationPlayback& source,
        const float weight,
        const double time,
        const double sourceTime,
        SAnimationEvaluation& output,
        const SLocalPose& referencePose
    ) const
    {
        if (!std::isfinite(weight) || weight < 0 || weight > 1)
            return false;

        SAnimationEvaluation result;
        if (!playback.Sample(time, referencePose, result))
            return false;

        if (weight < 1.0f)
        {
            const auto* pose = &Pose;
            auto curves = Curves;

            SAnimationEvaluation sampled;
            if (source.IsValid())
            {
                if (!source.Sample(sourceTime, referencePose, sampled))
                    return false;

                pose = &sampled.Pose;
                curves = std::move(sampled.Curves);
            }

            if (pose->LocalTransforms.size() != result.Pose.LocalTransforms.size())
            {
                EE_CORE_ERROR("Animation transition requires matching source and target poses.")
                return false;
            }

            if (!curves.Blend(result.Curves, weight))
                return false;

            result.Curves = std::move(curves);

            for (size_t i = 0; i < result.Pose.LocalTransforms.size(); ++i)
                result.Pose.LocalTransforms[i] = SBoneTransform::Blend(
                    pose->LocalTransforms[i],
                    result.Pose.LocalTransforms[i],
                    weight
                );
        }

        const auto invalidBone = std::ranges::any_of(
            result.Pose.LocalTransforms,
            [](const auto& bone) { return !bone.IsValid(); }
        );

        if (invalidBone)
        {
            EE_CORE_ERROR("Animation blending produced an invalid local transform.")
            return false;
        }

        output = std::move(result);
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
