#pragma once

#include <Engine/Aether/Core/DynamicInput.h>
#include <Engine/Aether/Core/CompiledParameter.h>
#include <Engine/Aether/Core/Particle.h>

namespace Elixir::Aether::Core
{
    /** @brief Identifies where an operand obtains its value. */
    enum class EOperandSource : uint8_t
    {
        /** @brief Reads shared literal words from the constant buffer. */
        Constant = 0,

        /** @brief Reads instance values or curve samples from the parameter buffer. */
        Parameter = 1,

        /** @brief Resolves a runtime input identified by EDynamicInput. */
        DynamicInput = 2,

        /** @brief Reads a particle attribute identified by EParticleAttribute. */
        ParticleAttribute = 3,

        /** @brief Stores the value directly in the reference's payload bits. */
        Immediate = 4,
    };

    /** @brief Identifies how an operand's payload is stored or resolved. */
    enum class EOperandFormat : uint8_t
    {
        /** @brief Stores one 32-bit floating-point component. */
        Scalar = 0,

        /** @brief Stores two floating-point components and pads vector reads with zero. */
        Vector2 = 1,

        /** @brief Stores three floating-point components and pads the fourth with zero. */
        Vector3 = 2,

        /** @brief Stores four floating-point components. */
        Vector4 = 3,

        /** @brief Stores an unsigned 32-bit integer without changing its bits. */
        UInt = 4,

        /** @brief References the first parameter word of a curve's sample table. */
        Curve = 5,
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

    /** @brief Number of low bits preceding the three-bit source field. */
    inline constexpr uint32_t OperandSourceShift = 29;

    /** @brief Number of low bits preceding the three-bit format field. */
    inline constexpr uint32_t OperandFormatShift = 26;

    /** @brief Masks the format bits after shifting the reference. */
    inline constexpr uint32_t OperandFormatMask = 0x7u;

    /** @brief Masks the 26-bit word offset or immediate payload. */
    inline constexpr uint32_t OperandIndexMask = 0x03ffffffu;

    /** @brief Returns the origin encoded in an operand reference. */
    constexpr EOperandSource GetOperandSource(const uint32_t reference)
    {
        return EOperandSource(reference >> OperandSourceShift);
    }

    /** @brief Returns the payload format encoded in an operand reference. */
    constexpr EOperandFormat GetOperandFormat(const uint32_t reference)
    {
        return EOperandFormat((reference >> OperandFormatShift) & OperandFormatMask);
    }

    /** @brief Packs origin, format, and index, or returns UINT32_MAX for an invalid field. */
    inline uint32_t EncodeOperandReference(
        const EOperandSource source,
        const EOperandFormat format,
        const uint32_t index
    )
    {
        if (source > EOperandSource::Immediate ||
            format > EOperandFormat::Curve ||
            index > OperandIndexMask)
        {
            EE_CORE_ERROR("Particle operand source, format, or 26-bit index is invalid.");
            return UINT32_MAX;
        }

        return (uint32_t(source) << OperandSourceShift)
            | (uint32_t(format) << OperandFormatShift) | index;
    }

    /** @brief Returns the numeric word count, or zero for a curve or unsupported format. */
    constexpr uint32_t GetOperandFormatWordCount(const EOperandFormat format)
    {
        switch (format)
        {
            case EOperandFormat::Scalar:
            case EOperandFormat::UInt:
                return 1;
            case EOperandFormat::Vector2:
                return 2;
            case EOperandFormat::Vector3:
                return 3;
            case EOperandFormat::Vector4:
                return 4;
            default:
                return 0;
        }
    }

    /** @brief Returns a concrete type's storage format, or an invalid format for Target. */
    constexpr EOperandFormat GetOperandTypeFormat(const EOperandType type)
    {
        switch (type)
        {
            case EOperandType::Scalar:  return EOperandFormat::Scalar;
            case EOperandType::Vector3: return EOperandFormat::Vector3;
            case EOperandType::Vector4: return EOperandFormat::Vector4;
            case EOperandType::UInt:    return EOperandFormat::UInt;
            case EOperandType::Curve:   return EOperandFormat::Curve;
            default: return EOperandFormat(UINT8_MAX);
        }
    }

    /** @brief Returns a parameter's stored format, or an invalid format for an unsupported type. */
    constexpr EOperandFormat GetParameterOperandFormat(const EParameterType type)
    {
        switch (type)
        {
            case EParameterType::Scalar:        return EOperandFormat::Scalar;
            case EParameterType::Vector2:       return EOperandFormat::Vector2;
            case EParameterType::Vector3:       return EOperandFormat::Vector3;
            case EParameterType::UInt:          return EOperandFormat::UInt;
            case EParameterType::Vector4:
            case EParameterType::Unspecified:   return EOperandFormat::Vector4;
            default: return EOperandFormat(UINT8_MAX);
        }
    }

    /** @brief Checks whether a declared parameter can supply a logical operand type. */
    constexpr bool IsParameterOperandType(
        const EParameterType declared,
        const EOperandType actual
    )
    {
        if (actual == EOperandType::UInt)
            return declared == EParameterType::UInt;

        if (declared == EParameterType::Unspecified)
            return actual == EOperandType::Scalar || actual == EOperandType::Vector3 ||
                actual == EOperandType::Vector4;

        if (declared == EParameterType::Scalar)
            return actual == EOperandType::Scalar;

        const bool vector = declared == EParameterType::Vector2 ||
            declared == EParameterType::Vector3 || declared == EParameterType::Vector4;

        return vector && (actual == EOperandType::Vector3 || actual == EOperandType::Vector4);
    }

    /** @brief Stores a typed operand while a module is being compiled. */
    struct SGPUOperand
    {
        /** @brief Logical value type used to validate the operation argument. */
        EOperandType Type;

        /** @brief Packed three-bit origin, three-bit format, and 26-bit source-relative payload. */
        uint32_t Reference;
    };

    /** @brief Provides read-only tables used to validate operand references. */
    struct SOperandValidationContext
    {
        /** @brief Packed 32-bit constant words available to the operand. */
        std::span<const uint32_t> Constants;

        /** @brief Compiled parameters available to the operand and curve references. */
        std::span<const SCompiledParameter> Parameters;
    };

    /** @brief Returns the logical operand type of a particle attribute. */
    inline EOperandType GetAttributeOperandType(const EParticleAttribute attribute)
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

    /** @brief Returns an attribute's stored format; temporary registers hold four components. */
    inline EOperandFormat GetAttributeOperandFormat(const EParticleAttribute attribute)
    {
        if (attribute >= EParticleAttribute::Temp0 && attribute <= EParticleAttribute::Temp3)
            return EOperandFormat::Vector4;
        return GetOperandTypeFormat(GetAttributeOperandType(attribute));
    }

    /** @brief Checks whether an operand's type and reference match its source and available tables. */
    inline bool IsValidOperand(
        const SGPUOperand& operand,
        const SOperandValidationContext& context
    )
    {
        using enum EOperandType;

        const auto source = GetOperandSource(operand.Reference);
        const auto format = GetOperandFormat(operand.Reference);
        const auto index = operand.Reference & OperandIndexMask;
        const auto actual = operand.Type;
        const auto valueWidth = GetOperandFormatWordCount(format);

        if (source == EOperandSource::Parameter)
        {
            const bool curve = format == EOperandFormat::Curve;
            if (curve && actual != Curve) return false;
            if (curve && index == OperandIndexMask) return true;

            if (!curve && valueWidth == 0) return false;

            uint32_t offset = 0;
            for (const auto& definition : context.Parameters)
            {
                const auto width = GetParameterWordCount(definition.Type);
                if (width == 0 || definition.WordOffset != offset) return false;

                if (definition.WordOffset == index)
                {
                    if (curve)
                        return definition.Type == EParameterType::Unspecified && width == 4;

                    return valueWidth == width &&
                        format == GetParameterOperandFormat(definition.Type) &&
                        IsParameterOperandType(definition.Type, actual);
                }

                if (offset > OperandIndexMask - width)
                    return false;

                offset += width;
            }

            return false;
        }

        switch (source)
        {
            case EOperandSource::Constant:
            {
                if (valueWidth == 0 ||
                    std::size_t(index) + valueWidth > context.Constants.size())
                    return false;

                switch (format)
                {
                    case EOperandFormat::Scalar:
                        return actual == Scalar;
                    case EOperandFormat::Vector2:
                        return actual == Vector3 || actual == Vector4;
                    case EOperandFormat::Vector3:
                        return actual == Vector3;
                    case EOperandFormat::Vector4:
                        return actual == Vector4;
                    case EOperandFormat::UInt:
                        return actual == UInt;
                    default:
                        return false;
                }
            }

            case EOperandSource::DynamicInput:
                return format == EOperandFormat::Scalar &&
                    actual == Scalar && index <= uint32_t(EDynamicInput::ParticleSeed);

            case EOperandSource::ParticleAttribute:
            {
                const auto attr = EParticleAttribute(index);

                if (format != GetAttributeOperandFormat(attr))
                    return false;

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

            case EOperandSource::Immediate:
                return format == EOperandFormat::UInt && actual == UInt;

            default: return false;
        }

        return false;
    }
}
