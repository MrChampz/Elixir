#pragma once

#include <Engine/Animation/AnimationPlayer.h>
#include <Engine/Animation/BoneMask.h>

namespace Elixir
{
    /** How a layer contributes to the existing base layer pose. */
    enum class EAnimationLayerBlendMode : uint8_t
    {
        Override, Additive
    };

    /** Name reserved for the full-body base control. */
    inline constexpr std::string BaseLayerName = "Base";

    /** Applies an animation player's pose using a mask, weight, and blend mode. */
    class ELIXIR_API AnimationLayer final
    {
        friend class Animation;
        friend class BlendNodeEvaluator;

    public:
        /** @brief Get the player that owns this layer's playback and evaluation state. */
        AnimationPlayer& GetPlayer() { return m_Player; }

        /** @brief Inspect this layer's playback and evaluation state. */
        const AnimationPlayer& GetPlayer() const { return m_Player; }

        /** @brief Get this layer's stable composition name. */
        const std::string& GetName() const { return m_Name; }

        /** @brief Get the immutable mask used to compose the player pose. */
        const Ref<const BoneMask>& GetMask() const { return m_Mask; }

        /** @brief Get the layer's contribution in [0, 1]. */
        float GetWeight() const { return m_Weight; }

        /**
         * @brief Set the layer's contribution without resetting its playback.
         * @param weight Finite weight in [0, 1]; the base layer requires one.
         * @return False for invalid input; zero mutes events but still advances playback.
         */
        bool SetWeight(float weight);

        /** @brief Get the layer's pose composition mode. */
        EAnimationLayerBlendMode GetBlendMode() const { return m_BlendMode; }

        /**
         * @brief Set how this layer combines its pose on the next animation update.
         * @param mode Override blends absolute poses; Additive applies reference differences.
         * @return False for an unknown mode or an additive base layer; playback is preserved.
         */
        bool SetBlendMode(EAnimationLayerBlendMode mode);

        /**
         * @brief Set the local reference pose used to calculate additive differences.
         *
         * The same reference applies to every sequence and blend-space sample on this layer.
         * Channels without animation keys retain the mesh's reference transforms.
         * Changes affect composition on the next animation update without seeking playback.
         *
         * @param pose One valid local transform per bone; empty restores the mesh reference.
         * @return False for invalid input; the previous reference remains unchanged.
         */
        bool SetAdditiveReferencePose(const SLocalPose& pose);

        /** @brief Get the custom additive reference, or the mesh's local reference pose. */
        const SLocalPose& GetAdditiveReferencePose() const
        {
            return m_AdditiveReferencePose.LocalTransforms.empty()
                ? m_Player.GetReferencePose()
                : m_AdditiveReferencePose;
        }

        /** @brief Get the fixed skeleton, or null for invalid construction. */
        const Ref<const Skeleton>& GetSkeleton() const { return m_Player.GetSkeleton(); }

    private:
        /**
         * @brief Create a layer bound to a mask and reference pose.
         * @param name Stable name used for layer lookup and player notifications.
         * @param mask Immutable mask defining the skeleton and bone weights.
         * @param weight Validated layer contribution in [0, 1].
         * @param referencePose One local reference transform per bone.
         * @pre mask is non-null.
         */
        AnimationLayer(
            std::string name,
            Ref<const BoneMask> mask,
            float weight,
            const SLocalPose& referencePose
        );

        /**
         * @brief Copy composition settings and independent playback for a candidate change.
         * @param other Layer whose settings and player are copied.
         */
        AnimationLayer(const AnimationLayer& other) = default;

        /**
         * @brief Replace a candidate layer after its complete pose has been validated.
         * @param other Layer whose composition settings and player replace this layer.
         */
        AnimationLayer& operator=(const AnimationLayer& other) = default;

        /**
         * @brief Publish a candidate layer without cloning its player sources.
         * @param other Layer whose composition settings and player are consumed.
         */
        AnimationLayer& operator=(AnimationLayer&& other) noexcept = default;

        /**
         * @brief Blend the player's pose into a candidate using this layer's settings.
         * @param output Underlying local pose to modify; discard it on failure.
         * @param pose Absolute local pose produced by this layer's player.
         * @return False for invalid dimensions or invalid resulting transforms.
         */
        bool BlendPose(SLocalPose& output, const SLocalPose& pose) const;

        /**
         * @brief Compose authored scalar channels using the layer weight and blend mode.
         * @param output Accumulated channels modified only on success.
         * @param curves Candidate channels from this layer's player.
         */
        bool BlendCurves(
            AnimationCurveValues& output,
            const AnimationCurveValues& curves
        ) const;

        std::string m_Name;
        EAnimationLayerBlendMode m_BlendMode = EAnimationLayerBlendMode::Override;
        float m_Weight = 1;
        bool m_Base = false;
        Ref<const BoneMask> m_Mask;
        SLocalPose m_AdditiveReferencePose;
        AnimationPlayer m_Player;
    };
}
