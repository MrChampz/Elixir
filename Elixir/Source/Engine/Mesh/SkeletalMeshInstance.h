#pragma once

#include <Engine/Mesh/SkeletalMesh.h>
#include <Engine/Animation/AnimationLayer.h>
#include <Engine/Animation/BoneMask.h>

namespace Elixir
{
    /** @brief Owns a mesh instance's pose, playback, and animation transitions. */
    class ELIXIR_API SkeletalMeshInstance final
    {
    public:
        /**
         * @brief Bind a mesh for this instance's lifetime and initialize its reference pose.
         * @param mesh Non-null shared mesh asset retained for the entire instance lifetime.
         */
        explicit SkeletalMeshInstance(Ref<const SkeletalMesh> mesh);

        /**
         * @brief Start a compatible sequence at time zero; preserve state on failure.
         * @param sequence Animation to play.
         * @param looping Wrap playback at the end of the animation.
         */
        bool Play(Ref<const AnimationSequence> sequence, bool looping = true);

        /**
         * @brief Blend into a compatible sequence starting at time zero.
         *
         * Both sequences advance during the fade. An interrupted fade starts from the
         * current blended pose. Zero duration switches immediately. Preserve state on failure.
         *
         * @param sequence Animation to play.
         * @param fadeDuration Finite nonnegative transition duration in seconds.
         * @param looping Wrap playback at the end of the target animation.
         */
        bool Play(
            Ref<const AnimationSequence> sequence,
            float fadeDuration,
            bool looping = true
        );

        /**
         * @brief Start any compatible blend space at cycle phase zero.
         * @param blendSpace Shared pose source, independent of its dimension count.
         * @param values One finite coordinate per axis; copied into playback storage.
         * @param fadeDuration Finite nonnegative fade duration in seconds.
         * @param looping Wrap the normalized cycle phase at one.
         */
        bool Play(
            Ref<const BlendSpace> blendSpace,
            std::span<const float> values,
            float fadeDuration = 0.0f,
            bool looping = true
        );

        /**
         * @brief Start a blend space with a temporary list of coordinates.
         * @param blendSpace Shared compatible pose source.
         * @param values One finite coordinate per axis.
         * @param fadeDuration Finite nonnegative fade duration in seconds.
         * @param looping Wrap the cycle phase at one.
         */
        bool Play(
            Ref<const BlendSpace> blendSpace,
            const std::initializer_list<float> values,
            const float fadeDuration = 0.0f,
            const bool looping = true
        )
        {
            return Play(
                std::move(blendSpace),
                std::span(values.begin(), values.size()),
                fadeDuration,
                looping
            );
        }

        /**
         * @brief Set coordinates for the next update without resetting phase or fade.
         * @param values One finite coordinate per active blend-space axis; copied on success.
         */
        bool SetBlendSpaceValue(std::span<const float> values);

        /**
         * @brief Set blend coordinates using a temporary list.
         * @param values One finite coordinate per active blend-space axis.
         */
        bool SetBlendSpaceValue(const std::initializer_list<float> values)
        {
            return SetBlendSpaceValue(std::span(values.begin(), values.size()));
        }

        /** @brief Pause playback while preserving the current pose and time. */
        void Pause() { m_Paused = true; }

        /** @brief Resume the instance clock while preserving each layer's pause state. */
        void Resume();

        /** @brief Stop the base, remove nonbase layers, and restore the reference pose. */
        void Stop();

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
         * positive update includes start events. Paused and zero-delta updates emit none.
         * Updates crossing more than AnimationSequence::MaxNotifyEventsPerUpdate fail safely.
         *
         * @param dt Finite nonnegative elapsed time in seconds.
         */
        bool Update(float dt);

        /**
         * @brief Get events from the last successful update, in chronological order.
         *
         * Read on the same thread as playback updates. The span remains valid until the
         * next successful update, Play, Seek, Stop, or SetLocalPose. Failed operations keep
         * the previous batch; consumers must process events only after successful updates.
         */
        std::span<const SAnimationNotifyEvent> GetNotifyEvents() const
        {
            return m_NotifyEvents;
        }

        /**
         * @brief Seek base playback silently without changing layer clocks.
         *
         * Clear pending events and the base transition on success.
         *
         * @param time Finite time in seconds, wrapped or clamped by the playback mode.
         */
        bool Seek(float time);

        /** @brief Get the shared mesh asset. */
        const Ref<const SkeletalMesh>& GetMesh() const { return m_Mesh; }

        /**
         * @brief Set the base local pose, stop playback, and remove nonbase layers on success.
         * @param pose One valid parent-relative transform per skeleton bone.
         */
        bool SetLocalPose(std::span<const SBoneTransform> pose);

        /** @brief Get one parent-relative transform per skeleton bone. */
        const std::vector<SBoneTransform>& GetLocalPose() const { return m_LocalPose; }

        /** @brief Get one skeleton-space matrix per skeleton bone. */
        const std::vector<glm::mat4>& GetModelPose() const { return m_ModelPose; }

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
        AnimationLayer& GetBaseLayer() { return *m_Layers.front(); }

        /** @brief Inspect the fixed first layer without modifying its playback. */
        const AnimationLayer& GetBaseLayer() const { return *m_Layers.front(); }

        /** @brief Get the number of layers, including the base layer. */
        size_t GetLayerCount() const { return m_Layers.size(); }

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
         * @brief Compose ordered layer poses and validate the final model matrices.
         * @param evaluations Evaluated layers, or empty to use their current local poses.
         * @param local Receives the complete blended local pose.
         * @param model Receives the complete skeleton-space pose.
         * @param replacement Optional layer preview used before committing a playback change.
         */
        bool ComposePose(
            std::span<const AnimationLayer::SLayerEvaluation> evaluations,
            std::vector<SBoneTransform>& local,
            std::vector<glm::mat4>& model,
            const AnimationLayer* replacement = nullptr
        ) const;

        /**
         * @brief Apply a base-layer change after validating the complete composed pose.
         * @param layer Candidate with the base layer's fixed name, mask, and skeleton.
         */
        bool ApplyBaseLayer(const AnimationLayer& layer);

        const Ref<const SkeletalMesh> m_Mesh;

        /** The first layer is Base and remains alive for this instance's lifetime. */
        std::vector<Scope<AnimationLayer>> m_Layers;
        std::vector<SAnimationNotifyEvent> m_NotifyEvents;

        std::vector<SBoneTransform> m_LocalPose;
        std::vector<glm::mat4> m_ModelPose;

        bool m_Paused = false;
    };
}
