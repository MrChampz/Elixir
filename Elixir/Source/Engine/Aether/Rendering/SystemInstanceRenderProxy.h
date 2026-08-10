#pragma once

#include <Engine/Aether/SystemInstance.h>

namespace Elixir::Aether::Simulation { class Simulator; }

namespace Elixir::Aether::Rendering
{
    using Aether::SystemInstanceSnapshot;
    using Simulation::Simulator;

    /**
     * @brief Provides immutable SystemInstance data to the particle renderer.
     *
     * SystemInstanceSnapshot creates this proxy from one compiled system, world
     * transform, and parameter-override set. FrameSubmission stores the proxy, so
     * Renderer never reads mutable SystemInstance state.
     *
     * The proxy resolves each compiled parameter to either its instance override or
     * its compiled default value. Its parameter table has the same order as
     * SCompiledSystem::Parameters.
     *
     * @thread_safety Immutable after construction.
     */
    class SystemInstanceRenderProxy
    {
        friend class SystemInstanceSnapshot;
        friend class FrameSubmission;
        friend class Simulator;

    public:
        /**
         * @brief Returns a resolved parameter value by compiled-table index.
         *
         * @param parameterIndex Index in SCompiledSystem::Parameters.
         * @return The instance override when present; otherwise, the compiled
         * default value.
         *
         * @pre parameterIndex is less than SCompiledSystem::Parameters::size().
         * @warning An invalid index triggers an assertion and returns a zero vector
         * when assertions do not stop execution.
         */
        glm::vec4 GetParameterValue(const uint32_t parameterIndex) const
        {
            EE_CORE_ASSERT(
                parameterIndex < m_ParameterValues.size(),
                "Aether parameter index is outside the render proxy table."
            )
            return parameterIndex < m_ParameterValues.size()
                ? m_ParameterValues[parameterIndex]
                : glm::vec4{};
        }

        /**
         * @brief Returns the compiled-system selection revision.
         * @return Revision incremented when the selected compiled system changes.
         */
        uint32_t GetRevision() const { return m_Revision; }

        /**
         * @brief Returns the resolved parameter-value revision.
         * @return Revision incremented when effective parameter values change.
         */
        uint32_t GetParameterRevision() const { return m_ParameterRevision; }

        /**
         * @brief Returns the immutable compiled system used by this proxy.
         * @return Compiled system selected by the source instance.
         */
        const SCompiledSystem& GetCompiledSystem() const { return *m_CompiledSystem; }

        /**
         * @brief Returns the world transform selected by the source instance.
         * @return Immutable world transform for this frame.
         */
        const glm::mat4& GetWorldTransform() const { return m_WorldTransform; }

    private:
        // Resolves compiled defaults and instance overrides into a dense parameter table.
        SystemInstanceRenderProxy(
            const SSystemInstanceKey& key,
            uint32_t revision,
            uint32_t parameterRevision,
            Ref<const SCompiledSystem> system,
            const glm::mat4& worldTransform,
            const ParameterOverridesMap& overrides
        );

        // Returns the internal identity used by Renderer instance records.
        const SSystemInstanceKey& GetKey() const { return m_Key; }

        SSystemInstanceKey m_Key;
        uint32_t m_Revision = 1;
        uint32_t m_ParameterRevision = 1;
        Ref<const SCompiledSystem> m_CompiledSystem;
        glm::mat4 m_WorldTransform{ 1.0f };
        std::vector<glm::vec4> m_ParameterValues;
    };
}
