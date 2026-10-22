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
         * @brief Evaluate every layer's state machine, then advance and compose the mesh once.
         *
         * The first update creates the configured layers and starts their initial states.
         * Each machine takes at most one transition in declaration order. Parameters are
         * shared by all machines. Drive playback and mesh updates through this controller.
         * Removing a configured layer makes subsequent updates fail safely.
         *
         * @param dt Finite nonnegative elapsed time in seconds.
         * @return False for invalid input, missing layers, or failed playback or composition.
         */
        bool Update(float dt);

        /**
         * @brief Restore shared parameters and restart all machines on the next update.
         */
        void Reset();

        /**
         * @brief Get a layer's target state, or an empty view before startup or if unknown.
         * @param layerName Name of the configured layer; defaults to Base.
         */
        std::string_view GetCurrentState(std::string_view layerName = BaseLayerName) const;

    private:
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
         * @brief Start a state's sequence or blend space with the requested fade.
         * @param machine Layer and state-machine index in the shared controller.
         * @param state Valid state index in that machine.
         * @param fadeDuration Fade duration in seconds.
         */
        bool PlayState(size_t machine, size_t state, float fadeDuration);

        /**
         * @brief Read a state's axis values into reusable per-instance storage.
         * @param machine Layer and state-machine index.
         * @param state Valid state index with resolved parameter bindings.
         */
        std::span<const float> GetBlendValues(size_t machine, size_t state);

        /** @brief Create configured layers in composition order on a fresh mesh instance. */
        bool InitializeLayers();

        /**
         * @brief Evaluate one layer's state machine without advancing its clock.
         * @param machine Layer and state-machine index.
         */
        bool UpdateStateMachine(size_t machine);

        Ref<const AnimationController> m_Controller;
        SkeletalMeshInstance& m_Instance;

        /** Skeleton compatibility is fixed when this controller instance is constructed. */
        bool m_Compatible = false;
        bool m_Initialized = false;

        std::vector<TAnimationParameterValue> m_Parameters;
        std::vector<float> m_BlendValues;
        std::vector<std::optional<size_t>> m_States;
    };
}
