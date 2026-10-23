#pragma once

#include <Engine/Mesh/SkeletalMesh.h>
#include <Engine/Animation/AnimationLayer.h>
#include <Engine/Animation/BoneMask.h>
#include <Engine/Animation/TwoBoneIK.h>

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

        /** @brief Stop playback, clear IK and extra layers, and restore the reference pose. */
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
        const SRootMotionDelta& GetRootMotionDelta() const { return m_RootMotionDelta; }

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
         * @brief Get window lifecycle events from the last successful Update.
         *
         * Process the batch once after success. Playback commands and removed layers queue
         * cancelled Ends for the next successful Update, even when paused or given zero time.
         * Queued cancellation Ends are delivered in addition to the authored event budget.
         */
        std::span<const SAnimationNotifyStateEvent> GetNotifyStateEvents() const
        {
            return m_NotifyStateEvents;
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

        /**
         * @brief Replace the ordered two-bone constraints for the next pose evaluation.
         *
         * Targets use skeleton space unless TargetReference selects a bone or socket.
         * Then position and rotation are local offsets in that reference frame. Poles
         * always use skeleton space, before the character's world transform. References
         * read the current candidate pose, including earlier constraints. A reference
         * cannot be affected by its own constraint or any later active constraint;
         * order drivers before followers. Invalid references or dependencies are rejected.
         * Referenced rotations require uniform positive scales through the reference
         * hierarchy and socket; position-only references support affine scale.
         * Each chain uses the end bone's parent and grandparent. Solve after all layers
         * and root-motion locking, before sockets and skinning. Later constraints see
         * earlier results. Local translations and scales are retained. Without a target
         * rotation, only the two parent rotations change. With one, also blend the end
         * orientation after positional correction toward that skeleton-space rotation.
         * The same weight controls both adjustments. Weight blends rotations, so intermediate
         * weights do not linearly interpolate the end position.
         *
         * Active chains require nonzero segment lengths and uniform positive scales
         * through their ancestors. Unreachable targets clamp to the reachable range.
         * Degenerate pole directions fall back to the animated bend or a stable axis.
         * Changes appear on Update, including Update(0) while paused. Failed updates
         * retain the last accepted pose, playback clocks, root delta, and notify batches.
         * Stop clears constraints. Read and modify them on the pose update thread.
         *
         * @param constraints Copied ordered list; empty disables IK on the next evaluation.
         * @return False for invalid bones, references, dependencies, nonfinite settings,
         * nonunit target rotations, or weights outside [0, 1].
         */
        bool SetTwoBoneIKConstraints(std::span<const STwoBoneIKConstraint> constraints);

        /** @brief Inspect the ordered constraints used by the next pose evaluation. */
        std::span<const STwoBoneIKConstraint> GetTwoBoneIKConstraints() const
        {
            return m_TwoBoneIKConstraints;
        }

        /**
         * @brief Get an attachment matrix from the final composed pose in skeleton space.
         *
         * Includes all layers, montages, and root-motion locking. This reads the last
         * accepted model pose without advancing animation or applying inverse bind matrices.
         * Multiply by the character's world transform to place an attachment in the scene.
         * Read on the same thread as pose updates.
         *
         * @param name Case-sensitive socket name in the mesh's skeleton.
         * @return Empty for a missing socket, mesh, pose, or a nonfinite result.
         */
        std::optional<glm::mat4> GetSocketTransform(std::string_view name) const;

        /**
         * @brief Read a socket using a cached index from Skeleton::FindSocket.
         * @param socketIndex Index in this mesh's Skeleton::GetSockets().
         * @return Skeleton-space matrix, or empty for an invalid index, pose, or result.
         */
        std::optional<glm::mat4> GetSocketTransform(size_t socketIndex) const;

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

        /**
         * @brief Validate static ancestors and lock the selected root in a candidate pose.
         * @param pose Complete local pose to validate and modify.
         */
        bool LockRootMotion(std::span<SBoneTransform> pose) const;

        /**
         * @brief Retain cancelled window events before removing a layer.
         * @param layer Layer whose active and pending windows must be ended.
         */
        void QueueLayerNotifyStateEnds(AnimationLayer& layer);

        /**
         * @brief Solve ordered constraints on a candidate pose before accepting the frame.
         * @param local Candidate parent-relative transforms.
         * @param model Candidate skeleton-space matrices, rebuilt after each active chain.
         */
        bool ApplyTwoBoneIK(
            std::vector<SBoneTransform>& local,
            std::vector<glm::mat4>& model
        ) const;

        std::vector<STwoBoneIKConstraint> m_TwoBoneIKConstraints;

        const Ref<const SkeletalMesh> m_Mesh;

        /** The first layer is Base and remains alive for this instance's lifetime. */
        std::vector<Scope<AnimationLayer>> m_Layers;
        std::vector<SAnimationNotifyEvent> m_NotifyEvents;
        std::vector<SAnimationNotifyStateEvent> m_NotifyStateEvents;
        std::vector<SAnimationNotifyStateEvent> m_PendingNotifyStateEnds;

        std::vector<SBoneTransform> m_LocalPose;
        std::vector<glm::mat4> m_ModelPose;

        bool m_Paused = false;
        bool m_RootMotionEnabled = false;
        SRootMotionDelta m_RootMotionDelta;
        uint32_t m_RootMotionBone = 0;
        SRootMotionDelta m_RootMotionParent;
        float m_RootMotionParentScale = 1;
    };
}
