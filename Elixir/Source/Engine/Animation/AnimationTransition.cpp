#include "epch.h"
#include "AnimationTransition.h"

#include <Engine/Animation/Pose.h>

#include <limits>

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
        SAnimationSample evaluation;
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
        SAnimationSample& output,
        const SLocalPose& referencePose
    ) const
    {
        if (!std::isfinite(weight) || weight < 0 || weight > 1)
            return false;

        SAnimationSample result;
        if (!playback.Sample(time, referencePose, result))
            return false;

        if (weight < 1.0f && Mode == EAnimationTransitionMode::Inertialization)
        {
            if (!m_Inertialization.Apply(double(weight) * Duration, Duration, result))
                return false;
        }
        else if (weight < 1.0f)
        {
            const auto* pose = &SourceSample.Pose;
            auto curves = SourceSample.Curves;

            SAnimationSample sampled;
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

    bool AnimationTransition::CaptureInertialization(
        AnimationPlayback& playback,
        const SAnimationSample& sourceSample,
        const SAnimationSample* previous,
        const float deltaSeconds,
        const SLocalPose& referencePose
    )
    {
        SAnimationSample targetSample, nextSample;

        // Resolve entry transitions before measuring motion.
        // Retain the resolved state for destination playback.
        auto destination = playback;

        const auto initialPhase = destination.Advance(0, referencePose);

        if (!destination.Sample(initialPhase, referencePose, targetSample))
            return false;

        destination.Apply(initialPhase);

        // A short forward probe measures destination motion without advancing its clock.
        const auto duration = destination.GetDuration();

        auto probe = destination;
        const auto probeSeconds = duration > 0
            ? std::min(
                  1.0f / 120.0f,
                  std::max(duration * 0.01f, std::numeric_limits<float>::denorm_min())
              )
            : 1.0f / 120.0f;

        const auto nextPhase = probe.Advance(probeSeconds, referencePose);
        if (!probe.Sample(nextPhase, referencePose, nextSample))
            return false;

        AnimationInertialization inertia;
        const auto captured = inertia.Capture(
            sourceSample,
            previous,
            deltaSeconds,
            targetSample,
            nextSample,
            probeSeconds
        );

        if (!captured) return false;

        m_Inertialization = std::move(inertia);
        playback = std::move(destination);

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

        if (!IsActive() || Mode == EAnimationTransitionMode::Inertialization)
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
        if (Mode != EAnimationTransitionMode::CrossFade &&
            Mode != EAnimationTransitionMode::Inertialization)
            return false;

        if (!std::isfinite(Duration) || Duration < 0 ||
            !std::isfinite(Elapsed) || Elapsed < 0 || Elapsed > Duration)
            return false;

        if (!IsActive()) return true;

        if (Mode == EAnimationTransitionMode::Inertialization)
            return m_Inertialization.IsCaptured();

        return Playback.IsValid() || !SourceSample.Pose.LocalTransforms.empty();
    }
}
