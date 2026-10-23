#include "epch.h"
#include "AnimationSequence.h"

namespace Elixir
{
    namespace
    {
        template<typename TKeys, typename TValidate>
        bool ValidateKeys(const TKeys& keys, const float duration, TValidate validate)
        {
            float previous = -1.0f;
            for (const auto& key : keys)
            {
                if (!std::isfinite(key.Time) ||
                    key.Time < 0 ||
                    key.Time > duration ||
                    key.Time <= previous ||
                    !validate(key.Value))
                    return false;

                previous = key.Time;
            }

            return true;
        }

        template<typename TKeys, typename TValue, typename TBlend>
        TValue SampleKeys(
            const TKeys& keys,
            const float time,
            const EBoneInterpolation mode,
            const TValue& fallback,
            TBlend blend
        )
        {
            if (keys.empty()) return fallback;
            if (time <= keys.front().Time) return keys.front().Value;
            if (time >= keys.back().Time) return keys.back().Value;

            const auto next = std::upper_bound(
                keys.begin(),
                keys.end(),
                time,
                [](const float t, const auto& key) { return t < key.Time; }
            );

            const auto& previous = *std::prev(next);

            if (mode == EBoneInterpolation::Step)
                return previous.Value;

            return blend(
                previous.Value,
                next->Value,
                (time - previous.Time) / (next->Time - previous.Time)
            );
        }
    }

    Ref<AnimationSequence> AnimationSequence::Create(
        std::string name,
        Ref<const Skeleton> skeleton,
        const float duration,
        std::vector<SBoneAnimationTrack> tracks,
        std::vector<SAnimationSyncMarker> syncMarkers,
        std::vector<SAnimationNotify> notifies
    )
    {
        if (!skeleton || !std::isfinite(duration) || duration < 0)
        {
            EE_CORE_ERROR("AnimationSequence requires a skeleton and a finite nonnegative duration.")
            return nullptr;
        }

        for (const auto& marker : syncMarkers)
        {
            if (!std::isfinite(marker.Time))
            {
                EE_CORE_ERROR("AnimationSequence sync marker times must be finite.")
                return nullptr;
            }
        }

        std::ranges::sort(syncMarkers, {}, &SAnimationSyncMarker::Time);
        std::unordered_set<std::string> markerNames;
        float previousMarkerTime = -1.0f;
        for (const auto& marker : syncMarkers)
        {
            if (marker.Name.empty() || !markerNames.insert(marker.Name).second ||
                !std::isfinite(marker.Time) || marker.Time < 0.0f ||
                marker.Time >= duration || marker.Time <= previousMarkerTime)
            {
                EE_CORE_ERROR("AnimationSequence contains invalid or duplicate sync markers.")
                return nullptr;
            }
            previousMarkerTime = marker.Time;
        }

        for (const auto& notify : notifies)
        {
            if (notify.Name.empty() ||
                !std::isfinite(notify.Time) ||
                notify.Time < 0.0f ||
                notify.Time > duration ||
                duration == 0.0f)
            {
                EE_CORE_ERROR("AnimationSequence contains an invalid notify name or time.")
                return nullptr;
            }
        }

        std::ranges::stable_sort(notifies, [](const auto& first, const auto& second)
        {
            return first.Time < second.Time;
        });

        std::unordered_set<uint32_t> indices;

        for (const auto& track : tracks)
        {
            const auto vectorValid = [](const glm::vec3& v)
            {
                return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
            };

            const auto rotationValid = [](const glm::quat& q)
            {
                return std::isfinite(q.x) &&
                    std::isfinite(q.y) &&
                    std::isfinite(q.z) &&
                    std::isfinite(q.w) &&
                    std::abs(glm::dot(q, q) - 1.0f) < 1e-4f;
            };

            const auto scaleValid = [&](const glm::vec3& v)
            {
                return vectorValid(v) &&
                    std::abs(v.x) >= 1e-8f &&
                    std::abs(v.y) >= 1e-8f &&
                    std::abs(v.z) >= 1e-8f;
            };

            const auto modeValid = [](const auto mode)
            {
                return mode == EBoneInterpolation::Step ||
                    mode == EBoneInterpolation::Linear;
            };

            if (track.BoneIndex >= skeleton->GetBones().size() ||
                !indices.insert(track.BoneIndex).second ||
                !ValidateKeys(track.Translations, duration, vectorValid) ||
                !ValidateKeys(track.Rotations, duration, rotationValid) ||
                !ValidateKeys(track.Scales, duration, scaleValid) ||
                !modeValid(track.TranslationInterpolation) ||
                !modeValid(track.RotationInterpolation) ||
                !modeValid(track.ScaleInterpolation))
            {
                EE_CORE_ERROR("AnimationSequence contains invalid or duplicate bone tracks.")
                return nullptr;
            }
        }

        return Ref<AnimationSequence>(new AnimationSequence(
            std::move(name),
            std::move(skeleton),
            duration,
            std::move(tracks),
            std::move(syncMarkers),
            std::move(notifies)
        ));
    }

    AnimationSequence::AnimationSequence(
        std::string name,
        Ref<const Skeleton> skeleton,
        const float duration,
        std::vector<SBoneAnimationTrack> tracks,
        std::vector<SAnimationSyncMarker> syncMarkers,
        std::vector<SAnimationNotify> notifies
    ) : m_Name(std::move(name)),
        m_Skeleton(std::move(skeleton)),
        m_Duration(duration),
        m_Tracks(std::move(tracks)),
        m_SyncMarkers(std::move(syncMarkers)),
        m_Notifies(std::move(notifies)) {}

    bool AnimationSequence::Sample(
        const float time,
        std::vector<SBoneTransform>& output,
        const std::span<const SBoneTransform> referencePose
    ) const
    {
        if (!std::isfinite(time))
        {
            EE_CORE_ERROR("AnimationSequence sample time must be finite.")
            return false;
        }

        const auto invalidTransform = std::ranges::any_of(
            referencePose,
            [](const auto& t) { return !t.IsValid(); }
        );

        if (!referencePose.empty() &&
            (referencePose.size() != m_Skeleton->GetBones().size() || invalidTransform))
        {
            EE_CORE_ERROR("AnimationSequence requires valid reference transforms for every bone.")
            return false;
        }

        std::vector pose(referencePose.begin(), referencePose.end());
        if (pose.empty())
        {
            for (const auto& bone : m_Skeleton->GetBones())
                pose.push_back(bone.ReferenceTransform);
        }

        const float t = std::clamp(time, 0.0f, m_Duration);

        for (const auto& track : m_Tracks)
        {
            auto& transform = pose[track.BoneIndex];

            const auto blendVector = [](const auto& a, const auto& b, const float alpha)
            {
                return glm::mix(a, b, alpha);
            };

            const auto blendQuaternion = [](const auto& a, const auto& b, const float alpha)
            {
                return glm::normalize(glm::slerp(a, b, alpha));
            };

            transform.Translation = SampleKeys(
                track.Translations,
                t,
                track.TranslationInterpolation,
                transform.Translation,
                blendVector
            );

            transform.Scale = SampleKeys(
                track.Scales,
                t,
                track.ScaleInterpolation,
                transform.Scale,
                blendVector
            );

            transform.Rotation = SampleKeys(
                track.Rotations,
                t,
                track.RotationInterpolation,
                transform.Rotation,
                blendQuaternion
            );

            if (!transform.IsValid())
            {
                EE_CORE_ERROR("AnimationSequence produced a singular or invalid transform.")
                return false;
            }
        }

        output = std::move(pose);

        return true;
    }

    bool AnimationSequence::ExtractRootMotion(
        double startPhase,
        double endPhase,
        const bool looping,
        const SBoneTransform& reference,
        SRootMotionDelta& output,
        const uint32_t boneIndex
    ) const
    {
        if (!std::isfinite(startPhase) ||
            !std::isfinite(endPhase) ||
            endPhase < startPhase ||
            !reference.IsValid() ||
            boneIndex >= m_Skeleton->GetBones().size())
        {
            EE_CORE_ERROR("Root motion requires valid reference transforms and forward phases.")
            return false;
        }

        if (!looping)
        {
            startPhase = std::clamp(startPhase, 0.0, 1.0);
            endPhase = std::clamp(endPhase, 0.0, 1.0);
        }

        if (endPhase - startPhase > 4096 ||
            std::abs(startPhase) > 0x1p52 ||
            std::abs(endPhase) > 0x1p52)
        {
            EE_CORE_ERROR("Root motion exceeded the supported cycle interval.")
            return false;
        }

        if (startPhase == endPhase || m_Duration == 0)
        {
            output = {};
            return true;
        }

        const auto track = std::ranges::find(
            m_Tracks,
            boneIndex,
            &SBoneAnimationTrack::BoneIndex
        );

        if (track == m_Tracks.end())
        {
            output = {};
            return true;
        }

        const auto sample = [&](const float time) -> SRootMotionDelta
        {
            return {
                SampleKeys(
                    track->Translations,
                    time, track->TranslationInterpolation,
                    reference.Translation,
                    [](const auto& a, const auto& b, const float w)
                    {
                        return glm::mix(a, b, w);
                    }
                ),
                SampleKeys(
                    track->Rotations,
                    time,
                    track->RotationInterpolation,
                    reference.Rotation,
                    [](const auto& a, const auto& b, const float w)
                    {
                        return glm::normalize(glm::slerp(a, b, w));
                    }
                )
            };
        };

        const auto origin = sample(0);
        const auto cycleDelta = origin.Inverse() * sample(m_Duration);

        const auto trajectory = [&](const double phase)
        {
            const auto cycles = int64_t(std::floor(phase));
            auto step = cycles < 0 ? cycleDelta.Inverse() : cycleDelta;
            auto count = uint64_t(cycles < 0 ? -cycles : cycles);

            SRootMotionDelta result;

            while (count > 0)
            {
                if (count & 1) result = result * step;
                count >>= 1;

                if (count) step = step * step;
            }

            return result * origin.Inverse() *
                sample(float((phase - std::floor(phase)) * m_Duration));
        };

        const SRootMotionDelta root{ reference.Translation, reference.Rotation };
        const auto delta = root * trajectory(startPhase).Inverse() *
            trajectory(endPhase) * root.Inverse();

        if (!delta.IsValid())
        {
            EE_CORE_ERROR("Root motion produced a nonfinite movement delta.")
            return false;
        }

        output = delta;
        return true;
    }

    double AnimationSequence::GetSynchronizedPhase(
        const double phase,
        const AnimationSequence& reference
    ) const
    {
        if (!std::isfinite(phase))
        {
            EE_CORE_ERROR("Animation synchronization phase must be finite.")
            return 0;
        }

        if (this == &reference || !CanSynchronizeWith(reference))
            return phase;

        const auto cycle = std::floor(phase);
        const auto time = (phase - cycle) * reference.m_Duration;
        const auto& markers = reference.m_SyncMarkers;
        const auto count = markers.size();

        const auto upper = std::ranges::upper_bound(
            markers,
            time,
            {},
            &SAnimationSyncMarker::Time
        );

        const auto next = upper == markers.end()
            ? size_t(0)
            : size_t(upper - markers.begin());
        const auto previous = (next + count - 1) % count;

        auto start = double(markers[previous].Time);
        auto end = double(markers[next].Time);

        if (upper == markers.begin())
            start -= reference.m_Duration;
        else if (upper == markers.end())
            end += reference.m_Duration;

        const auto first = std::ranges::find(
            m_SyncMarkers,
            markers.front().Name,
            &SAnimationSyncMarker::Name
        );

        const auto offset = size_t(first - m_SyncMarkers.begin());
        auto targetStart = double(m_SyncMarkers[(offset + previous) % count].Time) +
            double((offset + previous) / count) * m_Duration;
        auto targetEnd = double(m_SyncMarkers[(offset + previous + 1) % count].Time) +
            double((offset + previous + 1) / count) * m_Duration;

        if (upper == markers.begin())
        {
            targetStart -= m_Duration;
            targetEnd -= m_Duration;
        }

        const auto weight = std::clamp((time - start) / (end - start), 0.0, 1.0);

        return cycle + (targetStart + (targetEnd - targetStart) * weight) / m_Duration;
    }

    Ref<AnimationSequence> AnimationSequence::WithSyncMarkers(
        std::vector<SAnimationSyncMarker> syncMarkers
    ) const
    {
        return Create(
            m_Name,
            m_Skeleton,
            m_Duration,
            m_Tracks,
            std::move(syncMarkers),
            m_Notifies
        );
    }

    bool AnimationSequence::CanSynchronizeWith(const AnimationSequence& reference) const
    {
        const auto count = m_SyncMarkers.size();
        if (count == 0 || count != reference.m_SyncMarkers.size())
            return false;

        const auto first = std::ranges::find(
            m_SyncMarkers,
            reference.m_SyncMarkers.front().Name,
            &SAnimationSyncMarker::Name
        );

        if (first == m_SyncMarkers.end())
            return false;

        const auto offset = size_t(first - m_SyncMarkers.begin());
        for (size_t i = 0; i < count; ++i)
        {
            if (m_SyncMarkers[(offset + i) % count].Name != reference.m_SyncMarkers[i].Name)
                return false;
        }

        return true;
    }

    float AnimationSequence::GetSynchronizedTime(
        const float phase,
        const AnimationSequence& reference
    ) const
    {
        if (!std::isfinite(phase))
        {
            EE_CORE_ERROR("Animation synchronization phase must be finite.")
            return 0.0f;
        }

        const auto normalized = std::clamp(phase, 0.0f, 1.0f);

        if (this == &reference || !CanSynchronizeWith(reference))
            return normalized * m_Duration;

        const auto& markers = reference.m_SyncMarkers;
        const auto count = markers.size();

        const auto time = double(normalized) * reference.m_Duration;
        const auto upper = std::ranges::upper_bound(
            markers,
            time,
            {},
            &SAnimationSyncMarker::Time
        );

        const auto next = upper == markers.end()
            ? size_t(0)
            : size_t(upper - markers.begin());
        const auto previous = (next + count - 1) % count;

        auto start = double(markers[previous].Time);
        auto end = double(markers[next].Time);

        if (upper == markers.begin())
            start -= reference.m_Duration;
        else if (upper == markers.end())
            end += reference.m_Duration;

        const auto first = std::ranges::find(
            m_SyncMarkers,
            markers.front().Name,
            &SAnimationSyncMarker::Name
        );

        const auto offset = size_t(first - m_SyncMarkers.begin());
        const auto targetStart = double(m_SyncMarkers[(offset + previous) % count].Time);
        auto targetEnd = double(m_SyncMarkers[(offset + next) % count].Time);

        if (targetEnd <= targetStart)
            targetEnd += m_Duration;

        const auto weight = std::clamp((time - start) / (end - start), 0.0, 1.0);
        const auto targetTime = targetStart + (targetEnd - targetStart) * weight;
        const auto wrapped = std::fmod(targetTime, double(m_Duration));

        if (wrapped == 0.0 && targetTime > 0.0 && normalized > 0.0f)
            return m_Duration;

        return float(wrapped);
    }

    Ref<AnimationSequence> AnimationSequence::WithNotifies(
        std::vector<SAnimationNotify> notifies
    ) const
    {
        return Create(
            m_Name,
            m_Skeleton,
            m_Duration,
            m_Tracks,
            m_SyncMarkers,
            std::move(notifies)
        );
    }

    bool AnimationSequence::CollectNotifyOccurrences(
        const double startPhase,
        double endPhase,
        const bool looping,
        std::vector<SAnimationNotifyOccurrence>& output,
        const AnimationSequence* reference,
        const bool includeStart
    ) const
    {
        if (!std::isfinite(startPhase) ||
            !std::isfinite(endPhase) ||
            startPhase < 0.0 ||
            startPhase > 1.0 ||
            endPhase < startPhase)
        {
            EE_CORE_ERROR("Notify collection requires a valid forward phase interval.")
            return false;
        }

        if (!looping)
            endPhase = std::min(endPhase, 1.0);

        if (startPhase == endPhase || m_Notifies.empty())
        {
            output.clear();
            return true;
        }

        const auto synchronized = reference && CanSynchronizeWith(*reference);

        std::vector<SAnimationNotifyOccurrence> events;
        for (size_t index = 0; index < m_Notifies.size(); ++index)
        {
            const auto& notify = m_Notifies[index];
            auto position = double(notify.Time) / m_Duration;

            if (synchronized)
            {
                const auto marker = std::ranges::find(
                    m_SyncMarkers,
                    notify.Time,
                    &SAnimationSyncMarker::Time
                );

                if (marker != m_SyncMarkers.end())
                {
                    const auto matching = std::ranges::find(
                        reference->m_SyncMarkers,
                        marker->Name,
                        &SAnimationSyncMarker::Name
                    );
                    position = double(matching->Time) / reference->GetDuration();
                }
                else
                {
                    const auto time = reference->GetSynchronizedTime(float(position), *this);
                    position = double(time) / reference->GetDuration();
                }

                if (position == 1.0 && notify.Time < m_Duration)
                    position = 0.0;
            }

            const auto atStart = includeStart &&
                position == startPhase &&
                notify.Time < m_Duration;

            auto firstCycle = 0.0;
            auto count = 0.0;

            if (looping)
            {
                firstCycle = std::floor(startPhase - position) + 1.0;
                const auto lastCycle = std::floor(endPhase - position);
                count = std::max(0.0, lastCycle - firstCycle + 1.0);
            }
            else if (position > startPhase && position <= endPhase)
                count = 1.0;

            const auto remaining = MaxNotifyEventsPerUpdate - events.size();
            if (count + double(atStart) > double(remaining))
            {
                EE_CORE_ERROR("Animation update exceeded the notify event capacity.")
                return false;
            }

            if (atStart)
                events.push_back({ index, 0.0 });

            for (size_t cycle = 0; cycle < size_t(count); ++cycle)
                events.push_back({
                    index,
                    position + firstCycle + double(cycle) - startPhase
                });
        }

        std::ranges::stable_sort(events, [&](const auto& first, const auto& second)
        {
            if (first.PhaseOffset != second.PhaseOffset)
                return first.PhaseOffset < second.PhaseOffset;
            const auto firstAtEnd = m_Notifies[first.NotifyIndex].Time == m_Duration;
            const auto secondAtEnd = m_Notifies[second.NotifyIndex].Time == m_Duration;
            return firstAtEnd && !secondAtEnd;
        });

        output = std::move(events);
        return true;
    }
}
