#pragma once

#include <Engine/Animation/Pose.h>
#include <Engine/Mesh/SkeletalMesh.h>
#include <Engine/Animation/BoneMask.h>
#include <Engine/Animation/Animation.h>

namespace Elixir
{
    /** Pairs a mesh and an independent animation for existing character regression tests. */
    class AnimationTestCharacter final
    {
    public:
        /**
         * @brief Create an instance bound to one skeletal mesh for its entire lifetime.
         * @param mesh Skeletal mesh retained by the instance. Must not be null.
         * @return A new instance, or null if the mesh is null.
         */
        static Ref<AnimationTestCharacter> Create(Ref<const SkeletalMesh> mesh);

        /**
         * @brief Start a compatible animation source at its initial position; preserve state on failure.
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

        /** @brief Stop the base, remove nonbase layers, and restore the reference pose. */
        void Stop();

        /** @brief Pause playback while preserving the current pose and time. */
        void Pause() { m_Animation->Pause(); }

        /** @brief Resume the instance clock while preserving each layer's pause state. */
        void Resume();

        /**
         * @brief Seek base playback silently without changing layer clocks.
         *
         * Clear pending events and the base transition on success.
         *
         * @param time Finite time in seconds, wrapped or clamped by the playback mode.
         */
        bool Seek(float time);

        /**
         * @brief Advance playback, transition, and instantaneous events; preserve state on
         * failure.
         *
         * All layers advance together unless paused individually. Instance Pause freezes
         * every layer. Muted layers and empty masks emit no events. Layer events retain
         * their source sequence and layer name. The combined event limit applies to all
         * layers.
         * Only the target playback emits notifies during a fade. A blend space emits from
         * its dominant clip and maps event times through its marker reference. The first
         * positive update includes start events. Paused and zero-delta updates only deliver queued cancellation Ends.
         * Updates crossing more than AnimationSequence::MaxNotifyEventsPerUpdate fail safely.
         *
         * @param dt Finite nonnegative elapsed time in seconds.
         */
        bool Update(float dt);

        /**
         * @brief Set numeric source inputs without restarting playback.
         * @param inputs Values in the order expected by the source.
         * @return True if the inputs are accepted; otherwise, preserve the previous values.
         */
        bool SetInputs(std::span<const float> inputs);

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
        bool IsRootMotionEnabled() const { return m_Animation->IsRootMotionEnabled(); }

        /**
         * @brief Get mesh-local movement from the last successful Update.
         *
         * Post-multiply the character transform by this delta once after a successful
         * update. Paused, disabled, and zero-delta updates produce identity. Play, Seek,
         * Stop, manual pose changes, and extraction toggles clear it. Failed updates keep
         * the previous delta; consume it only after successful updates.
         */
        const SRootMotionDelta& GetRootMotionDelta() const { return m_Animation->GetRootMotionDelta(); }

        /** @brief Access this mesh's animation. */
        Animation& GetAnimation() { return *m_Animation; }

        /** @brief Inspect this mesh's animation. */
        const Animation& GetAnimation() const { return *m_Animation; }

        /**
         * @brief Attach an animation whose skeleton is compatible with this mesh.
         * @param animation Animation retained by the mesh instance. Must not be null.
         * @return False for a null or incompatible animation; retain the previous animation.
         */
        bool ReplaceAnimation(Ref<Animation> animation);

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
            return m_Animation->GetNotifyEvents();
        }

        /** @brief Get the shared mesh asset. */
        const Ref<const SkeletalMesh>& GetMesh() const { return m_Mesh; }

        /**
         * @brief Set the base local pose, stop playback, and remove nonbase layers on success.
         * @param pose One valid parent-relative transform per skeleton bone.
         */
        bool SetLocalPose(const SLocalPose& pose);

        /** @brief Get one parent-relative transform per skeleton bone. */
        const SLocalPose& GetLocalPose() const { return m_Animation->GetLocalPose(); }

        /** @brief Get one skeleton-space matrix per skeleton bone. */
        const SModelPose& GetModelPose() const { return m_Animation->GetModelPose(); }

        /** @brief Get target time in seconds using the current blend-space cycle duration. */
        float GetTime() const;

        /** @brief Get target cycle phase in [0, 1], or zero when no animation is assigned. */
        float GetNormalizedTime() const;

        /** @brief Check whether Update advances playback or a transition. */
        bool IsPlaying() const;

        /** @brief Check whether the base playback is transitioning. */
        bool IsTransitioning() const { return GetBaseLayer().IsTransitioning(); }

        /**
         * @brief Get the fixed first layer with a full-body mask and weight one.
         * @return A reference that remains valid for this mesh instance's lifetime.
         */
        AnimationLayer& GetBaseLayer() { return m_Animation->GetBaseLayer(); }

        /** @brief Inspect the fixed first layer without modifying its playback. */
        const AnimationLayer& GetBaseLayer() const { return m_Animation->GetBaseLayer(); }

        /** @brief Get the number of layers, including the base layer. */
        size_t GetLayerCount() const { return m_Animation->GetLayerCount(); }

        /**
         * @brief Add an independently controlled override layer after existing layers.
         *
         * Each bone blends with weight times its mask value. Later layers override earlier
         * layers. Changes appear on the next successful Update, including Update(0).
         * A completed nonlooping animation holds its last pose until stopped or removed.
         *
         * @param name Unique nonempty instance-local name; Base is reserved.
         * @param mask Immutable mask compatible with this mesh's skeleton.
         * @param weight Finite contribution in [0, 1].
         * @return False for invalid input or a duplicate name.
         */
        bool AddLayer(std::string name, Ref<const BoneMask> mask, float weight = 1.0f);

        /**
         * @brief Get a layer's composition settings and independent playback.
         *
         * The instance evaluates every layer during Update. Playback changes through this
         * reference appear in the composed pose on the next successful instance update.
         *
         * @param name Layer name.
         * @return Null for an unknown name. Nonbase layers live until removal;
         * Base lives with the mesh.
         */
        AnimationLayer* GetLayer(std::string_view name);

        /**
         * @brief Inspect a layer without modifying its playback.
         * @param name Layer name.
         * @return Null for an unknown name. Nonbase layers live until removal;
         * Base lives with the mesh.
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

    private:
        /**
         * @brief Bind a mesh for this instance's lifetime and initialize its reference pose.
         * @param mesh Non-null shared mesh asset retained for the entire instance lifetime.
         */
        explicit AnimationTestCharacter(Ref<const SkeletalMesh> mesh);

        const Ref<const SkeletalMesh> m_Mesh;
        Ref<Animation> m_Animation;
    };
}
