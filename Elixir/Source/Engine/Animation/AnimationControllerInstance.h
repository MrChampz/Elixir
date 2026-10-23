#pragma once

#include <Engine/Animation/AnimationController.h>

namespace Elixir
{
    class SkeletalMeshInstance;

    /** Coordinates shared parameters, layer state machines, and playback for one character. */
    class ELIXIR_API AnimationControllerInstance final
    {
    public:
        /**
         * @brief Bind a controller to one character without changing its pose yet.
         * @param controller Shared layer, state-machine, and parameter definitions.
         * @param instance Mesh instance whose layers and playback this controller drives.
         * @pre The mesh must outlive this controller and have only Base on its first update.
         */
        AnimationControllerInstance(
            Ref<const AnimationController> controller,
            SkeletalMeshInstance& instance
        );

        AnimationControllerInstance(const AnimationControllerInstance&) = delete;
        AnimationControllerInstance& operator=(const AnimationControllerInstance&) = delete;

        /**
         * @brief Set a finite numeric parameter; preserve the old value on failure.
         * @param name Name of an existing numeric parameter.
         * @param value Finite parameter value.
         */
        bool SetFloat(std::string_view name, float value);

        /**
         * @brief Set a boolean parameter; reject unknown names and numeric parameters.
         * @param name Name of an existing boolean parameter.
         * @param value New parameter value.
         */
        bool SetBool(std::string_view name, bool value);

        /**
         * @brief Get a numeric parameter, or an empty result for a missing or wrong type.
         * @param name Parameter name.
         */
        std::optional<float> GetFloat(std::string_view name) const;

        /**
         * @brief Get a boolean parameter, or an empty result for a missing or wrong type.
         * @param name Parameter name.
         */
        std::optional<bool> GetBool(std::string_view name) const;

        /**
         * @brief Update layer sources, then advance and compose the mesh once.
         *
         * The first update creates configured layers and starts their animation sources.
         * Each machine takes at most one transition in declaration order. Direct sources
         * keep playing without state transitions. Parameters are shared by every layer.
         * Drive playback and mesh updates through this controller.
         * Removing a configured layer makes subsequent updates fail safely.
         *
         * @param dt Finite nonnegative elapsed time in seconds.
         * @return False for invalid input, missing layers, or failed playback or composition.
         */
        bool Update(float dt);

        /**
         * @brief Restore shared parameters and restart every layer source on the next update.
         */
        void Reset();

        /**
         * @brief Get a machine's target state; empty for direct sources or before startup.
         * @param layerName Name of the configured layer; defaults to Base.
         */
        std::string_view GetCurrentState(std::string_view layerName = BaseLayerName) const;

    private:
        struct SLayerPlayback
        {
            /** Direct playback starts once until Reset. */
            bool Started = false;

            /** Only state-machine sources select a state. */
            std::optional<size_t> State;
        };

        /**
         * @brief Find a parameter without allocating a temporary string.
         * @param name Parameter name.
         */
        std::optional<size_t> FindParameter(std::string_view name) const;

        /**
         * @brief Compare a parameter with a validated condition.
         * @param condition Condition whose parameter index and type were resolved at creation.
         */
        bool Matches(const AnimationController::SCondition& condition) const;

        /**
         * @brief Start a sequence on a layer with the requested fade.
         * @param layerIndex Layer index in the controller.
         * @param source Validated sequence playback.
         * @param fadeDuration Fade duration in seconds.
         * @param mode Method used to smooth the source change.
         */
        bool PlaySource(
            size_t layerIndex,
            const AnimationController::SSequenceSource& source,
            float fadeDuration,
            EAnimationTransitionMode mode = EAnimationTransitionMode::CrossFade
        );

        /**
         * @brief Start a blend space on a layer with the requested fade.
         * @param layerIndex Layer index in the controller.
         * @param source Validated blend-space playback and numeric bindings.
         * @param fadeDuration Fade duration in seconds.
         * @param mode Method used to smooth the source change.
         */
        bool PlaySource(
            size_t layerIndex,
            const AnimationController::SBlendSpaceSource& source,
            float fadeDuration,
            EAnimationTransitionMode mode = EAnimationTransitionMode::CrossFade
        );

        /**
         * @brief Dispatch a state's playback to its sequence or blend-space source.
         * @param layerIndex Layer index in the controller.
         * @param source Validated playback alternative selected by the state.
         * @param fadeDuration Fade duration in seconds.
         * @param mode Method used to smooth the source change.
         */
        bool PlaySource(
            size_t layerIndex,
            const AnimationController::TPlaybackSource& source,
            float fadeDuration,
            EAnimationTransitionMode mode = EAnimationTransitionMode::CrossFade
        );

        /**
         * @brief Read a playback source's axis values into reusable storage.
         * @param source Validated source with resolved numeric parameter bindings.
         */
        std::span<const float> GetBlendValues(
            const AnimationController::SBlendSpaceSource& source
        );

        /**
         * @brief Start or refresh one layer's direct playback or state machine.
         * @param layerIndex Layer index in the controller.
         */
        bool UpdateLayerSource(size_t layerIndex);

        /** @brief Create configured layers in composition order on a fresh mesh instance. */
        bool InitializeLayers();

        /**
         * @brief Start direct sequence playback once until Reset.
         * @param layerIndex Layer index in the controller.
         * @param source Validated sequence playback.
         */
        bool UpdateSource(
            size_t layerIndex,
            const AnimationController::SSequenceSource& source
        );

        /**
         * @brief Start a direct blend space and refresh its numeric coordinates.
         * @param layerIndex Layer index in the controller.
         * @param source Validated blend-space playback and numeric bindings.
         */
        bool UpdateSource(
            size_t layerIndex,
            const AnimationController::SBlendSpaceSource& source
        );

        /**
         * @brief Evaluate one layer's state machine without advancing its clock.
         * @param layerIndex Layer index in the controller.
         * @param machine Compiled states and transition rules for this layer.
         */
        bool UpdateSource(
            size_t layerIndex,
            const AnimationController::SStateMachine& machine
        );

        Ref<const AnimationController> m_Controller;
        SkeletalMeshInstance& m_Instance;

        /** Skeleton compatibility is fixed when this controller instance is constructed. */
        bool m_Compatible = false;
        bool m_Initialized = false;

        std::vector<TAnimationParameterValue> m_Parameters;
        std::vector<float> m_BlendValues;
        std::vector<SLayerPlayback> m_Playback;
    };
}
