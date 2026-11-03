#pragma once

#include <Engine/Aether/Core/Operations/SetLiteral.h>
#include <Engine/Aether/Core/Operations/RandomRange.h>
#include <Engine/Aether/Core/Operations/SampleDisk.h>
#include <Engine/Aether/Core/Operations/SampleCone.h>
#include <Engine/Aether/Core/Operations/SampleBox.h>
#include <Engine/Aether/Core/Operations/AddWithDelta.h>
#include <Engine/Aether/Core/Operations/Dampen.h>
#include <Engine/Aether/Core/Operations/LerpOverLife.h>
#include <Engine/Aether/Core/Operations/KillOutsideBounds.h>
#include <Engine/Aether/Core/Operations/AddFromAttribute.h>
#include <Engine/Aether/Core/Operations/SetPositionOnCircle.h>
#include <Engine/Aether/Core/Operations/SetPositionCircularPath.h>
#include <Engine/Aether/Core/Operations/SetPositionVortexRibbonPath.h>
#include <Engine/Aether/Core/Operations/SetRibbonIdFromSpawnOrder.h>
#include <Engine/Aether/Core/Operations/SampleCurve.h>
#include <Engine/Aether/Core/Operations/SampleColorCurve.h>
#include <Engine/Aether/Core/Operations/Add.h>
#include <Engine/Aether/Core/Operations/Mul.h>
#include <Engine/Aether/Core/Operations/Clamp.h>
#include <Engine/Aether/Core/Operations/CopyFromAttribute.h>
#include <Engine/Aether/Core/Operations/ApplyVortex.h>
#include <Engine/Aether/Core/Operations/ApplyCurlNoise.h>

namespace Elixir::Aether::Core
{
    /** @brief Returns the operation instances registered by the runtime. */
    inline std::span<const Operation* const> GetOperations()
    {
        using namespace Operations;

        static const SetLiteral setLiteral;
        static const RandomRange randomRange;
        static const SampleDisk sampleDisk;
        static const SampleCone sampleCone;
        static const SampleBox sampleBox;
        static const AddWithDelta addWithDelta;
        static const Dampen dampen;
        static const LerpOverLife lerpOverLife;
        static const KillOutsideBounds killOutsideBounds;
        static const AddFromAttribute addFromAttribute;
        static const SetPositionOnCircle setPositionOnCircle;
        static const SetPositionCircularPath setPositionCircularPath;
        static const SetPositionVortexRibbonPath setPositionVortexRibbonPath;
        static const SetRibbonIdFromSpawnOrder setRibbonIdFromSpawnOrder;
        static const SampleCurve sampleCurve;
        static const SampleColorCurve sampleColorCurve;
        static const Add add;
        static const Mul mul;
        static const Clamp clamp;
        static const CopyFromAttribute copyFromAttribute;
        static const ApplyVortex applyVortex;
        static const ApplyCurlNoise applyCurlNoise;

        static const Operation* const operations[]{
            &setLiteral,
            &randomRange,
            &sampleDisk,
            &sampleCone,
            &sampleBox,
            &addWithDelta,
            &dampen,
            &lerpOverLife,
            &killOutsideBounds,
            &addFromAttribute,
            &setPositionOnCircle,
            &setPositionCircularPath,
            &setPositionVortexRibbonPath,
            &setRibbonIdFromSpawnOrder,
            &sampleCurve,
            &sampleColorCurve,
            &add,
            &mul,
            &clamp,
            &copyFromAttribute,
            &applyVortex,
            &applyCurlNoise
        };

        return operations;
    }

    /** @brief Returns the operation, or nullptr when the type is unknown. */
    inline const Operation* FindOperation(const EOperationType type)
    {
        for (const auto* operation : GetOperations())
            if (operation->GetDefinition().Type == type) return operation;

        return nullptr;
    }
}
