#pragma once

#include <Engine/Aether/Core/Particle.h>
#include <Engine/Aether/Core/Operand.h>

namespace Elixir::Aether::Core
{
    /** @brief Stores the fixed 16-byte header of a compiled particle instruction. */
    struct SOperation
    {
        EParticleOp Type = EParticleOp::SetLiteral;
        EParticleAttribute Target = EParticleAttribute::None;

        /** First word in the program's operand table. */
        uint32_t OperandOffset = 0;

        /** Number of arguments validated against the operation contract. */
        uint32_t OperandCount = 0;
    };

    /** @brief Stores compiled instructions, references, and packed literal words. */
    struct SCompiledOperations
    {
        std::vector<SOperation> Ops;

        /** @brief Four-byte source references addressed by each instruction. */
        std::vector<uint32_t> Operands;

        /** @brief Literal bit patterns packed without float4 padding. */
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
                const auto source = EOperandSource(reference >> 29);

                const auto width = GetSourceWidth(source);
                if (width == 0) continue;

                const auto index = reference & OperandIndexMask;
                if (relocated[index] == UINT32_MAX)
                {
                    relocated[index] = uint32_t(packed.size());
                    packed.insert(packed.end(), Constants.begin() + index, Constants.begin() + index + width);
                }

                reference = (uint32_t(source) << 29) | relocated[index];
            }

            Constants.swap(packed);
        }

    private:
        // Returns the number of constant words addressed by a source.
        static uint32_t GetSourceWidth(const EOperandSource source)
        {
            if (source == EOperandSource::Constant)
                return 1u;

            if (source == EOperandSource::ConstantVector3)
                return 3u;

            if (source == EOperandSource::ConstantVector4)
                return 4u;

            return 0u;
        }
    };
}
