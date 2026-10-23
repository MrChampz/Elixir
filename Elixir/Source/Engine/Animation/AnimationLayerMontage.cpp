#include "epch.h"
#include "AnimationLayer.h"

namespace Elixir
{
    bool AnimationLayer::PlayMontage(
        Ref<const AnimationMontage> montage,
        const std::string_view startSection
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

        const auto section = startSection.empty() ? std::optional<size_t>(0)
            : montage->FindSection(startSection);
        if (!section)
        {
            EE_CORE_ERROR("Montage starting section '{}' does not exist.", startSection)
            return false;
        }

        SMontagePlayback playback;
        playback.Montage = std::move(montage);
        playback.Cursor.Section = *section;
        playback.Cursor.Time = playback.Montage->GetSections()[*section].StartTime;
        playback.Playing = true;
        for (size_t i = 0; i < playback.Montage->GetSections().size(); ++i)
            playback.NextSections.push_back(playback.Montage->GetNextSection(i));
        if (m_Montage.Montage && playback.Montage->GetBlendIn() > 0)
            playback.SourcePose = GetLocalPose();

        std::vector<SBoneTransform> sampled;
        if (!playback.Montage->GetSequence()->Sample(
                float(playback.Cursor.Time), sampled, m_ReferencePose))
            return false;

        auto candidate = *this;
        candidate.m_Montage = std::move(playback);

        auto pose = m_LocalPose;
        if (!candidate.BlendMontage(candidate.m_Montage.Cursor, pose))
            return false;

        m_Montage = std::move(candidate.m_Montage);
        m_MontagePose = std::move(pose);
        m_NotifyEvents.clear();

        return true;
    }

    bool AnimationLayer::SetNextMontageSection(
        const std::string_view from, const std::string_view to
    )
    {
        if (!m_Montage.Montage)
        {
            EE_CORE_ERROR("Section continuation requires an active montage.")
            return false;
        }
        const auto source = m_Montage.Montage->FindSection(from);
        const auto target = to.empty() ? std::nullopt : m_Montage.Montage->FindSection(to);
        if (!source || (!to.empty() && !target))
        {
            EE_CORE_ERROR("Montage section continuation requires existing section names.")
            return false;
        }
        const auto& cursor = m_Montage.Cursor;
        const auto remaining = GetMontageEnd(cursor) - cursor.Elapsed;
        if (m_Montage.StopTime >= 0 || remaining <= m_Montage.Montage->GetBlendOut())
        {
            EE_CORE_ERROR("Montage section continuation cannot change during the exit fade.")
            return false;
        }
        const auto previous = m_Montage.NextSections[*source];
        const auto previousWeight = GetMontageWeight();
        m_Montage.NextSections[*source] = target;
        if (GetMontageWeight() < previousWeight)
        {
            m_Montage.NextSections[*source] = previous;
            EE_CORE_ERROR("Montage continuation was requested too late for a smooth exit.")
            return false;
        }
        return true;
    }

    bool AnimationLayer::JumpToMontageSection(const std::string_view name)
    {
        if (!m_Montage.Montage || m_Montage.StopTime >= 0)
        {
            EE_CORE_ERROR("Section jumps require an active montage without an explicit exit.")
            return false;
        }
        const auto section = m_Montage.Montage->FindSection(name);
        if (!section)
        {
            EE_CORE_ERROR("Montage section '{}' does not exist.", name)
            return false;
        }
        auto cursor = m_Montage.Cursor;
        cursor.Section = *section;
        cursor.Time = m_Montage.Montage->GetSections()[*section].StartTime;
        cursor.IncludeNotifyStart = true;
        auto pose = m_LocalPose;
        if (!BlendMontage(cursor, pose))
            return false;
        m_Montage.Cursor = cursor;
        m_MontagePose = std::move(pose);
        m_NotifyEvents.clear();
        return true;
    }

    std::string_view AnimationLayer::GetCurrentMontageSection() const
    {
        return m_Montage.Montage
            ? m_Montage.Montage->GetSections()[m_Montage.Cursor.Section].Name : std::string_view{};
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
        m_Montage.StopTime = m_Montage.Cursor.Elapsed;
        m_Montage.StopDuration = duration;

        return true;
    }

    double AnimationLayer::GetMontageEnd(const SMontageCursor& cursor) const
    {
        if (!m_Montage.Montage || cursor.Finished)
            return cursor.Elapsed;
        auto end = cursor.Elapsed;
        auto section = cursor.Section;
        auto time = cursor.Time;
        const auto& sections = m_Montage.Montage->GetSections();
        for (size_t visited = 0; visited <= sections.size(); ++visited)
        {
            if (visited == sections.size())
            {
                end = std::numeric_limits<double>::infinity();
                break;
            }
            end += double(m_Montage.Montage->GetSectionEnd(section)) - time;
            const auto next = m_Montage.NextSections[section];
            if (!next)
                break;
            section = *next;
            time = sections[section].StartTime;
        }
        return m_Montage.StopTime < 0
            ? end : std::min(end, m_Montage.StopTime + m_Montage.StopDuration);
    }

    bool AnimationLayer::EvaluateMontage(
        const float dt, SMontageEvaluation& output
    ) const
    {
        output = {};
        output.Cursor = m_Montage.Cursor;
        if (!m_Montage.Playing || dt == 0)
            return true;

        auto& cursor = output.Cursor;
        const auto& montage = *m_Montage.Montage;
        const auto& sections = montage.GetSections();
        double remaining = dt;
        while (remaining > 0)
        {
            if (output.Intervals.size() >= 4096)
            {
                EE_CORE_ERROR("Montage update exceeded 4096 section intervals.")
                return false;
            }
            const double sectionEnd = montage.GetSectionEnd(cursor.Section);
            const auto stopRemaining = m_Montage.StopTime >= 0
                ? std::max(0.0, m_Montage.StopTime + m_Montage.StopDuration - cursor.Elapsed)
                : remaining;
            const auto interval = std::min({remaining, sectionEnd - cursor.Time, stopRemaining});
            if (interval > 0)
                output.Intervals.push_back({cursor, interval, output.ActiveDuration});
            cursor.Time += interval;
            cursor.Elapsed += interval;
            cursor.IncludeNotifyStart = false;
            output.ActiveDuration += interval;
            remaining -= interval;

            if (m_Montage.StopTime >= 0 && stopRemaining <= interval)
            {
                cursor.Finished = true;
                break;
            }
            if (cursor.Time >= sectionEnd)
            {
                const auto next = m_Montage.NextSections[cursor.Section];
                if (!next)
                {
                    cursor.Finished = true;
                    break;
                }
                cursor.Section = *next;
                cursor.Time = sections[*next].StartTime;
                // Adjacent sections share one timeline event at their common boundary.
                cursor.IncludeNotifyStart = cursor.Time != sectionEnd;
            }
        }
        return true;
    }

    float AnimationLayer::GetMontageWeight(const SMontageCursor& cursor) const
    {
        if (!m_Montage.Montage || cursor.Finished)
            return 0;
        const auto end = GetMontageEnd(cursor);
        if (cursor.Elapsed >= end)
            return 0;

        const auto& montage = *m_Montage.Montage;
        const auto in = montage.GetBlendIn() > 0
            ? std::clamp(cursor.Elapsed / montage.GetBlendIn(), 0.0, 1.0) : 1.0;
        const auto out = montage.GetBlendOut() > 0
            ? std::clamp((end - cursor.Elapsed) / montage.GetBlendOut(), 0.0, 1.0) : 1.0;
        auto weight = std::min(in, out);
        if (m_Montage.StopTime >= 0)
        {
            const auto stop = std::clamp(
                1.0 - (cursor.Elapsed - m_Montage.StopTime) / m_Montage.StopDuration, 0.0, 1.0
            );
            weight = std::min(out, m_Montage.StopWeight * stop);
        }
        return float(weight);
    }

    float AnimationLayer::GetAverageMontageWeight(const SMontageInterval& interval) const
    {
        const auto& start = interval.Start;
        const auto end = start.Elapsed + interval.Duration;
        if (interval.Duration <= 0)
            return GetMontageWeight(start);

        const auto& montage = *m_Montage.Montage;
        const auto duration = GetMontageEnd(start);
        const double in = montage.GetBlendIn();
        const double out = montage.GetBlendOut();
        const auto peak = std::isfinite(duration) && in + out > 0
            ? duration * (in / (in + out)) : start.Elapsed;
        const auto denominator = m_Montage.StopDuration - m_Montage.StopWeight * out;
        const auto stopCrossing = m_Montage.StopTime >= 0 && denominator != 0
            ? (duration * m_Montage.StopDuration - m_Montage.StopWeight * out *
                (m_Montage.StopTime + m_Montage.StopDuration)) / denominator : start.Elapsed;
        std::array<double, 9> points = {
            start.Elapsed, end, in, duration - out, peak, duration,
            m_Montage.StopTime, duration, stopCrossing
        };
        for (auto& point : points)
            point = std::clamp(point, start.Elapsed, end);
        std::ranges::sort(points);

        double area = 0;
        for (size_t i = 1; i < points.size(); ++i)
        {
            auto middle = start;
            middle.Elapsed = (points[i - 1] + points[i]) * 0.5;
            middle.Time += middle.Elapsed - start.Elapsed;
            area += (points[i] - points[i - 1]) * GetMontageWeight(middle);
        }
        return float(area / interval.Duration);
    }

    bool AnimationLayer::BlendMontage(
        const SMontageCursor& cursor,
        std::vector<SBoneTransform>& pose
    ) const
    {
        const auto montage = m_Montage.Montage;
        if (!montage) return true;

        const auto weight = GetMontageWeight(cursor);
        if (weight == 0 && (m_Montage.SourcePose.empty() || cursor.Finished))
            return true;

        std::vector<SBoneTransform> sampled;
        if (!montage->GetSequence()->Sample(float(cursor.Time), sampled, m_ReferencePose))
            return false;

        const auto fadeIn = montage->GetBlendIn();
        auto sourceWeight = fadeIn > 0
            ? float(std::clamp(1.0 - cursor.Elapsed / fadeIn, 0.0, 1.0))
            : 0.0f;

        const auto fadeOut = montage->GetBlendOut();
        if (fadeOut > 0)
            sourceWeight *= float(std::clamp(
                (GetMontageEnd(cursor) - cursor.Elapsed) / fadeOut,
                0.0,
                1.0
            ));

        if (m_Montage.StopTime >= 0)
            sourceWeight *= float(std::clamp(
                1.0 - (cursor.Elapsed - m_Montage.StopTime) / m_Montage.StopDuration,
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
        if (!EvaluateMontage(dt, output.Montage))
            return false;

        const auto emitNotifies = m_Weight > 0 && m_Mask->HasInfluence();
        if (emitNotifies && !CollectMontageNotifyEvents(dt, output.Montage, output.NotifyEvents))
            return false;
        output.PlaybackPose = output.LocalPose;
        return BlendMontage(output.Montage.Cursor, output.LocalPose);
    }

    bool AnimationLayer::CollectMontageNotifyEvents(
        const float dt, const SMontageEvaluation& evaluation,
        std::vector<SAnimationNotifyEvent>& output
    ) const
    {
        const auto& sequence = m_Montage.Montage->GetSequence();
        const auto duration = sequence->GetDuration();
        for (const auto& interval : evaluation.Intervals)
        {
            std::vector<SAnimationNotifyOccurrence> occurrences;
            if (!sequence->CollectNotifyOccurrences(
                    interval.Start.Time / duration,
                    (interval.Start.Time + interval.Duration) / duration, false,
                    occurrences, nullptr, interval.Start.IncludeNotifyStart))
                return false;
            if (occurrences.size() > AnimationSequence::MaxNotifyEventsPerUpdate - output.size())
            {
                EE_CORE_ERROR("Montage sections exceeded the notify event capacity.")
                return false;
            }
            for (const auto& occurrence : occurrences)
                output.push_back({sequence, occurrence.NotifyIndex,
                    interval.TimeOffset + occurrence.PhaseOffset * duration, m_Name});
        }

        if (evaluation.Cursor.Finished && dt > evaluation.ActiveDuration && m_Playing)
        {
            auto playback = m_Playback;
            playback.Time = AdvancePlayback(playback, float(evaluation.ActiveDuration));
            playback.IncludeNotifyStart = false;
            std::vector<SAnimationNotifyEvent> resumed;
            if (!CollectPlaybackNotifyEvents(
                    playback, float(dt - evaluation.ActiveDuration), resumed))
                return false;
            if (resumed.size() > AnimationSequence::MaxNotifyEventsPerUpdate - output.size())
            {
                EE_CORE_ERROR("Montage and source exceeded the notify event capacity.")
                return false;
            }
            for (auto& event : resumed)
            {
                event.TimeOffset += evaluation.ActiveDuration;
                output.push_back(std::move(event));
            }
        }
        return true;
    }

    bool AnimationLayer::ExtractRootMotion(
        const float dt, const uint32_t boneIndex, SRootMotionDelta& output,
        const SLayerEvaluation& evaluation
    ) const
    {
        if (!m_Montage.Montage)
            return ExtractSourceRootMotion(dt, boneIndex, output);
        if (!m_Montage.Playing || dt == 0)
        {
            output = {};
            return true;
        }

        const auto& sequence = m_Montage.Montage->GetSequence();
        const auto duration = sequence->GetDuration();
        SRootMotionDelta result;
        for (const auto& interval : evaluation.Montage.Intervals)
        {
            SRootMotionDelta source, action;
            if (!ExtractSourceRootMotion(
                    float(interval.Duration), boneIndex, source, float(interval.TimeOffset)))
                return false;
            if (!sequence->ExtractRootMotion(
                    interval.Start.Time / duration,
                    (interval.Start.Time + interval.Duration) / duration,
                    false, m_ReferencePose[boneIndex], action, boneIndex))
                return false;
            result = result * SRootMotionDelta::Blend(
                source, action, GetAverageMontageWeight(interval)
            );
        }
        SRootMotionDelta remaining;
        const auto activeTime = float(evaluation.Montage.ActiveDuration);
        if (!ExtractSourceRootMotion(dt - activeTime, boneIndex, remaining, activeTime))
            return false;
        result = result * remaining;
        if (!result.IsValid())
        {
            EE_CORE_ERROR("Montage produced an invalid root-motion delta.")
            return false;
        }
        output = result;
        return true;
    }
}
