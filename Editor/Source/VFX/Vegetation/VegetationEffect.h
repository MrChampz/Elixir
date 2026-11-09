#pragma once

#include "VegetationProcessor.h"
#include "../VFXDocument.h"
#include <Engine/Aether/Rendering/EmitterRenderConfig.h>
#include <Engine/Aether/System.h>

namespace EditorVFX
{
    /** Owns the sample's tube geometry and vegetation metadata outside the Engine API. */
    struct VegetationRenderConfig final : Elixir::Aether::Rendering::SEmitterRenderConfig
    {
        /** Application-owned solver and development settings. */
        VegetationSettings Constraint;
        /** Number of particle points in each sample chain. */
        uint32_t MaxPoints = 24;
        /** Particle-slot stride between connected sample points. */
        uint32_t OrderStep = 1;
        /** Number of sides around each tube. */
        uint32_t RadialSegments = 8;
        /** Spline sections generated for each particle connection. */
        uint32_t Subdivisions = 4;
        /** Multiplier applied to particle size to compute tube radius. */
        float WidthScale = 0.3f;
        /** Radius multiplier at the root. */
        float RootWidthScale = 1;
        /** Radius multiplier at the developed tip. */
        float TipWidthScale = 1;
        /** Positive exponent controlling the radius transition. */
        float WidthExponent = 1;
        /** Additional radius multiplier for branches. */
        float BranchWidthScale = 0.65f;
        /** @brief Checks the sample's geometry and simulation settings. */
        bool IsValid() const override;
        /** @brief Returns the vertex count for one tube segment. */
        uint32_t GetSegmentVertexCount() const { return Subdivisions * RadialSegments * 6; }
    };

    /**
     * @brief Extracts Editor vegetation data before delegating generic fields to Aether.
     * @param source Editable effect JSON.
     * @param error Optional error destination; cleared on success.
     * @param playback Playback control used by any vegetation emitters.
     * @return The effect, or null when its application or Engine fields are invalid.
     */
    Elixir::Ref<Elixir::Aether::System> ParseVegetationEffect(
        std::string_view source,
        std::string* error = nullptr,
        Elixir::Ref<VegetationPlayback> playback = {}
    );
}
