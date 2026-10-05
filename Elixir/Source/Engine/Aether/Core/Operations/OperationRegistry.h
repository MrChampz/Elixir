#pragma once

#include "SetLiteral.h"
#include "RandomRange.h"
#include "SampleDisk.h"
#include "SampleCone.h"
#include "SampleBox.h"
#include "AddWithDelta.h"
#include "Dampen.h"
#include "LerpOverLife.h"
#include "KillOutsideBounds.h"
#include "AddFromAttribute.h"
#include "SetPositionOnCircle.h"
#include "SetPositionCircularPath.h"
#include "SetPositionVortexRibbonPath.h"
#include "SetRibbonIdFromSpawnOrder.h"
#include "SampleCurve.h"
#include "SampleColorCurve.h"
#include "Add.h"
#include "Mul.h"
#include "Clamp.h"
#include "CopyFromAttribute.h"
#include "ApplyVortex.h"

namespace Elixir::Aether::Core
{
    /** @brief Returns all operation definitions registered by the runtime. */
    inline std::span<const SOperationDefinition* const> GetOperationDefinitions()
    {
        static const std::array Definitions{
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
        return Definitions;
    }

    /** @brief Returns the definition, or nullptr when the operation is unknown. */
    inline const SOperationDefinition* FindOperationDefinition(EParticleOp type)
    {
        for (const auto* definition : GetOperationDefinitions())
            if (definition->Type == type) return definition;
        return nullptr;
    }
}
