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
        std::vector<SBoneAnimationTrack> tracks
    )
    {
        if (!skeleton || !std::isfinite(duration) || duration < 0)
        {
            EE_CORE_ERROR("AnimationSequence requires a skeleton and a finite nonnegative duration.")
            return nullptr;
        }

        std::unordered_set<uint32_t> indices;

        for (const auto& track : tracks)
        {
            const auto vectorValid = [](const glm::vec3& v)
            {
                return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
            };

            const auto rotationValid = [](const glm::quat& q)
            {
                return std::isfinite(q.x) && std::isfinite(q.y) && std::isfinite(q.z) &&
                    std::isfinite(q.w) && std::abs(glm::dot(q, q) - 1.0f) < 1e-4f;
            };

            const auto scaleValid = [&](const glm::vec3& v)
            {
                return vectorValid(v) && std::abs(v.x) >= 1e-8f && std::abs(v.y) >= 1e-8f &&
                    std::abs(v.z) >= 1e-8f;
            };

            const auto modeValid = [](const auto mode)
            {
                return mode == EBoneInterpolation::Step || mode == EBoneInterpolation::Linear;
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
            std::move(tracks)
        ));
    }

    AnimationSequence::AnimationSequence(
        std::string name,
        Ref<const Skeleton> skeleton,
        const float duration,
        std::vector<SBoneAnimationTrack> tracks
    ) : m_Name(std::move(name)),
        m_Skeleton(std::move(skeleton)),
        m_Duration(duration),
        m_Tracks(std::move(tracks)) {}

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
}
