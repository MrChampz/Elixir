#pragma once

#include <Engine/Aether/Core/DynamicInput.h>
#include <Engine/Aether/Core/ParameterStore.h>
#include <Engine/Aether/Core/Particle.h>

namespace Elixir::Aether::Core
{
    /** @brief Identifies the storage or runtime source addressed by an operand. */
    enum class EOperandSource : uint32_t
    {
        /** @brief Reads one 32-bit word from the constant table. */
        Constant            = 0,

        /** @brief Reads a value from the system instance's parameter table. */
        Parameter           = 1,

        /** @brief Resolves a runtime input identified by EDynamicInput. */
        DynamicInput        = 2,

        /** @brief Reads a particle attribute identified by EParticleAttribute. */
        ParticleAttribute   = 3,

        /** @brief Stores an unsigned integer directly in the reference's 29 payload bits. */
        ImmediateUInt       = 4,

        /** @brief References a curve through its compiled parameter index. */
        Curve               = 5,

        /** @brief Reads three consecutive 32-bit words from the constant table. */
        ConstantVector3     = 6,

        /** @brief Reads four consecutive 32-bit words from the constant table. */
        ConstantVector4     = 7,
    };

    /** @brief Identifies the logical type required by an operation argument. */
    enum class EOperandType : uint8_t
    {
        /** @brief Requires a single floating-point component. */
        Scalar,

        /** @brief Requires three floating-point components. */
        Vector3,

        /** @brief Requires four floating-point components. */
        Vector4,

        /** @brief Requires an unsigned 32-bit integer. */
        UInt,

        /** @brief Requires a curve reference rather than a numeric value. */
        Curve,

        /** @brief Requires the type resolved from the operation's destination attribute. */
        Target
    };

    /** @brief Reserves three tag bits and 29 payload bits in each operand reference. */
    inline constexpr uint32_t OperandIndexMask = 0x1fffffffu;

    /** @brief Stores a typed operand while a module is being compiled. */
    struct SOperand
    {
        /** @brief Logical value type used to validate the operation argument. */
        EOperandType Type;

        /** @brief Packed source tag and source-relative index. */
        uint32_t Reference;
    };

    /** @brief Provides read-only tables used to validate operand references. */
    struct SOperandValidationContext
    {
        /** @brief Packed 32-bit constant words available to the operand. */
        std::span<const uint32_t> Constants;

        /** @brief Compiled parameters available to the operand and curve references. */
        std::span<const SParameter> Parameters;
    };

    /** @brief Returns the logical operand type of a particle attribute. */
    inline EOperandType GetAttributeOperandType(EParticleAttribute attribute)
    {
        switch (attribute)
        {
            case EParticleAttribute::Position:
            case EParticleAttribute::Velocity:
            case EParticleAttribute::Tangent:
                return EOperandType::Vector3;

            case EParticleAttribute::RibbonId:
                return EOperandType::UInt;

            case EParticleAttribute::Color:
            case EParticleAttribute::None:
                return EOperandType::Vector4;

            default:
                return EOperandType::Scalar;
        }
    }

    /** @brief Checks whether an operand's type and reference match its source and available tables. */
    inline bool IsValidOperand(
        const SOperand& operand,
        const SOperandValidationContext& context
    )
    {
        using enum EOperandType;

        const auto source = EOperandSource(operand.Reference >> 29);
        const auto index = operand.Reference & OperandIndexMask;
        const auto actual = operand.Type;

        switch (source)
        {
            case EOperandSource::Constant:
                return (actual == Scalar || actual == UInt) &&
                    index < context.Constants.size();

            case EOperandSource::ConstantVector3:
                return actual == Vector3 &&
                    std::size_t(index) + 3 <= context.Constants.size();

            case EOperandSource::ConstantVector4:
                return actual == Vector4 &&
                    std::size_t(index) + 4 <= context.Constants.size();

            case EOperandSource::Parameter:
            {
                const auto invalid = actual != Scalar && actual != Vector3 && actual != Vector4;
                if (invalid || index >= context.Parameters.size())
                    return false;

                const auto declared = context.Parameters[index].Type;
                if (declared == EParameterType::Unspecified)
                    return true;

                if (declared == EParameterType::Scalar)
                    return actual == Scalar;

                if (declared == EParameterType::Vector)
                    return actual == Vector3 || actual == Vector4;

                return false;
            }

            case EOperandSource::DynamicInput:
                return actual == Scalar && index <= uint32_t(EDynamicInput::ParticleSeed);

            case EOperandSource::ParticleAttribute:
            {
                const auto attr = EParticleAttribute(index);
                const auto sameType = actual == GetAttributeOperandType(attr);

                if (index <= uint32_t(EParticleAttribute::None) ||
                    index > uint32_t(EParticleAttribute::Temp3))
                    return false;

                const auto validTempRegister = index >= uint32_t(EParticleAttribute::Temp0) &&
                    (actual == Scalar || actual == Vector3 || actual == Vector4);

                if (sameType || validTempRegister)
                    return true;

                return false;
            }

            case EOperandSource::ImmediateUInt:
                return actual == UInt;

            case EOperandSource::Curve:
                return actual == Curve &&
                    (index == OperandIndexMask || index < context.Parameters.size());
        }

        return false;
    }
}
