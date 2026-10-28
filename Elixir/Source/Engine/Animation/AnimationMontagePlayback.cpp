#include "epch.h"
#include "AnimationMontagePlayback.h"

#include <Engine/Animation/Pose.h>

namespace Elixir
{
    SAnimationPlaybackInterval AnimationMontagePlayback::GetPlaybackInterval(
        const SAnimationMontageInterval& interval
    ) const
    {
        if (!IsActive()) return {};

        const auto& sequence = m_State.Montage->GetSequence();
        const auto duration = sequence->GetDuration();

        return {
            .Sequence = sequence,
            .StartPhase = interval.Start.Time / duration,
            .EndPhase = (interval.Start.Time + interval.Duration) / duration,
            .DurationSeconds = duration,
            .FrameOffset = interval.TimeOffset,
            .Looping = false,
            .IncludeStart = interval.Start.IncludeNotifyStart
        };
    }

    bool AnimationMontagePlayback::Play(
        Ref<const AnimationMontage> montage,
        const std::string_view section,
        const SLocalPose& visible,
        const SLocalPose& continuing,
        const SLocalPose& referencePose,
        SLocalPose& output
    )
    {
        if (!montage || (IsActive() && !m_State.Montage->IsInterruptible()))
            return false;

        const auto selected = section.empty()
            ? std::optional<size_t>(0)
            : montage->FindSection(section);

        if (!selected) return false;

        AnimationMontagePlayback candidate;
        candidate.m_State.Montage = std::move(montage);

        auto& state = candidate.m_State;
        state.Cursor.Section = *selected;
        state.Cursor.Time = state.Montage->GetSections()[*selected].StartTime;
        state.Playing = true;

        for (size_t i = 0; i < state.Montage->GetSections().size(); ++i)
            state.NextSections.push_back(state.Montage->GetNextSection(i));

        if (IsActive() && state.Montage->GetBlendIn() > 0)
            state.SourcePose = visible;

        auto pose = continuing;

        if (!candidate.Sample(state.Cursor, pose, referencePose))
            return false;

        *this = std::move(candidate);
        output = std::move(pose);

        return true;
    }

    bool AnimationMontagePlayback::SetNextSection(
        const std::string_view from,
        const std::string_view to
    )
    {
        if (!IsActive()) return false;

        const auto source = m_State.Montage->FindSection(from);
        const auto target = to.empty()
            ? std::nullopt
            : m_State.Montage->FindSection(to);

        if (!source || (!to.empty() && !target))
            return false;

        const auto remaining = GetEnd(m_State.Cursor) - m_State.Cursor.Elapsed;

        if (IsStopping() || remaining <= m_State.Montage->GetBlendOut())
            return false;

        const auto previous = m_State.NextSections[*source];
        const auto weight = GetWeight(m_State.Cursor);

        m_State.NextSections[*source] = target;

        if (GetWeight(m_State.Cursor) < weight)
        {
            m_State.NextSections[*source] = previous;
            return false;
        }

        return true;
    }

    bool AnimationMontagePlayback::Jump(const std::string_view name,
        const SLocalPose& continuing,
        const SLocalPose& referencePose,
        SLocalPose& output)
    {
        if (!IsActive() || IsStopping()) return false;

        const auto section = m_State.Montage->FindSection(name);
        if (!section) return false;

        auto cursor = m_State.Cursor;
        cursor.Section = *section;
        cursor.Time = m_State.Montage->GetSections()[*section].StartTime;
        cursor.IncludeNotifyStart = true;

        auto pose = continuing;
        if (!Sample(cursor, pose, referencePose))
            return false;

        m_State.Cursor = cursor;

        output = std::move(pose);
        return true;
    }

    bool AnimationMontagePlayback::Stop(const std::optional<float> blendOut)
    {
        if (blendOut && (!std::isfinite(*blendOut) || *blendOut < 0))
            return false;

        if (!IsActive()) return true;

        if (!m_State.Montage->IsInterruptible())
            return false;

        const auto duration = blendOut.value_or(m_State.Montage->GetBlendOut());
        if (duration == 0)
        {
            m_State = {};
            return true;
        }

        if (IsStopping()) return true;

        m_State.StopWeight = GetWeight(m_State.Cursor);
        m_State.StopTime = m_State.Cursor.Elapsed;
        m_State.StopDuration = duration;

        return true;
    }

    void AnimationMontagePlayback::Apply(const SAnimationMontageCursor& cursor)
    {
        m_State.Cursor = cursor;

        if (cursor.Finished)
            m_State = {};
    }

    std::string_view AnimationMontagePlayback::GetCurrentSection() const
    {
        return IsActive()
            ? m_State.Montage->GetSections()[m_State.Cursor.Section].Name
            : std::string_view{};
    }

    double AnimationMontagePlayback::GetEnd(const SAnimationMontageCursor& cursor) const
    {
        if (!m_State.Montage || cursor.Finished)
            return cursor.Elapsed;

        auto end = cursor.Elapsed;
        auto time = cursor.Time;
        auto section = cursor.Section;
        const auto& sections = m_State.Montage->GetSections();

        for (size_t visited = 0; visited <= sections.size(); ++visited)
        {
            if (visited == sections.size())
            {
                end = std::numeric_limits<double>::infinity();
                break;
            }

            end += double(m_State.Montage->GetSectionEnd(section)) - time;

            const auto next = m_State.NextSections[section];
            if (!next) break;

            section = *next;
            time = sections[section].StartTime;
        }

        return m_State.StopTime < 0
            ? end
            : std::min(end, m_State.StopTime + m_State.StopDuration);
    }

    bool AnimationMontagePlayback::Evaluate(
        const float dt,
        SAnimationMontageEvaluation& output
    ) const
    {
        output = {};
        output.Cursor = m_State.Cursor;

        if (!m_State.Playing || dt == 0)
            return true;

        auto& cursor = output.Cursor;
        const auto& montage = *m_State.Montage;
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
            const auto stopRemaining = m_State.StopTime >= 0
                ? std::max(0.0, m_State.StopTime + m_State.StopDuration - cursor.Elapsed)
                : remaining;

            const auto interval = std::min({
                remaining,
                sectionEnd - cursor.Time,
                stopRemaining
            });

            if (interval > 0)
                output.Intervals.push_back({
                    cursor,
                    interval,
                    output.ActiveDuration
                });

            cursor.Time += interval;
            cursor.Elapsed += interval;
            cursor.IncludeNotifyStart = false;
            output.ActiveDuration += interval;
            remaining -= interval;

            if (m_State.StopTime >= 0 && stopRemaining <= interval)
            {
                cursor.Finished = true;
                break;
            }

            if (cursor.Time >= sectionEnd)
            {
                const auto next = m_State.NextSections[cursor.Section];
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

    float AnimationMontagePlayback::GetWeight(const SAnimationMontageCursor& cursor) const
    {
        if (!m_State.Montage || cursor.Finished)
            return 0;

        const auto end = GetEnd(cursor);
        if (cursor.Elapsed >= end)
            return 0;

        const auto& montage = *m_State.Montage;
        const auto in = montage.GetBlendIn() > 0
            ? std::clamp(cursor.Elapsed / montage.GetBlendIn(), 0.0, 1.0)
            : 1.0;
        const auto out = montage.GetBlendOut() > 0
            ? std::clamp((end - cursor.Elapsed) / montage.GetBlendOut(), 0.0, 1.0)
            : 1.0;
        auto weight = std::min(in, out);

        if (m_State.StopTime >= 0)
        {
            const auto stop = std::clamp(
                1.0 - (cursor.Elapsed - m_State.StopTime) / m_State.StopDuration,
                0.0,
                1.0
            );
            weight = std::min(out, m_State.StopWeight * stop);
        }

        return float(weight);
    }

    float AnimationMontagePlayback::GetAverageWeight(const SAnimationMontageInterval& interval) const
    {
        const auto& start = interval.Start;
        const auto end = start.Elapsed + interval.Duration;

        if (interval.Duration <= 0)
            return GetWeight(start);

        const auto& montage = *m_State.Montage;
        const auto duration = GetEnd(start);

        const double in = montage.GetBlendIn();
        const double out = montage.GetBlendOut();

        const auto peak = std::isfinite(duration) && in + out > 0
            ? duration * (in / (in + out))
            : start.Elapsed;

        const auto denominator = m_State.StopDuration - m_State.StopWeight * out;
        const auto stopCrossing = m_State.StopTime >= 0 && denominator != 0
            ? (duration * m_State.StopDuration - m_State.StopWeight * out *
                (m_State.StopTime + m_State.StopDuration)) / denominator
            : start.Elapsed;

        std::array points = {
            start.Elapsed,
            end,
            in,
            duration - out,
            peak,
            duration,
            m_State.StopTime,
            duration,
            stopCrossing
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
            area += (points[i] - points[i - 1]) * GetWeight(middle);
        }

        return float(area / interval.Duration);
    }

    bool AnimationMontagePlayback::Sample(
        const SAnimationMontageCursor& cursor,
        SLocalPose& pose,
        const SLocalPose& referencePose
    ) const
    {
        const auto montage = m_State.Montage;
        if (!montage) return true;

        const auto weight = GetWeight(cursor);
        if (weight == 0 && (m_State.SourcePose.LocalTransforms.empty() || cursor.Finished))
            return true;

        SLocalPose sampled;
        if (!montage->GetSequence()->Sample(float(cursor.Time), sampled, referencePose))
            return false;

        const auto fadeIn = montage->GetBlendIn();
        auto sourceWeight = fadeIn > 0
            ? float(std::clamp(1.0 - cursor.Elapsed / fadeIn, 0.0, 1.0))
            : 0.0f;

        const auto fadeOut = montage->GetBlendOut();
        if (fadeOut > 0)
            sourceWeight *= float(std::clamp(
                (GetEnd(cursor) - cursor.Elapsed) / fadeOut,
                0.0,
                1.0
            ));

        if (m_State.StopTime >= 0)
            sourceWeight *= float(std::clamp(
                1.0 - (cursor.Elapsed - m_State.StopTime) / m_State.StopDuration,
                0.0,
                1.0
            ));

        for (size_t i = 0; i < pose.LocalTransforms.size(); ++i)
        {
            if (!m_State.SourcePose.LocalTransforms.empty() && sourceWeight > 0)
            {
                pose.LocalTransforms[i] = SBoneTransform::Blend(
                    pose.LocalTransforms[i],
                    m_State.SourcePose.LocalTransforms[i],
                    sourceWeight
                );
            }

            pose.LocalTransforms[i] = SBoneTransform::Blend(
                pose.LocalTransforms[i],
                sampled.LocalTransforms[i],
                weight
            );

            if (!pose.LocalTransforms[i].IsValid())
            {
                EE_CORE_ERROR("Montage blending produced an invalid local transform.")
                return false;
            }
        }

        if (!m_State.SourcePose.LocalTransforms.empty() && sourceWeight > 0 &&
            !pose.Curves.Blend(m_State.SourcePose.Curves, sourceWeight))
            return false;
        return pose.Curves.Blend(sampled.Curves, weight);
    }

}
