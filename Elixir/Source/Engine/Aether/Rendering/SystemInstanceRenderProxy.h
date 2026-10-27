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
     * transform, and resolved parameter table. FrameSubmission stores the proxy,
     * so Renderer never reads mutable SystemInstance state.
     *
     * The proxy shares the immutable parameter table created by SystemInstance.
     * Each SCompiledParameter locates its native payload in that table.
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
         * @param parameterIndex Index in SCompiledSystem::Parameters.
         * @return The instance override when present; otherwise, the compiled
         * default value.
         * Unused vector components are zero; unsigned values retain their bits in x.
         *
         * @pre parameterIndex is less than SCompiledSystem::Parameters::size().
         * @warning An invalid index triggers an assertion and returns a zero vector
         * when assertions do not stop execution.
         */
        glm::vec4 GetParameterValue(const uint32_t parameterIndex) const
        {
            const auto& parameters = m_CompiledSystem->Parameters;

            EE_CORE_ASSERT(
                parameterIndex < parameters.size(),
                "Aether parameter index is outside the render proxy table."
            )
            if (parameterIndex >= parameters.size())
                return {};

            const auto& parameter = parameters[parameterIndex];
            const auto count = GetParameterWordCount(parameter.Type);

            EE_CORE_ASSERT(
                std::size_t(parameter.WordOffset) + count <= m_ParameterWords->size(),
                "Aether parameter range is outside the render proxy table."
            )
            if (count == 0 || count > 4 ||
                std::size_t(parameter.WordOffset) + count > m_ParameterWords->size())
                return {};

            glm::vec4 value{};
            std::memcpy(
                &value.x,
                m_ParameterWords->data() + parameter.WordOffset,
                count * sizeof(uint32_t)
            );

            return value;
        }

        /**
         * @brief Get the native parameter payload words.
         * @return A span that remains valid while this proxy is alive.
         */
        std::span<const uint32_t> GetParameterWords() const { return *m_ParameterWords; }

        /**
         * @brief Read an unsigned parameter without numeric conversion.
         * @param parameterIndex Index in SCompiledSystem::Parameters.
         * @return The value, or zero for an invalid index or a non-UInt parameter.
         */
        uint32_t GetParameterUInt(const uint32_t parameterIndex) const
        {
            if (parameterIndex >= m_CompiledSystem->Parameters.size() ||
                m_CompiledSystem->Parameters[parameterIndex].Type != EParameterType::UInt)
                return 0;

            return DecodeUIntParameter(GetParameterValue(parameterIndex));
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
        /**
         * @brief Store immutable state resolved by SystemInstance.
         * @param key Internal identity of the source instance.
         * @param revision Compiled-system selection revision.
         * @param parameterRevision Resolved parameter-value revision.
         * @param system Compiled system selected by the instance.
         * @param worldTransform World transform selected by the instance.
         * @param parameterWords Immutable parameter payload words addressed by the compiled
         * table.
         * @pre system and parameterWords are not null.
         */
        SystemInstanceRenderProxy(
            const SSystemInstanceKey& key,
            uint32_t revision,
            uint32_t parameterRevision,
            Ref<const SCompiledSystem> system,
            const glm::mat4& worldTransform,
            Ref<const ResolvedParameterWords> parameterWords
        );

        /** @brief Get the internal identity used by renderer records. */
        const SSystemInstanceKey& GetKey() const { return m_Key; }

        SSystemInstanceKey m_Key;
        uint32_t m_Revision = 1;
        uint32_t m_ParameterRevision = 1;
        Ref<const SCompiledSystem> m_CompiledSystem;
        glm::mat4 m_WorldTransform{ 1.0f };
        Ref<const ResolvedParameterWords> m_ParameterWords;
    };
}
