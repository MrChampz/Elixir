#pragma once

#include <Engine/Animation/Pose.h>
#include <Engine/Animation/PoseModifier.h>
#include <Engine/Animation/Skeleton.h>
#include <Engine/Animation/BoneMask.h>
#include <Engine/Animation/AnimationLayer.h>
#include <Engine/Animation/AnimationFrame.h>

namespace Elixir
{
    /**
     * @brief Owns character sources, ordered layers, and completely published animation
     * frames.
     */
    class ELIXIR_API Animation final
    {
    public:
        /**
         * @brief Create an animation without requiring a controller.
         * @param skeleton Skeleton retained for the animation's lifetime.
         * @param referencePose Sampling defaults copied on creation, one per skeleton bone.
         * @return A new animation, or null for an invalid skeleton or reference pose.
         */
        static Ref<Animation> Create(
            Ref<const Skeleton> skeleton,
            const SLocalPose& referencePose
        );

        /**
         * @brief Advance playback, transition, and instantaneous events; preserve state on
         * failure.
         *
         * All layers advance together unless paused individually. Animation Pause freezes
         * every layer. Muted layers and empty masks emit no events. Layer events retain
         * their source sequence and layer name. The combined event limit applies to all
         * layers.
         * Only the target playback emits notifies during a fade. A blend space emits from
         * its dominant clip and maps event times through its marker reference. The first
         * positive update includes start events. Paused and zero-delta updates only deliver
         * queued cancellation Ends.
         * Updates crossing more than Sequence::MaxNotifyEventsPerUpdate fail safely.
         *
         * @param dt Finite nonnegative elapsed time in seconds.
         */
        bool Update(float dt);

        /**
         * @brief Bind a layer's source inputs to this animation's named parameters.
         * @param layerName Layer whose source will consume the bound values.
         * @param bindings Numeric parameter names in source input order.
         * @return False for an unknown layer or invalid bindings. Preserve bindings on failure.
         */
        bool BindLayerInputs(std::string_view layerName, std::span<const std::string> bindings);

        /**
         * @brief Rebuild the pose from current layers without advancing their sources.
         * @return False if composition fails; preserve the published frame on failure.
         */
        bool RefreshPose();

        /**
         * @brief Start a compatible animation source at its initial position; preserve state
         * on failure.
         * @param source Animation source to play.
         * @param looping Wrap playback at the end of the animation.
         */
        bool Play(Ref<const AnimationNode> source, bool looping = true);

        /**
         * @brief Blend into a compatible animation source at its initial position.
         *
         * Both sequences advance during the fade. An interrupted fade starts from the
         * current blended pose. Zero duration switches immediately. Preserve state on failure.
         *
         * @param source Animation source to play.
         * @param fadeDuration Finite nonnegative transition duration in seconds.
         * @param looping Wrap playback at the end of the target animation.
         */
        bool Play(
            Ref<const AnimationNode> source,
            float fadeDuration,
            bool looping = true
        );

        /**
         * @brief Stop playback and restore the reference pose.
         *
         * Remove nonbase layers and detach modifiers without changing their configuration.
         */
        void Stop();

        /** @brief Pause playback while preserving the current pose and time. */
        void Pause() { m_Paused = true; }

        /** @brief Resume the animation clock while preserving each layer's pause state. */
        void Resume() { m_Paused = false; }

        /**
         * @brief Seek base playback silently without changing layer clocks.
         *
         * Clear pending events and the base transition on success.
         *
         * @param time Finite time in seconds, wrapped or clamped by the playback mode.
         */
        bool Seek(float time);

        /**
        * @brief Define manual source parameters before assigning any playback.
        * @param definitions Unique typed parameter defaults.
        */
        bool SetParameters(std::vector<SAnimationParameter> definitions);

        /** @brief Get this character's shared typed parameters. */
        AnimationParameters& GetParameters() { return *m_Parameters; }

        /** @brief Inspect this character's typed parameters. */
        const AnimationParameters& GetParameters() const { return *m_Parameters; }

        /**
         * @brief Set numeric source inputs without restarting playback.
         * @param inputs Values in the order expected by the source.
         * @return True if the inputs are accepted; otherwise, preserve the previous values.
         */
        bool SetInputs(const std::span<const float> inputs)
        {
            return GetBaseLayer().GetPlayer().SetSourceInputs(inputs);
        }

        /**
         * @brief Set numeric source inputs without restarting playback.
         * @param inputs Values in the order expected by the source.
         * @return True if the inputs are accepted; otherwise, preserve the previous values.
         */
        bool SetInputs(const std::initializer_list<float> inputs)
        {
            return SetInputs(std::span(inputs.begin(), inputs.size()));
        }

        /**
         * @brief Extract Base movement and lock the selected bone to its reference pose.
         *
         * Extraction is disabled by default. Configure before playback to avoid a visible
         * pose change. Only translation and rotation from Base move the character; other
         * layers still compose normally. This method does not advance clocks.
         *
         * Ancestors must stay in their reference pose and use uniform scale.
         *
         * @param enabled Extract movement for gameplay instead of leaving it in the pose.
         * @param boneName Root-motion bone name; empty selects bone zero.
         * @return False if the mesh or recomposed pose is invalid; previous state is retained.
         */
        bool SetRootMotionEnabled(bool enabled, std::string_view boneName = {});

        /** @brief Check whether Base extracts rigid movement from the selected bone. */
        bool IsRootMotionEnabled() const { return m_RootMotionEnabled; }

        /**
         * @brief Get mesh-local movement from the last successful Update.
         *
         * Post-multiply the character transform by this delta once after a successful
         * update. Paused, disabled, and zero-delta updates produce identity. Play, Seek,
         * Stop, manual pose changes, and extraction toggles clear it. Failed updates keep
         * the previous delta; consume it only after successful updates.
         */
        const SRootMotionDelta& GetRootMotionDelta() const { return m_Frame.RootMotion; }

        /** @brief Get the last completely validated character frame. */
        const SAnimationFrame& GetFrame() const { return m_Frame; }

        /** @brief Get the fixed skeleton used to evaluate this animation. */
        const Ref<const Skeleton>& GetSkeleton() const { return m_Skeleton; }

        /**
         * @brief Inspect one source's committed state.
         * @param layerName Layer to inspect; Base selects the permanent first layer.
         */
        std::string_view GetCurrentState(std::string_view layerName = "Base") const;

        /**
         * @brief Get events from the last successful update, in chronological order.
         *
         * Read on the same thread as playback updates. The span remains valid until the
         * next successful update, Play, Seek, Stop, or SetLocalPose. Failed operations keep
         * the previous batch; consumers must process events only after successful updates.
         * Instant, Begin, Update, and End share this list, ordered by TimeOffset.
         * Playback changes queue cancelled Ends for the next successful update, including
         * paused or zero-delta updates. Pause retains active windows without Updates.
         * Queued cancellation Ends do not consume the authored event budget.
         */
        std::span<const SAnimationNotifyEvent> GetNotifyEvents() const
        {
            return m_Frame.NotifyEvents;
        }

        /**
         * @brief Set the base local pose, stop playback, and remove nonbase layers on success.
         * @param pose One valid parent-relative transform per skeleton bone.
         */
        bool SetLocalPose(const SLocalPose& pose);

        /** @brief Get one parent-relative transform per skeleton bone. */
        const SLocalPose& GetLocalPose() const { return m_Frame.LocalPose; }

        /** @brief Get one skeleton-space matrix per skeleton bone. */
        const SModelPose& GetModelPose() const { return m_Frame.ModelPose; }

        /** @brief Get target time in seconds using the current blend-space cycle duration. */
        float GetTime() const
        {
            return GetBaseLayer().GetPlayer().GetTime();
        }

        /** @brief Get target cycle phase in [0, 1], or zero when no animation is assigned. */
        float GetNormalizedTime() const
        {
            return GetBaseLayer().GetPlayer().GetNormalizedTime();
        }

        /** @brief Check whether Update advances playback or a transition. */
        bool IsPlaying() const;

        /** @brief Check whether the base playback is transitioning. */
        bool IsTransitioning() const
        {
            return GetBaseLayer().GetPlayer().IsTransitioning();
        }

        /**
         * @brief Get the fixed first layer with a full-body mask and weight one.
         * @return A reference that remains valid for this animation's lifetime.
         */
        AnimationLayer& GetBaseLayer() { return *m_Layers.front(); }

        /** @brief Inspect the fixed first layer without modifying its playback. */
        const AnimationLayer& GetBaseLayer() const { return *m_Layers.front(); }

        /** @brief Get the number of layers, including the base layer. */
        size_t GetLayerCount() const;

        /**
         * @brief Add an independently controlled override layer after existing layers.
         *
         * Each bone blends with weight times its mask value. Later layers override earlier
         * layers. Changes appear on the next successful Update, including Update(0).
         * A completed nonlooping animation holds its last pose until stopped or removed.
         *
         * @param name Unique nonempty animation-local name; Base is reserved.
         * @param mask Immutable mask compatible with this mesh's skeleton.
         * @param weight Finite contribution in [0, 1].
         * @return False for invalid input or a duplicate name.
         */
        bool AddLayer(std::string name, Ref<const BoneMask> mask, float weight = 1.0f);

        /**
         * @brief Get a layer's composition settings and independent playback.
         *
         * The animation evaluates every layer during Update. Playback changes through this
         * reference appear in the composed pose on the next successful animation update.
         *
         * @param name Layer name.
         * @return Null for an unknown or ambiguous name. Source changes may invalidate
         * nested controls. The outer Base control lives with the animation.
         */
        AnimationLayer* GetLayer(std::string_view name);

        /**
         * @brief Inspect a layer without modifying its playback.
         * @param name Layer name.
         * @return Null for an unknown or ambiguous name. Source changes may invalidate
         * nested controls. The outer Base control lives with the animation.
         */
        const AnimationLayer* GetLayer(std::string_view name) const;

        /**
         * @brief Set a layer's contribution for the next Update without resetting its time.
         * @param name Layer name.
         * @param weight Finite contribution in [0, 1]; zero mutes events but advances time.
         */
        bool SetLayerWeight(std::string_view name, float weight);

        /**
         * @brief Get a layer's contribution, or an empty result if it does not exist.
         * @param name Layer name.
         */
        std::optional<float> GetLayerWeight(std::string_view name) const;

        /**
         * @brief Remove a layer; the next Update rebuilds the composed pose.
         * @param name Layer name.
         * @return False if the layer does not exist or is the base layer.
         */
        bool RemoveLayer(std::string_view name);

        /** @brief Remove all nonbase layers; the next Update rebuilds the base pose. */
        void ClearLayers();

        /**
         * @brief Retain ordered corrections applied before publishing each candidate pose.
         *
         * Changes take effect on the next evaluation, including paused Update(0), Seek,
         * Play, and SetLocalPose. A failed correction preserves the previous frame and clocks.
         * Stop detaches modifiers but does not modify their externally owned configuration.
         *
         * @param modifiers Corrections in execution order; empty removes all corrections.
         * @return False for a null modifier; retain the previous list on failure.
         */
        bool SetPoseModifiers(std::span<const Ref<const PoseModifier>> modifiers);

        /** @brief Inspect modifiers retained until the next replacement or Stop. */
        std::span<const Ref<const PoseModifier>> GetPoseModifiers() const
        {
            return m_PoseModifiers;
        }

        Animation(const Animation& other) = delete;
        Animation& operator=(const Animation& other) = delete;

      private:
        /**
         * @brief Retain a fixed skeleton and copy its mesh-specific reference pose.
         * @param skeleton Skeleton retained for this animation's lifetime.
         * @param referencePose Mesh-specific defaults copied into this animation.
         */
        Animation(
            Ref<const Skeleton> skeleton,
            const SLocalPose& referencePose
        );

        /**
         * @brief Compose ordered layer poses and validate the final model matrices.
         * @param evaluations Evaluated layers, or empty to use their current local poses.
         * @param local Receives the complete blended local pose.
         * @param model Receives the complete skeleton-space pose.
         * @param replacement Optional layer preview used before committing a playback change.
         */
        bool ComposePose(
            std::span<const SAnimationPlayerEvaluation> evaluations,
            SLocalPose& local,
            SModelPose& model,
            const AnimationLayer* replacement = nullptr
        ) const;

        /**
         * @brief Apply a base-layer change after validating the complete composed pose.
         * @param layer Candidate with the base layer's fixed name, mask, and skeleton.
         */
        bool ApplyBaseLayer(const AnimationLayer& layer);

        /**
         * @brief Validate static ancestors and lock the selected root in a candidate pose.
         * @param pose Complete local pose to validate and modify.
         */
        bool LockRootMotion(SLocalPose& pose) const;

        /**
         * @brief Retain cancelled window events before removing a layer.
         * @param layer Layer whose active and pending windows must be ended.
         */
        void QueueLayerNotifyEnds(AnimationLayer& layer);

        /**
         * @brief Validate pose corrections and preserve the extracted movement frame.
         * @param local Candidate transforms modified by each modifier in order.
         * @param model Receives model matrices rebuilt after each successful correction.
         */
        bool ApplyPoseModifiers(SLocalPose& local, SModelPose& model) const;

        std::vector<Ref<const PoseModifier>> m_PoseModifiers;
        Ref<AnimationParameters> m_Parameters = CreateRef<AnimationParameters>();

        const Ref<const Skeleton> m_Skeleton;
        const SLocalPose m_ReferencePose;

        /** The first layer is Base and remains alive for this animation's lifetime. */
        std::vector<Scope<AnimationLayer>> m_Layers;
        std::vector<SAnimationNotifyEvent> m_PendingNotifyEnds;

        SAnimationFrame m_Frame;
        bool m_Paused = false;

        bool m_RootMotionEnabled = false;
        uint32_t m_RootMotionBone = 0;
        SRootMotionDelta m_RootMotionParent;
        float m_RootMotionParentScale = 1;
    };
}
