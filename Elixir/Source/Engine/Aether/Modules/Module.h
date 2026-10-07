#pragma once

#include <Engine/Core/Core.h>
#include <Engine/Aether/Core/ParameterStore.h>
#include <Engine/Aether/Core/Particle.h>
#include <Engine/Aether/Core/Operation.h>
#include <Engine/Aether/Core/Operand.h>
#include <Engine/Aether/Core/DynamicInput.h>

namespace Elixir::Aether::Modules
{
    using namespace Elixir::Aether::Core;

    /** @brief Identifies the point in the particle lifetime at which a module runs. */
    enum class EModulePhase : uint8_t
    {
        Spawn, Update
    };

    /**
     * @brief Gives a module the data it needs to compile GPU operations.
     *
     * The context resolves emitter-local values before system-level values. It
     * appends operations to the compiled system and does not own its inputs.
     * Failed operand compilation logs an error and returns an invalid operand
     * that Emit rejects.
     */
    class ELIXIR_API ModuleCompileContext final
    {
    public:
        /**
         * @brief Creates a context for one emitter compilation.
         * @param operations Destination for instructions, references, and constants.
         * @param parameters Compiled parameters available to the emitter.
         * @param emitterName Name used to resolve emitter-local parameters.
         * @param gravityScale Multiplier configured by the emitter.
         */
        ModuleCompileContext(
            SCompiledOperations& operations,
            const std::vector<SParameter>& parameters,
            std::string_view emitterName,
            float gravityScale
        );

        /**
         * @brief Finds a parameter available to this emitter.
         *
         * The lookup first checks the emitter-local name, then the system name.
         *
         * @param name Authored parameter name.
         * @return The compiled parameter index, or UINT32_MAX when not found.
         */
        uint32_t FindParameter(std::string_view name) const;

        /**
         * @brief Finds the first compiled parameter for a curve.
         *
         * The lookup first checks the emitter-local curve, then the system curve.
         *
         * @param name Authored curve name.
         * @return The compiled parameter index, or UINT32_MAX when not found.
         */
        uint32_t FindCurve(std::string_view name) const;

        /**
         * @brief Returns the gravity multiplier configured by the emitter.
         * @return The gravity multiplier.
         */
        float GetGravityScale() const { return m_GravityScale; }

        /**
         * @brief Appends one operation to the compiled system.
         * The operand count and types must match the operation definition.
         * Invalid arguments are logged and leave the operation and operand tables unchanged.
         */
        void Emit(EOperationType type, EParticleAttribute target, std::initializer_list<SOperand> operands);

        /** @brief Compiles a scalar literal or optional named binding. */
        SOperand Scalar(float value, std::string_view parameter = "");

        /** @brief Compiles a three-component literal or optional named binding. */
        SOperand Vector(glm::vec3 value, std::string_view parameter = "");

        /** @brief Compiles a four-component literal or optional named binding. */
        SOperand Value(glm::vec4 value, std::string_view parameter = "");

        /** @brief Compiles an unsigned integer without converting it to float. */
        SOperand UInt(uint32_t value);

        /** @brief Compiles a particle input, or the constant one for None. */
        SOperand Dynamic(EDynamicInput input);

        /** @brief Compiles an attribute read with the supplied logical type. */
        static SOperand Attribute(EParticleAttribute attribute, EOperandType type);

        /** @brief Compiles a curve reference, preserving missing-curve defaults. */
        SOperand Curve(std::string_view name);

    private:
        // Interns typed literal words or resolves a named parameter binding.
        SOperand Literal(
            EOperandType type,
            std::span<const uint32_t> words,
            std::string_view parameter
        );

        SCompiledOperations& m_Operations;
        const std::vector<SParameter>& m_Parameters;
        std::string_view m_EmitterName;
        float m_GravityScale;

        // Includes the logical type in each key to preserve literal identity.
        std::map<std::array<uint32_t, 5>, uint32_t> m_Literals;
    };

    /** @brief Base class for every authored particle module. */
    class ELIXIR_API Module
    {
    public:
        explicit Module(EModulePhase phase = EModulePhase::Spawn);
        virtual ~Module();

        /** @brief Appends this module's GPU operations to the supplied context. */
        virtual void Compile(ModuleCompileContext& context) const = 0;

        /** @brief Returns the phase in which this module is compiled. */
        EModulePhase GetPhase() const { return m_Phase; }

    private:
        EModulePhase m_Phase;
    };

    /** @brief Base class for modules that run when a particle is created. */
    class ELIXIR_API SpawnModule : public Module
    {
    protected:
        SpawnModule() : Module(EModulePhase::Spawn) {}
    };

    /** @brief Base class for modules that run while a particle is updated. */
    class ELIXIR_API UpdateModule : public Module
    {
    protected:
        UpdateModule() : Module(EModulePhase::Update) {}
    };
}
