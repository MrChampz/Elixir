#pragma once

#include <Engine/Animation/AnimationSequence.h>

namespace Elixir
{
    /** Names a timeline range ending at the next section start or the sequence end. */
    struct SAnimationMontageSection
    {
        std::string Name;

        /** Sequence time in seconds; the first section starts at zero. */
        float StartTime = 0.0f;

        /** Empty optional follows timeline order; empty string ends the montage. */
        std::optional<std::string> NextSection;
    };

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

        /** Ordered section starts and default links; empty creates one Default section. */
        std::vector<SAnimationMontageSection> Sections;
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

        /** @brief Get the named ranges in timeline order. */
        const std::vector<SAnimationMontageSection>& GetSections() const
        {
            return m_Info.Sections;
        }

        /**
         * @brief Find a section without changing playback.
         * @param name Case-sensitive section name.
         * @return Empty if the name does not exist.
         */
        std::optional<size_t> FindSection(std::string_view name) const;

        /**
         * @brief Get the timeline end of a valid section in sequence seconds.
         * @param index Section index in GetSections.
         * @return Zero for an invalid index.
         */
        float GetSectionEnd(size_t index) const;

        /**
         * @brief Get the compiled default continuation for a valid section.
         * @param index Section index in GetSections.
         * @return Empty when the section ends playback or the index is invalid.
         */
        std::optional<size_t> GetNextSection(size_t index) const;

    private:
        /**
         * @brief Store validated action settings.
         * @param info Immutable settings retained by this asset.
         */
        explicit AnimationMontage(SAnimationMontageCreateInfo info);

        SAnimationMontageCreateInfo m_Info;
        std::vector<std::optional<size_t>> m_NextSections;
    };
}
