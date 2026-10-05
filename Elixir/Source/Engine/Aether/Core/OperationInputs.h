#pragma once

// Each operation owns its argument positions; this header preserves the shared include.
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
