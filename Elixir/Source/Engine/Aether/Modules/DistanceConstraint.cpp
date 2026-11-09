#include "epch.h"
#include "DistanceConstraint.h"

namespace Elixir::Aether::Modules
{
    namespace
    {
        /**
         * @brief Reads endpoint and analytic surface constraints.
         * @param context Parser reporting invalid fields.
         * @param object Serialized distance module.
         * @param config Destination constraint.
         */
        void ParseAttachments(
            ModuleParseContext& context,
            simdjson::ondemand::object& object,
            SDistanceConstraint& config
        )
        {
            if (context.HasField(object, "endAttachment"))
            {
                simdjson::ondemand::object end;
                if (object["endAttachment"].get_object().get(end))
                {
                    context.Fail("endAttachment must be an object.");
                    return;
                }
                auto& target = config.EndAttachment;
                target.Enabled = true;
                if (context.HasField(end, "enabled"))
                    target.Enabled = context.RequireBoolean(end, "enabled");
                if (context.HasField(end, "position"))
                    target.Position = glm::vec3(context.RequireFloatVec<3>(end, "position"));
                if (context.HasField(end, "space"))
                {
                    const auto space = context.RequireString(end, "space");
                    if (space == "World")
                        target.Space = EStrandEndSpace::World;
                    else if (space != "Simulation")
                        context.Fail("Unknown endpoint space '{}'.", space);
                }
            }
            if (!context.HasField(object, "surfaceAttachment"))
                return;
            simdjson::ondemand::object surface;
            if (object["surfaceAttachment"].get_object().get(surface))
            {
                context.Fail("surfaceAttachment must be an object.");
                return;
            }
            auto& settings = config.SurfaceAttachment;
            if (context.HasField(surface, "searchRadius"))
                settings.SearchRadius = context.RequireFloat(surface, "searchRadius");
            if (context.HasField(surface, "offset"))
                settings.Offset = context.RequireFloat(surface, "offset");
            if (context.HasField(surface, "climbStrength"))
                settings.ClimbStrength = context.RequireFloat(surface, "climbStrength");
            if (context.HasField(surface, "climbDirection"))
                settings.ClimbDirection =
                    glm::vec3(context.RequireFloatVec<3>(surface, "climbDirection"));
            if (context.HasField(surface, "breakStretchRatio"))
                settings.BreakStretchRatio =
                    context.RequireFloat(surface, "breakStretchRatio");
            simdjson::ondemand::array shapes;
            if (surface["surfaces"].get_array().get(shapes))
            {
                context.Fail("surfaceAttachment requires a surfaces array.");
                return;
            }
            for (auto value : shapes)
            {
                simdjson::ondemand::object shape;
                if (value.get_object().get(shape) || settings.Count == settings.Capacity)
                {
                    context.Fail("Surface attachment supports at most eight surface objects.");
                    return;
                }
                auto& entry = settings.Surfaces[settings.Count++];
                const auto type = context.RequireString(shape, "type");
                if (type == "Plane")
                    entry.Type = EStrandSurfaceType::Plane;
                else if (type == "Sphere")
                    entry.Type = EStrandSurfaceType::Sphere;
                else if (type == "Box")
                    entry.Type = EStrandSurfaceType::Box;
                else
                    context.Fail("Unknown surface type '{}'.", type);
                if (context.HasField(shape, "center"))
                    entry.Center = glm::vec3(context.RequireFloatVec<3>(shape, "center"));
                if (context.HasField(shape, "normal"))
                    entry.Normal = glm::vec3(context.RequireFloatVec<3>(shape, "normal"));
                if (context.HasField(shape, "halfExtents"))
                    entry.HalfExtents =
                        glm::vec3(context.RequireFloatVec<3>(shape, "halfExtents"));
                if (context.HasField(shape, "radius"))
                    entry.Radius = context.RequireFloat(shape, "radius");
                if (context.HasField(shape, "rounding"))
                    entry.Rounding = context.RequireFloat(shape, "rounding");
                if (context.Failed())
                    return;
            }
        }
    }

    Scope<Module> DistanceConstraint::Create(
        ModuleParseContext& context,
        simdjson::ondemand::object& object
    )
    {
        SDistanceConstraint config;
        if (ModuleParseContext::HasField(object, "maxPoints"))
            config.MaxPoints = context.RequireUInt(object, "maxPoints");
        if (ModuleParseContext::HasField(object, "orderStep"))
            config.OrderStep = context.RequireUInt(object, "orderStep");
        if (ModuleParseContext::HasField(object, "segmentLength"))
            config.SegmentLength = context.RequireFloat(object, "segmentLength");
        if (ModuleParseContext::HasField(object, "stiffness"))
            config.Stiffness = context.RequireFloat(object, "stiffness");
        if (ModuleParseContext::HasField(object, "pinFirstParticle"))
            config.PinFirstParticle = context.RequireBoolean(object, "pinFirstParticle");

        ParseAttachments(context, object, config);

        if (!config.IsValid())
            context.Fail("Invalid distance constraint configuration.");

        if (context.Failed())
            return nullptr;

        return CreateScope<DistanceConstraint>(config);
    }
}
