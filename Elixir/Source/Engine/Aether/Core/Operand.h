#pragma once

#include <Engine/Aether/Core/DynamicInput.h>
#include <Engine/Aether/Core/CompiledParameter.h>
#include <Engine/Aether/Core/Particle.h>

#include <cmath>

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

    /** @brief Identifies how an operand's value is stored or encoded. */
    enum class EOperandFormat : uint8_t
    {
        /** @brief Stores one 32-bit floating-point component. */
        Float = 0,

        /** @brief Stores two floating-point components and pads vector reads with zero. */
        Float2 = 1,

        /** @brief Stores three floating-point components and pads the fourth with zero. */
        Float3 = 2,

        /** @brief Stores four floating-point components. */
        Float4 = 3,

        /** @brief Stores an unsigned 32-bit integer without changing its bits. */
        UInt = 4,

        /** @brief References the first parameter word of a curve's sample table. */
        Curve = 5,
    };

    /** @brief Identifies the operand input type expected by an operation argument. */
    enum class EOperandInput : uint8_t
    {
        /** @brief Requires a single floating-point component. */
        Float,

        /** @brief Requires three floating-point components. */
        Float3,

        /** @brief Requires four floating-point components. */
        Float4,

        /** @brief Requires an unsigned 32-bit integer. */
        UInt,

        /** @brief Requires a curve reference rather than a numeric value. */
        Curve,

        /** @brief Requires the type resolved from the operation's destination attribute. */
        Target
    };

    /** Number of low bits preceding the three-bit source field. */
    inline constexpr uint32_t OperandSourceShift = 29;

    /** Number of low bits preceding the three-bit format field. */
    inline constexpr uint32_t OperandFormatShift = 26;

    /** Masks the format bits after shifting the reference. */
    inline constexpr uint32_t OperandFormatMask = 0x7u;

    /** Masks the 26-bit word offset or immediate payload. */
    inline constexpr uint32_t OperandIndexMask = 0x03ffffffu;

    /**
     * @brief Get the origin encoded in an operand reference.
     * @param reference Packed operand reference.
     */
    constexpr EOperandSource GetOperandSource(const uint32_t reference)
    {
        return EOperandSource(reference >> OperandSourceShift);
    }

    /**
     * @brief Get the payload format encoded in an operand reference.
     * @param reference Packed operand reference.
     */
    constexpr EOperandFormat GetOperandFormat(const uint32_t reference)
    {
        return EOperandFormat((reference >> OperandFormatShift) & OperandFormatMask);
    }

    /**
     * @brief Pack an operand origin, format, and payload.
     * @param source Origin of the operand value.
     * @param format Storage format of the value.
     * @param index Source-relative word offset or immediate payload; at most OperandIndexMask.
     * @return Packed reference, or UINT32_MAX for an invalid field.
     */
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

    /**
     * @brief Get the numeric storage width of an operand.
     * @param format Operand storage format.
     * @return Number of words, or zero for a curve or unsupported format.
     */
    constexpr uint32_t GetOperandFormatWordCount(const EOperandFormat format)
    {
        switch (format)
        {
            case EOperandFormat::Float:
            case EOperandFormat::UInt:      return 1;
            case EOperandFormat::Float2:    return 2;
            case EOperandFormat::Float3:    return 3;
            case EOperandFormat::Float4:    return 4;
            default: return 0;
        }
    }

    /**
     * @brief Get the storage format for a concrete logical type.
     * @param type Logical operand type.
     * @return Storage format, or an invalid format for Target or an unsupported type.
     */
    constexpr EOperandFormat GetOperandTypeFormat(const EOperandInput type)
    {
        switch (type)
        {
            case EOperandInput::Float:  return EOperandFormat::Float;
            case EOperandInput::Float3: return EOperandFormat::Float3;
            case EOperandInput::Float4: return EOperandFormat::Float4;
            case EOperandInput::UInt:   return EOperandFormat::UInt;
            case EOperandInput::Curve:  return EOperandFormat::Curve;
            default: return EOperandFormat(UINT8_MAX);
        }
    }

    /**
     * @brief Get the stored format of a parameter.
     * @param type Declared parameter type.
     * @return Storage format, or an invalid format for an unsupported type.
     */
    constexpr EOperandFormat GetParameterOperandFormat(const EParameterType type)
    {
        switch (type)
        {
            case EParameterType::Float:         return EOperandFormat::Float;
            case EParameterType::Float2:        return EOperandFormat::Float2;
            case EParameterType::Float3:        return EOperandFormat::Float3;
            case EParameterType::UInt:          return EOperandFormat::UInt;
            case EParameterType::Float4:
            case EParameterType::Unspecified:   return EOperandFormat::Float4;
            default: return EOperandFormat(UINT8_MAX);
        }
    }

    /**
     * @brief Check whether a parameter can supply a logical operand type.
     * @param declared Declared parameter type.
     * @param actual Logical type required by the operand.
     */
    constexpr bool IsParameterOperandType(
        const EParameterType declared,
        const EOperandInput actual
    )
    {
        if (actual == EOperandInput::UInt)
            return declared == EParameterType::UInt || declared == EParameterType::Float;

        if (declared == EParameterType::Unspecified)
            return actual == EOperandInput::Float || actual == EOperandInput::Float3 ||
                actual == EOperandInput::Float4;

        if (declared == EParameterType::Float)
            return actual == EOperandInput::Float;

        const bool vector = declared == EParameterType::Float2 ||
            declared == EParameterType::Float3 || declared == EParameterType::Float4;

        return vector && (actual == EOperandInput::Float3 || actual == EOperandInput::Float4);
    }

    /**
     * @brief Checks whether a scalar can be converted to an unsigned integer exactly.
     * @param value Scalar to validate before conversion.
     * @return True for finite, nonnegative integers smaller than 2 to the power of 32.
     */
    inline bool IsValidUIntScalar(const float value)
    {
        return std::isfinite(value) && value >= 0 && value < 4294967296.0f &&
            std::floor(value) == value;
    }

    /** @brief Stores a typed operand while a module is being compiled. */
    struct SGPUOperand
    {
        /** Logical value type used to validate the operation argument. */
        EOperandInput Type;

        /** Packed three-bit origin, three-bit format, and 26-bit source-relative payload. */
        uint32_t Reference;
    };

    /** @brief Provides read-only tables used to validate operand references. */
    struct SOperandValidationContext
    {
        /** Packed 32-bit constant words available to the operand. */
        std::span<const uint32_t> Constants;

        /** Compiled parameters available to the operand and curve references. */
        std::span<const SCompiledParameter> Parameters;
    };

    /**
     * @brief Get the logical operand type of a particle attribute.
     * @param attribute Particle attribute to read.
     */
    inline EOperandInput GetAttributeOperandType(const EParticleAttribute attribute)
    {
        switch (attribute)
        {
            case EParticleAttribute::Position:
            case EParticleAttribute::Velocity:
            case EParticleAttribute::Tangent:
                return EOperandInput::Float3;

            case EParticleAttribute::RibbonId:
                return EOperandInput::UInt;

            case EParticleAttribute::Color:
            case EParticleAttribute::None:
                return EOperandInput::Float4;

            default:
                return EOperandInput::Float;
        }
    }

    /**
     * @brief Get an attribute storage format; temporary registers hold four components.
     * @param attribute Particle attribute to read.
     */
    inline EOperandFormat GetAttributeOperandFormat(const EParticleAttribute attribute)
    {
        if (attribute >= EParticleAttribute::Temp0 && attribute <= EParticleAttribute::Temp3)
            return EOperandFormat::Float4;
        return GetOperandTypeFormat(GetAttributeOperandType(attribute));
    }

    /**
     * @brief Check an operand type and reference against its source and tables.
     * @param operand Typed operand to validate.
     * @param context Constant and parameter tables addressed by the reference.
     * @return True when the reference is valid for the declared logical type.
     */
    inline bool IsValidOperand(
        const SGPUOperand& operand,
        const SOperandValidationContext& context
    )
    {
        using enum EOperandInput;

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
                        IsParameterOperandType(definition.Type, actual) &&
                        (actual != UInt || definition.Type != EParameterType::Float ||
                            IsValidUIntScalar(definition.Value.x));
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
                    case EOperandFormat::Float:
                        return actual == Float;
                    case EOperandFormat::Float2:
                        return actual == Float3 || actual == Float4;
                    case EOperandFormat::Float3:
                        return actual == Float3;
                    case EOperandFormat::Float4:
                        return actual == Float4;
                    case EOperandFormat::UInt:
                        return actual == UInt;
                    default:
                        return false;
                }
            }

            case EOperandSource::DynamicInput:
                return format == EOperandFormat::Float &&
                    actual == Float && index <= uint32_t(EDynamicInput::ParticleSeed);

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
                    (actual == Float || actual == Float3 || actual == Float4);

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
