#pragma once

#include <Engine/Aether/Core/Particle.h>
#include <Engine/Aether/Core/Operand.h>

namespace Elixir::Aether::Core
{
    /** @brief Identifies the GPU operation executed by a particle instruction. */
    enum class EOperationType : uint32_t
    {
        SetLiteral = 0,
        RandomRange = 1,
        SampleDisk = 2,
        SampleCone = 3,
        SampleBox = 4,
        AddWithDelta = 5,
        Dampen = 6,
        LerpOverLife = 7,
        KillOutsideBounds = 8,
        AddFromAttribute = 9,
        SetPositionOnCircle = 10,
        SetPositionCircularPath = 11,
        SetPositionVortexRibbonPath = 12,
        SetRibbonIdFromSpawnOrder = 13,
        SampleCurve = 14,
        SampleColorCurve = 15,
        Add = 16,
        Mul = 17,
        Clamp = 18,
        CopyFromAttribute = 19,
        ApplyVortex = 20,
    };
    
    /** @brief Stores the fixed 16-byte header of a compiled particle instruction. */
    struct SOperation
    {
        EOperationType Type = EOperationType::SetLiteral;
        EParticleAttribute Target = EParticleAttribute::None;

        /** First word in the program's operand table. */
        uint32_t OperandOffset = 0;

        /** Number of arguments validated against the operation contract. */
        uint32_t OperandCount = 0;
    };

    /** @brief Stores compiled instructions, references, and packed literal words. */
    struct SCompiledOperations
    {
        std::vector<SOperation> Operations;

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
