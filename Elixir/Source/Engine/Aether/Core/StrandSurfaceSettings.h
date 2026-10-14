#pragma once

#include <array>
#include <cstdint>
#include <cmath>
#include <glm/glm.hpp>

namespace Elixir::Aether::Core
{
    /** Defines a static analytic surface in world coordinates. */
    enum class EStrandSurfaceType : uint32_t { Plane, Sphere, Box };

    /** Describes one surface that growing strands can follow and attach to. */
    struct SStrandSurface
    {
        /** Analytic shape used for nearest-surface queries. */
        EStrandSurfaceType Type = EStrandSurfaceType::Sphere;
        /** World-space center; a plane passes through this point. */
        glm::vec3 Center{};
        /** Positive half sizes for an axis-aligned box, in world units. */
        glm::vec3 HalfExtents{0.2f};
        /** Positive sphere radius in world units. */
        float Radius = 0.2f;
        /** Nonzero plane normal; normalized when uploaded. */
        glm::vec3 Normal{0, 0, 1};
        /** Box corner radius, from zero through the smallest half extent. */
        float Rounding = 0;

        /** Checks shape dimensions and finite world coordinates. */
        bool IsValid() const
        {
            const auto finite = [](glm::vec3 value)
            { return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z); };
            if (!finite(Center)) return false;
            if (Type == EStrandSurfaceType::Plane) return finite(Normal) && std::isfinite(glm::dot(Normal, Normal)) && glm::dot(Normal, Normal) > 0.000001f;
            if (Type == EStrandSurfaceType::Sphere) return std::isfinite(Radius) && Radius > 0;
            if (Type != EStrandSurfaceType::Box || !finite(HalfExtents)) return false;
            return HalfExtents.x > 0 && HalfExtents.y > 0 && HalfExtents.z > 0 &&
                std::isfinite(Rounding) && Rounding >= 0 && Rounding <= HalfExtents.x &&
                Rounding <= HalfExtents.y && Rounding <= HalfExtents.z;
        }
    };

    /** Configures static world surfaces and growth guidance for one strand emitter. */
    struct SStrandSurfaceSettings
    {
        /** Maximum number of surfaces queried by one emitter. */
        static constexpr uint32_t Capacity = 8;
        /** Surface slots; only the first Count entries are used. */
        std::array<SStrandSurface, Capacity> Surfaces{};
        /** Active surface count; zero disables guidance and attachment. */
        uint32_t Count = 0;
        /** Maximum distance from a surface at which the tip starts following it, in world units. */
        float SearchRadius = 0.1f;
        /** Distance between attached points and the surface, in world units. */
        float Offset = 0.014f;
        /** Acceleration along the projected climb direction, in world units per second squared. */
        float ClimbStrength = 4;
        /** Preferred world-space growth direction; projected into the surface tangent plane. */
        glm::vec3 ClimbDirection{0, 1, 0};

        /** Segment stretch ratio that releases an attachment; zero disables release, otherwise greater than one. */
        float BreakStretchRatio = 0;

        /** Validates query limits, guidance, and all active shapes. */
        bool IsValid() const
        {
            if (!std::isfinite(BreakStretchRatio) || (BreakStretchRatio != 0 && BreakStretchRatio <= 1) || Count > Capacity || !std::isfinite(SearchRadius) || SearchRadius <= 0 ||
                !std::isfinite(Offset) || Offset < 0 || Offset >= SearchRadius ||
                !std::isfinite(ClimbStrength) || ClimbStrength < 0 ||
                !std::isfinite(ClimbDirection.x) || !std::isfinite(ClimbDirection.y) || !std::isfinite(ClimbDirection.z) ||
                !std::isfinite(glm::dot(ClimbDirection, ClimbDirection)) || glm::dot(ClimbDirection, ClimbDirection) <= 0.000001f) return false;
            for (uint32_t i = 0; i < Count; ++i) if (!Surfaces[i].IsValid()) return false;
            return true;
        }
    };
}
