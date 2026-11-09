#include "epch.h"
#include "ConstraintData.h"

namespace Elixir::Aether::Simulation
{
    std::optional<SConstraintFrameData> BuildConstraintFrameData(
        const SDistanceConstraint& config,
        const glm::mat4& world
    )
    {
        if (!config.IsValid())
            return std::nullopt;
        for (uint32_t column = 0; column < 4; ++column)
            for (uint32_t row = 0; row < 4; ++row)
                if (!std::isfinite(world[column][row]))
                    return std::nullopt;
        const float determinant = glm::determinant(world);
        if (!std::isfinite(determinant) || std::abs(determinant) < 0.000001f)
            return std::nullopt;
        SConstraintFrameData result;
        result.World = world;
        result.Simulation = glm::inverse(world);
        for (uint32_t column = 0; column < 4; ++column)
            for (uint32_t row = 0; row < 4; ++row)
                if (!std::isfinite(result.Simulation[column][row])) return std::nullopt;
        const auto& growth = config.Growth;
        const auto& surface = config.SurfaceAttachment;
        result.Topology = {
            config.HasGrowth,
            growth.BranchCount,
            growth.BranchInterval,
            uint32_t(growth.State)
        };
        result.Counts = {
            growth.Seed,
            surface.Count,
            config.EndAttachment.Enabled,
            uint32_t(config.EndAttachment.Space)
        };
        result.Growth =
            {growth.Speed, growth.MaxLength, growth.BranchLengthScale, growth.BranchVariation};
        result.EndPosition = glm::vec4(config.EndAttachment.Position, 0);
        result.Surface = {
            surface.SearchRadius,
            surface.Offset,
            surface.ClimbStrength,
            surface.BreakStretchRatio
        };
        result.ClimbDelay = glm::vec4(surface.ClimbDirection, growth.BranchDelay);
        for (uint32_t index = 0; index < surface.Count; ++index)
        {
            const auto& shape = surface.Surfaces[index];
            auto& packed = result.Surfaces[index];
            packed.CenterType = glm::vec4(shape.Center, float(shape.Type));
            packed.NormalRadius = glm::vec4(shape.Type == Core::EStrandSurfaceType::Plane
                    ? glm::normalize(shape.Normal) : glm::vec3(0, 0, 1),
                shape.Radius);
            packed.ExtentsRounding = glm::vec4(shape.HalfExtents, shape.Rounding);
        }
        return result;
    }
}
