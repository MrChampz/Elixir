#pragma once

#include <Engine/Animation/AnimationSequence.h>

namespace Elixir
{
    /** Describes a one-shot action played over a layer's continuing animation. */
    struct SAnimationMontageCreateInfo
    {
        std::string Name;
        Ref<const AnimationSequence> Sequence;

        /** Seconds to reach full contribution, starting at the sequence beginning. */
        float BlendIn = 0.15f;

        /** Seconds to return to the layer, ending at the sequence end. */
        float BlendOut = 0.15f;

        /** Allow another montage or StopMontage to interrupt this action. */
        bool Interruptible = true;
    };

    /** Stores an immutable action; its target layer owns the playback clock. */
    class ELIXIR_API AnimationMontage final
    {
    public:
        /**
         * @brief Create a named action with finite nonnegative blend times.
         * @param info Sequence with positive duration and blend settings.
         * @return Null for invalid settings; blend times may overlap within the clip.
         */
        static Ref<AnimationMontage> Create(SAnimationMontageCreateInfo info);

        /** @brief Get the action name. */
        const std::string& GetName() const { return m_Info.Name; }

        /** @brief Get the sequence played once by this action. */
        const Ref<const AnimationSequence>& GetSequence() const { return m_Info.Sequence; }

        /** @brief Get the entrance duration in seconds. */
        float GetBlendIn() const { return m_Info.BlendIn; }

        /** @brief Get the automatic exit duration in seconds. */
        float GetBlendOut() const { return m_Info.BlendOut; }

        /** @brief Check whether playback can be stopped or replaced by another action. */
        bool IsInterruptible() const { return m_Info.Interruptible; }

    private:
        /**
         * @brief Store validated action settings.
         * @param info Immutable settings retained by this asset.
         */
        explicit AnimationMontage(SAnimationMontageCreateInfo info);

        SAnimationMontageCreateInfo m_Info;
    };
}
