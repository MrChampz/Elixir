#pragma once

#include <Engine/Animation/AnimationController.h>

namespace Elixir
{
    class SkeletalMeshInstance;
    class SkeletalMesh;

    /** Owns parameter values and state-machine playback decisions for one mesh instance. */
    class ELIXIR_API AnimationControllerInstance final
    {
    public:
        /**
         * @brief Bind a shared controller to one character without changing its pose yet.
         * @param controller Shared state-machine definition.
         * @param instance Character whose playback this controller drives.
         * @pre The mesh instance must outlive this controller instance.
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
         * @brief Evaluate one transition, then advance playback and blending.
         *
         * The first update starts the initial state. Check outgoing transitions in definition
         * order and take at most one per update. Transitions can interrupt an active fade.
         * Playback must be driven through this controller while it is in use. Replacing
         * the mesh restarts the machine and retains parameters if the new mesh is compatible.
         *
         * @param dt Finite nonnegative elapsed time in seconds.
         * @return False for invalid input, an incompatible mesh, or a failed pose update.
         */
        bool Update(float dt);

        /**
         * @brief Restore defaults and start the initial state on the next update.
         */
        void Reset();

        /**
         * @brief Get the target state name, or an empty view before the first update.
         */
        std::string_view GetCurrentState() const;

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
         * @param state Valid state index in the shared controller.
         * @param fadeDuration Fade duration in seconds.
         */
        bool PlayState(size_t state, float fadeDuration);

        Ref<const AnimationController> m_Controller;
        SkeletalMeshInstance& m_Instance;

        /** Last mesh checked for skeleton compatibility and initialized for playback. */
        Ref<const SkeletalMesh> m_Mesh;

        std::vector<TAnimationParameterValue> m_Parameters;
        std::optional<size_t> m_State;
    };
}
