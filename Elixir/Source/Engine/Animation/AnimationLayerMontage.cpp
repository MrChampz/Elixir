#include "epch.h"
#include "AnimationLayer.h"

namespace Elixir
{
    bool AnimationLayer::PlayMontage(
        Ref<const AnimationMontage> montage
    )
    {
        if (!m_Skeleton ||
            !montage ||
            (m_Skeleton != montage->GetSequence()->GetSkeleton() &&
                !m_Skeleton->IsCompatible(*montage->GetSequence()->GetSkeleton())))
        {
            EE_CORE_ERROR("Montage playback requires a compatible sequence and skeleton.")
            return false;
        }

        if (m_Montage.Montage && !m_Montage.Montage->IsInterruptible())
        {
            EE_CORE_ERROR("The active montage does not allow interruption.")
            return false;
        }

        SMontagePlayback playback;
        playback.Montage = std::move(montage);
        playback.Playing = true;
        if (m_Montage.Montage && playback.Montage->GetBlendIn() > 0)
            playback.SourcePose = GetLocalPose();

        std::vector<SBoneTransform> sampled;
        if (!playback.Montage->GetSequence()->Sample(
                0, sampled, m_ReferencePose))
            return false;

        auto candidate = *this;
        candidate.m_Montage = std::move(playback);

        auto pose = m_LocalPose;
        if (!candidate.BlendMontage(0, pose))
            return false;

        m_Montage = std::move(candidate.m_Montage);
        m_MontagePose = std::move(pose);
        m_NotifyEvents.clear();

        return true;
    }

    bool AnimationLayer::StopMontage(const std::optional<float> blendOut)
    {
        if (blendOut && (!std::isfinite(*blendOut) || *blendOut < 0))
        {
            EE_CORE_ERROR("Montage exit duration must be finite and nonnegative.")
            return false;
        }

        const auto montage = m_Montage.Montage;
        if (!montage) return true;

        if (!montage->IsInterruptible())
        {
            EE_CORE_ERROR("The active montage does not allow interruption.")
            return false;
        }

        const auto duration = blendOut.value_or(montage->GetBlendOut());
        if (duration == 0)
        {
            m_Montage = {};
            m_MontagePose.clear();
            m_NotifyEvents.clear();
            return true;
        }

        // Repeated stop requests must not restart or prolong the exit.
        if (m_Montage.StopTime >= 0)
            return true;

        m_Montage.StopWeight = GetMontageWeight();
        m_Montage.StopTime = m_Montage.Time;
        m_Montage.StopDuration = duration;

        return true;
    }

    double AnimationLayer::GetMontageEnd() const
    {
        const auto montage = m_Montage.Montage;
        if (!montage) return 0;

        const auto end = double(montage->GetSequence()->GetDuration());
        return m_Montage.StopTime < 0
            ? end
            : std::min(end, m_Montage.StopTime + m_Montage.StopDuration);
    }

    float AnimationLayer::GetMontageWeight(const double time) const
    {
        if (!m_Montage.Montage || time >= GetMontageEnd())
            return 0;

        const auto& montage = *m_Montage.Montage;
        const auto duration = montage.GetSequence()->GetDuration();

        const auto in = montage.GetBlendIn() > 0
            ? std::clamp(time / montage.GetBlendIn(), 0.0, 1.0)
            : 1.0;

        const auto out = montage.GetBlendOut() > 0
            ? std::clamp((duration - time) / montage.GetBlendOut(), 0.0, 1.0)
            : 1.0;

        auto weight = std::min(in, out);

        if (m_Montage.StopTime >= 0)
        {
            const auto stop = std::clamp(
                1.0 - (time - m_Montage.StopTime) / m_Montage.StopDuration,
                0.0,
                1.0
            );
            weight = std::min(out, m_Montage.StopWeight * stop);
        }

        return float(weight);
    }

    float AnimationLayer::GetAverageMontageWeight(const double start, const double end) const
    {
        if (end <= start)
            return GetMontageWeight(start);

        const auto& montage = *m_Montage.Montage;
        const double duration = montage.GetSequence()->GetDuration();

        const double in = montage.GetBlendIn();
        const double out = montage.GetBlendOut();
        const auto peak = in + out > 0 ? duration * (in / (in + out)) : 0;

        const auto denominator = m_Montage.StopDuration - m_Montage.StopWeight * out;
        const auto stopCrossing = m_Montage.StopTime >= 0 && denominator != 0
            ? (duration * m_Montage.StopDuration - m_Montage.StopWeight * out *
                (m_Montage.StopTime + m_Montage.StopDuration)) / denominator
            : start;

        std::array points = {
            start,
            end,
            in,
            duration - out,
            peak,
            duration,
            m_Montage.StopTime,
            GetMontageEnd(),
            stopCrossing
        };

        for (auto& point : points)
            point = std::clamp(point, start, end);

        std::ranges::sort(points);

        double area = 0;
        for (size_t i = 1; i < points.size(); ++i)
        {
            // Interior samples avoid treating a zero-length exit as a fade to zero.
            const auto a = points[i - 1], b = points[i];
            area += (b - a) * GetMontageWeight((a + b) * 0.5);
        }

        return float(area / (end - start));
    }

    bool AnimationLayer::BlendMontage(
        const double time,
        std::vector<SBoneTransform>& pose
    ) const
    {
        const auto montage = m_Montage.Montage;
        if (!montage) return true;

        const auto weight = GetMontageWeight(time);
        if (weight == 0 && (m_Montage.SourcePose.empty() || time >= GetMontageEnd()))
            return true;

        std::vector<SBoneTransform> sampled;
        if (!montage->GetSequence()->Sample(float(time), sampled, m_ReferencePose))
            return false;

        const auto fadeIn = montage->GetBlendIn();
        auto sourceWeight = fadeIn > 0
            ? float(std::clamp(1.0 - time / fadeIn, 0.0, 1.0))
            : 0.0f;

        const auto fadeOut = montage->GetBlendOut();
        if (fadeOut > 0)
            sourceWeight *= float(std::clamp(
                (montage->GetSequence()->GetDuration() - time) / fadeOut,
                0.0,
                1.0
            ));

        if (m_Montage.StopTime >= 0)
            sourceWeight *= float(std::clamp(
                1.0 - (time - m_Montage.StopTime) / m_Montage.StopDuration,
                0.0,
                1.0
            ));

        for (size_t i = 0; i < pose.size(); ++i)
        {
            if (!m_Montage.SourcePose.empty() && sourceWeight > 0)
                pose[i] = SBoneTransform::Blend(
                    pose[i],
                    m_Montage.SourcePose[i],
                    sourceWeight
                );

            pose[i] = SBoneTransform::Blend(pose[i], sampled[i], weight);

            if (!pose[i].IsValid())
            {
                EE_CORE_ERROR("Montage blending produced an invalid local transform.")
                return false;
            }
        }

        return true;
    }

    bool AnimationLayer::Evaluate(const float dt, SLayerEvaluation& output) const
    {
        if (!EvaluatePlayback(dt, output))
            return false;

        if (!m_Montage.Montage)
            return true;

        output.PlaybackPose = output.LocalPose;
        output.MontageTime = m_Montage.Time + (m_Montage.Playing ? dt : 0);

        return BlendMontage(output.MontageTime, output.LocalPose);
    }

    bool AnimationLayer::CollectNotifyEvents(
        const float dt,
        std::vector<SAnimationNotifyEvent>& output
    ) const
    {
        const auto montagePlayback = m_Montage;
        const auto montage = montagePlayback.Montage;

        if (!montage)
            return CollectPlaybackNotifyEvents(m_Playback, dt, output);

        if (!montagePlayback.Playing || dt == 0)
            return true;

        const auto activeTime = float(std::min(
            double(dt),
            GetMontageEnd() - montagePlayback.Time
        ));

        const SPlayback action = {
            .Sequence = montage->GetSequence(),
            .Time = montagePlayback.Time,
            .IncludeNotifyStart = montagePlayback.IncludeNotifyStart,
            .Looping = false
        };
        if (!CollectPlaybackNotifyEvents(action, activeTime, output))
            return false;

        if (dt > activeTime && m_Playing)
        {
            auto playback = m_Playback;
            playback.Time = AdvancePlayback(playback, activeTime);
            playback.IncludeNotifyStart = false;

            std::vector<SAnimationNotifyEvent> resumed;
            if (!CollectPlaybackNotifyEvents(playback, dt - activeTime, resumed))
                return false;

            if (resumed.size() > AnimationSequence::MaxNotifyEventsPerUpdate - output.size())
            {
                EE_CORE_ERROR("Montage and source exceeded the notify event capacity.")
                return false;
            }

            for (auto& event : resumed)
            {
                event.TimeOffset += activeTime;
                output.push_back(std::move(event));
            }
        }

        return true;
    }

    bool AnimationLayer::ExtractRootMotion(
        const float dt,
        const uint32_t boneIndex,
        SRootMotionDelta& output
    ) const
    {
        const auto montagePlayback = m_Montage;
        const auto montage = montagePlayback.Montage;

        if (!montage)
            return ExtractSourceRootMotion(dt, boneIndex, output);

        if (!montagePlayback.Playing || dt == 0)
        {
            output = {};
            return true;
        }

        const auto activeTime = float(std::min(
            double(dt),
            GetMontageEnd() - montagePlayback.Time
        ));
        SRootMotionDelta source, action, remaining;

        if (!ExtractSourceRootMotion(activeTime, boneIndex, source))
            return false;

        const auto& sequence = montage->GetSequence();
        const auto duration = sequence->GetDuration();

        const auto success = sequence->ExtractRootMotion(
                m_Montage.Time / duration,
                (m_Montage.Time + activeTime) / duration,
                false,
                m_ReferencePose[boneIndex],
                action,
                boneIndex
        );
        if (!success) return false;

        const auto weight = GetAverageMontageWeight(
            m_Montage.Time,
            m_Montage.Time + activeTime
        );

        const auto movement = SRootMotionDelta::Blend(source, action, weight);
        if (!ExtractSourceRootMotion(dt - activeTime, boneIndex, remaining, activeTime))
            return false;

        const auto result = movement * remaining;
        if (!result.IsValid())
        {
            EE_CORE_ERROR("Montage produced an invalid root-motion delta.")
            return false;
        }

        output = result;
        return true;
    }
}
