#pragma once

#include <cstdint>
#include <map>
#include <array>
#include <string>
#include <vector>
#include <Engine/Aether/Core/Particle.h>

namespace Elixir::Aether::Modules
{
    /** @brief Identifies the storage or runtime source addressed by an operand. */
    enum class EOperandSource : uint32_t
    {
#define AETHER_OPERAND_SOURCE(Name, Tag) Name = Tag,
#include <Engine/Aether/Core/ParticleOperandSources.def>
#undef AETHER_OPERAND_SOURCE
    };
    /** @brief Identifies the logical type required by an operation argument. */
    enum class EOperandType : uint32_t { Scalar, Vector3, Vector4, UInt, Curve, Target };
    /** @brief Reserves three tag bits and 29 payload bits in each operand reference. */
    inline constexpr uint32_t OperandIndexMask = 0x1fffffffu;

    /** @brief Stores the fixed 16-byte header of a compiled particle instruction. */
    struct SGPUParticleOp
    {
        Core::EParticleOp Type = Core::EParticleOp::SetLiteral;
        Core::EParticleAttribute Target = Core::EParticleAttribute::None;
        /** @brief First reference in the program's operand table. */
        uint32_t OperandOffset = 0;
        /** @brief Number of references, validated against the operation contract. */
        uint32_t OperandCount = 0;
    };
    static_assert(sizeof(SGPUParticleOp) == 16);

    /** @brief Stores a typed operand while a module is being compiled. */
    struct SModuleOperand
    {
        /** @brief Packed source tag and source-relative index. */
        uint32_t Reference;
        EOperandType Type;
    };

    /** @brief Stores compiled instructions, references, and packed literal words. */
    struct SParticleProgram
    {
        std::vector<SGPUParticleOp> Ops;
        /** @brief Four-byte references addressed by each instruction's operand range. */
        std::vector<uint32_t> Operands;
        /** @brief Literal bit patterns packed without float4 padding. */
        std::vector<uint32_t> Constants;
        /** @brief Releases the literal lookup index after compilation is complete. */
        void Finalize() { m_Literals.clear(); }
    private:
        friend class ModuleCompileContext;
        // Includes the logical type in each key to preserve literal identity.
        std::map<std::array<uint32_t, 5>, uint32_t> m_Literals;
    };

    namespace Inputs
    {
#define AETHER_OPERATION(Name, Opcode, ...)
#define AETHER_ARGUMENT(Op, Name, Index) /** @brief Position of the named operation argument. */ inline constexpr uint32_t Op##_##Name = Index;
#include <Engine/Aether/Core/ParticleOperations.def>
#undef AETHER_ARGUMENT
#undef AETHER_OPERATION
    }
}
