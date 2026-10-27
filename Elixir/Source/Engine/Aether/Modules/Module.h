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

    /** @brief Selects which simulation receives an update module. */
    enum class EModuleTarget : uint8_t
    {
        Particle,
        StrandPoints
    };

    /** @brief Stores compiled instructions, references, and packed literal words. */
    struct SCompiledModuleData
    {
        std::vector<SGPUOperation> Operations;

        /** Four-byte source references addressed by each instruction. */
        std::vector<uint32_t> Operands;

        /** Literal bit patterns packed without float4 padding. */
        std::vector<uint32_t> Constants;

        /**
         * @brief Removes unused literals and relocates references.
         * @pre Operand references address valid ranges in the constant table.
         */
        void CompactConstants()
        {
            std::vector<uint32_t> packed;
            std::vector relocated(Constants.size(), UINT32_MAX);

            for (auto& reference : Operands)
            {
                const auto source = GetOperandSource(reference);
                if (source != EOperandSource::Constant) continue;

                const auto width = GetOperandFormatWordCount(GetOperandFormat(reference));
                if (width == 0) continue;

                const auto index = reference & OperandIndexMask;
                if (relocated[index] == UINT32_MAX)
                {
                    relocated[index] = uint32_t(packed.size());
                    packed.insert(packed.end(), Constants.begin() + index, Constants.begin() + index + width);
                }

                reference = (reference & ~OperandIndexMask) | relocated[index];
            }

            Constants.swap(packed);
        }

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
            SCompiledModuleData& operations,
            const std::vector<SCompiledParameter>& parameters,
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
         * @brief Append one operation to the compiled system.
         *
         * The operand count and types must match the operation definition. Invalid arguments
         * are logged and leave the operation and operand tables unchanged. Any failure marks
         * this context invalid and prevents subsequent operations from being appended.
         *
         * @param type Operation to emit.
         * @param target Destination particle attribute.
         * @param operands Typed operands in operation-definition order.
         */
        void Emit(
            EOperationType type,
            EParticleAttribute target,
            std::initializer_list<SGPUOperand> operands
        );

        /**
         * @brief Compile a scalar literal or named parameter binding.
         * @param value Literal used when no named parameter matches.
         * @param parameter Optional parameter name; empty selects the literal.
         * @return The operand, or an invalid operand that Emit rejects when compilation fails.
         */
        SGPUOperand Scalar(float value, std::string_view parameter = "");

        /**
         * @brief Compile a three-component literal or named parameter binding.
         * @param value Literal used when no named parameter matches.
         * @param parameter Optional parameter name; empty selects the literal.
         * @return The operand, or an invalid operand that Emit rejects when compilation fails.
         */
        SGPUOperand Vector(glm::vec3 value, std::string_view parameter = "");

        /**
         * @brief Compile a four-component literal or named parameter binding.
         * @param value Literal used when no named parameter matches.
         * @param parameter Optional parameter name; empty selects the literal.
         * @return The operand, or an invalid operand that Emit rejects when compilation fails.
         */
        SGPUOperand Value(glm::vec4 value, std::string_view parameter = "");

        /**
         * @brief Compile an unsigned integer literal or named parameter binding.
         *
         * Float parameters convert numerically and require an integer default in [0, 2^32).
         * Invalid runtime float values resolve to zero. UInt parameters retain all their bits.
         *
         * @param value Literal used when no named parameter matches.
         * @param parameter Optional parameter name; empty selects the literal.
         * @return The operand, or an invalid operand that Emit rejects when compilation fails.
         */
        SGPUOperand UInt(uint32_t value, std::string_view parameter = "");

        /**
         * @brief Compile a dynamic particle input.
         * @param input Runtime input; None selects the constant one.
         * @return A scalar operand to pass to Emit, which validates the input reference.
         */
        SGPUOperand Dynamic(EDynamicInput input);

        /**
         * @brief Compile an attribute read with a logical type.
         * @param attribute Particle attribute to read.
         * @param type Logical type expected by the operation.
         * @return An attribute operand to pass to Emit, which validates the reference and
         * type.
         */
        static SGPUOperand Attribute(EParticleAttribute attribute, EOperandInput type);

        /**
         * @brief Compile a curve reference with a missing-curve default.
         *
         * An absent curve uses the missing-curve sentinel so the shader can use its default.
         *
         * @param name Curve name resolved in emitter scope before system scope.
         * @return A curve operand, or an invalid operand when its offset exceeds the supported
         * range.
         */
        SGPUOperand Curve(std::string_view name);

        /** @brief Report whether all operand and operation compilation steps succeeded. */
        bool IsValid() const { return m_Valid; }

    private:
        /**
         * @brief Intern typed literal words or resolve a named parameter binding.
         * @param input Logical operand type.
         * @param words Raw literal words used when no parameter matches.
         * @param parameter Optional parameter name to resolve.
         * @return The operand, or an invalid operand for a type mismatch or range overflow.
         */
        SGPUOperand Literal(
            EOperandInput input,
            std::span<const uint32_t> words,
            std::string_view parameter
        );

        SCompiledModuleData& m_ModuleData;
        const std::vector<SCompiledParameter>& m_Parameters;
        std::string_view m_EmitterName;
        float m_GravityScale;

        bool m_Valid = true;

        // Includes the logical type in each key to preserve literal identity.
        std::map<std::array<uint32_t, 5>, uint32_t> m_Literals;
    };

    /** @brief Base class for every authored particle module. */
    class ELIXIR_API Module
    {
    public:
        /**
         * @brief Create a module for a particle lifetime phase.
         * @param phase Phase in which the module emits operations.
         */
        explicit Module(EModulePhase phase = EModulePhase::Spawn);

        /** @brief Release the module through its common interface. */
        virtual ~Module();

        /**
         * @brief Append this module's GPU operations to the supplied context.
         * @param context Compilation context that receives operations and resolves parameters.
         */
        virtual void Compile(ModuleCompileContext& context) const = 0;

        /** @brief Returns the phase in which this module is compiled. */
        EModulePhase GetPhase() const { return m_Phase; }

        /** @brief Returns the execution target; existing modules default to Particle. */
        EModuleTarget GetTarget() const { return m_Target; }

        /** @brief Selects an execution target; unsupported targets log an error and leave the selection unchanged. */
        bool SetTarget(EModuleTarget target);

        /** @brief Reports whether this module can run against the selected simulation. */
        virtual bool SupportsTarget(EModuleTarget target) const { return target == EModuleTarget::Particle; }

    private:
        EModulePhase m_Phase;
        EModuleTarget m_Target = EModuleTarget::Particle;
    };

    /** @brief Base class for modules that run when a particle is created. */
    class ELIXIR_API SpawnModule : public Module
    {
    protected:
        /** @brief Create a module that runs when a particle is spawned. */
        SpawnModule() : Module(EModulePhase::Spawn) {}
    };

    /** @brief Base class for modules that run while a particle is updated. */
    class ELIXIR_API UpdateModule : public Module
    {
    protected:
        /** @brief Create a module that runs when a particle is updated. */
        UpdateModule() : Module(EModulePhase::Update) {}
    };
}
