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

namespace Elixir::Aether::Core
{
    /** @brief Returns all operation definitions registered by the runtime. */
    inline std::span<const SOperationDefinition* const> GetOperationDefinitions()
    {
        static const std::array definitions{
            &Operations::SetLiteral::GetDefinition(),
            &Operations::RandomRange::GetDefinition(),
            &Operations::SampleDisk::GetDefinition(),
            &Operations::SampleCone::GetDefinition(),
            &Operations::SampleBox::GetDefinition(),
            &Operations::AddWithDelta::GetDefinition(),
            &Operations::Dampen::GetDefinition(),
            &Operations::LerpOverLife::GetDefinition(),
            &Operations::KillOutsideBounds::GetDefinition(),
            &Operations::AddFromAttribute::GetDefinition(),
            &Operations::SetPositionOnCircle::GetDefinition(),
            &Operations::SetPositionCircularPath::GetDefinition(),
            &Operations::SetPositionVortexRibbonPath::GetDefinition(),
            &Operations::SetRibbonIdFromSpawnOrder::GetDefinition(),
            &Operations::SampleCurve::GetDefinition(),
            &Operations::SampleColorCurve::GetDefinition(),
            &Operations::Add::GetDefinition(),
            &Operations::Mul::GetDefinition(),
            &Operations::Clamp::GetDefinition(),
            &Operations::CopyFromAttribute::GetDefinition(),
            &Operations::ApplyVortex::GetDefinition()
        };

        return definitions;
    }

    /** @brief Returns the definition, or nullptr when the operation is unknown. */
    inline const SOperationDefinition* FindOperationDefinition(const EOperationType type)
    {
        for (const auto* definition : GetOperationDefinitions())
            if (definition->Type == type) return definition;

        return nullptr;
    }
}
