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
        ApplyCurlNoise = 21,
    };
    
    /** @brief Stores the fixed 16-byte header of a compiled particle instruction. */
    struct SGPUOperation
    {
        EOperationType Type = EOperationType::SetLiteral;
        EParticleAttribute Target = EParticleAttribute::None;

        /** First word in the program's operand table. */
        uint32_t OperandOffset = 0;

        /** Number of arguments validated against the operation contract. */
        uint32_t OperandCount = 0;
    };

    /** @brief Describes the inputs and destination rules of a particle operation. */
    struct SOperationDefinition
    {
        EOperationType Type;

        /** Expected operand input type at each argument position. */
        std::span<const EOperandInput> Inputs;

        /** Indicates whether the spawn shader executes this operation. */
        bool SupportsSpawn;

        /** Indicates whether the update shader executes this operation. */
        bool SupportsUpdate;
    };

    /** @brief Defines the argument and destination rules of a runtime operation. */
    class Operation
    {
    public:
        /** @brief Destroys the operation through its common interface. */
        virtual ~Operation() = default;

        /** @brief Returns the operand input types and supported execution phases. */
        virtual const SOperationDefinition& GetDefinition() const = 0;

        /**
         * @brief Check whether this operation supports a destination attribute.
         * @param target Particle attribute that the operation would write.
         */
        virtual bool IsValidTarget(const EParticleAttribute target) const = 0;
    };
}
