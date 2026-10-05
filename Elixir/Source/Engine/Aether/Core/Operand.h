#pragma once

#include <cstdint>

namespace Elixir::Aether::Core
{
    /** @brief Identifies the storage or runtime source addressed by an operand. */
    enum class EOperandSource : uint32_t
    {
        Constant            = 0,
        Parameter           = 1,
        DynamicInput        = 2,
        ParticleAttribute   = 3,
        ImmediateUInt       = 4,
        Curve               = 5,
        ConstantVector3     = 6,
        ConstantVector4     = 7,
    };

    /** @brief Identifies the logical type required by an operation argument. */
    enum class EOperandType : uint8_t
    {
        Scalar, Vector3, Vector4, UInt, Curve, Target
    };

    /** @brief Reserves three tag bits and 29 payload bits in each operand reference. */
    inline constexpr uint32_t OperandIndexMask = 0x1fffffffu;

    /** @brief Stores a typed operand while a module is being compiled. */
    struct SOperand
    {
        EOperandType Type;

        /** @brief Packed source tag and source-relative index. */
        uint32_t Reference;
    };
}
