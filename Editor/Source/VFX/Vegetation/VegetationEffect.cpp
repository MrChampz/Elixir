#include <epch.h>
#include "VegetationEffect.h"
#include <Engine/Aether/Effect/Effect.h>
#include <Engine/Aether/Modules/DistanceConstraint.h>

namespace EditorVFX
{
    using namespace Elixir;
    using namespace Elixir::Aether::Effect;
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
            VegetationSettings& config
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
        /**
         * @brief Reads the sample's development settings.
         * @param context Parser reporting invalid fields.
         * @param object Serialized growth module.
         * @param output Destination settings.
         */
        void ParseGrowth(
            ModuleParseContext& context,
            simdjson::ondemand::object& object,
            SStrandGrowthSettings& output
        )
        {
            auto& settings = output;
            if (context.HasField(object, "speed"))
                settings.Speed = context.RequireFloat(object, "speed");
            if (context.HasField(object, "maxLength"))
                settings.MaxLength = context.RequireFloat(object, "maxLength");
            if (context.HasField(object, "branchCount"))
                settings.BranchCount = context.RequireUInt(object, "branchCount");
            if (context.HasField(object, "branchInterval"))
                settings.BranchInterval = context.RequireUInt(object, "branchInterval");
            if (context.HasField(object, "branchLengthScale"))
                settings.BranchLengthScale = context.RequireFloat(object, "branchLengthScale");
            if (context.HasField(object, "branchVariation"))
                settings.BranchVariation = context.RequireFloat(object, "branchVariation");
            if (context.HasField(object, "branchDelay"))
                settings.BranchDelay = context.RequireFloat(object, "branchDelay");
            if (context.HasField(object, "seed"))
                settings.Seed = context.RequireUInt(object, "seed");
            if (context.HasField(object, "state"))
            {
                const auto state = context.RequireString(object, "state");
                if (state == "Growing")
                    settings.State = EStrandGrowthState::Growing;
                else if (state == "Mature")
                    settings.State = EStrandGrowthState::Mature;
                else if (state == "Paused")
                    settings.State = EStrandGrowthState::Paused;
                else
                    context.Fail("Unknown strand growth state '{}'.", state);
            }
            if (!settings.IsValid())
                context.Fail("Invalid strand growth settings.");
        }

        /**
         * @brief Extracts sample simulation and rendering settings.
         * @param emitter Original editable emitter.
         * @param config Destination sample configuration.
         * @param error Parse error destination.
         * @return Whether the emitter has valid sample settings.
         */
        bool ReadVegetation(
            const Value& emitter,
            VegetationRenderConfig& config,
            std::string& error
        )
        {
            ModuleParseContext context;
            const auto modules = emitter.Find("updateModules");
            const Value* distance = nullptr;
            const Value* growth = nullptr;
            if (modules && modules->Type == EValueType::Array)
            {
                for (const auto& module : modules->Array)
                {
                    if (!module.Enabled())
                        continue;
                    if (module.String("type") == "DistanceConstraint")
                    {
                        if (distance)
                        {
                            error = "Vegetation requires one distance module.";
                            return false;
                        }
                        distance = &module;
                    }
                    if (module.String("type") == "StrandGrowth")
                    {
                        if (growth)
                        {
                            error = "Vegetation requires one growth module.";
                            return false;
                        }
                        growth = &module;
                    }
                }
            }
            if (!distance)
            {
                error = "Vegetation requires a distance module.";
                return false;
            }
            auto distanceText = simdjson::padded_string(distance->JSON());
            simdjson::ondemand::parser parser;
            simdjson::ondemand::object object;
            auto distanceDocument = parser.iterate(distanceText);
            if (distanceDocument.get_object().get(object))
            {
                error = "Invalid vegetation distance object.";
                return false;
            }
            const auto module = Aether::Modules::DistanceConstraint::Create(context, object);
            if (!module)
            {
                error = context.GetError();
                return false;
            }
            const auto& base =
                static_cast<Aether::Modules::DistanceConstraint&>(*module).GetConfig();
            auto& constraint = config.Constraint;
            constraint.MaxPoints = base.MaxPoints;
            constraint.OrderStep = base.OrderStep;
            constraint.PinFirstParticle = base.PinFirstParticle;
            constraint.SegmentLength = base.SegmentLength;
            constraint.Stiffness = base.Stiffness;
            ParseAttachments(context, object, constraint);
            if (growth)
            {
                auto growthText = simdjson::padded_string(growth->JSON());
                simdjson::ondemand::parser growthParser;
                simdjson::ondemand::object growthObject;
                auto growthDocument = growthParser.iterate(growthText);
                if (growthDocument.get_object().get(growthObject))
                {
                    error = "Invalid vegetation growth object.";
                    return false;
                }
                constraint.HasGrowth = true;
                ParseGrowth(context, growthObject, constraint.Growth);
            }
            if (const auto solver = emitter.Find("solver"))
            {
                auto text = simdjson::padded_string(solver->JSON());
                simdjson::ondemand::parser solverParser;
                simdjson::ondemand::object solverObject;
                auto solverDocument = solverParser.iterate(text);
                if (solverDocument.get_object().get(solverObject))
                {
                    error = "Invalid vegetation solver object.";
                    return false;
                }
                if (context.HasField(solverObject, "iterations"))
                    constraint.SolverIterations =
                        context.RequireUInt(solverObject, "iterations");
            }
            const auto strand = emitter.Find("strand");
            if (!strand)
            {
                error = "Vegetation requires tube geometry settings.";
                return false;
            }
            auto text = simdjson::padded_string(strand->JSON());
            simdjson::ondemand::parser renderParser;
            simdjson::ondemand::object render;
            auto renderDocument = renderParser.iterate(text);
            if (renderDocument.get_object().get(render))
            {
                error = "Invalid vegetation rendering object.";
                return false;
            }
            config.MaxPoints = constraint.MaxPoints;
            config.OrderStep = constraint.OrderStep;
            if (context.HasField(render, "maxPoints"))
                config.MaxPoints = context.RequireUInt(render, "maxPoints");
            if (context.HasField(render, "orderStep"))
                config.OrderStep = context.RequireUInt(render, "orderStep");
            if (context.HasField(render, "radialSegments"))
                config.RadialSegments = context.RequireUInt(render, "radialSegments");
            if (context.HasField(render, "subdivisions"))
                config.Subdivisions = context.RequireUInt(render, "subdivisions");
            if (context.HasField(render, "widthScale"))
                config.WidthScale = context.RequireFloat(render, "widthScale");
            if (context.HasField(render, "rootWidthScale"))
                config.RootWidthScale = context.RequireFloat(render, "rootWidthScale");
            if (context.HasField(render, "tipWidthScale"))
                config.TipWidthScale = context.RequireFloat(render, "tipWidthScale");
            if (context.HasField(render, "widthExponent"))
                config.WidthExponent = context.RequireFloat(render, "widthExponent");
            if (context.HasField(render, "branchWidthScale"))
                config.BranchWidthScale = context.RequireFloat(render, "branchWidthScale");
            if (context.Failed())
            {
                error = context.GetError();
                return false;
            }
            if (!config.IsValid())
            {
                error = "Invalid vegetation configuration.";
                return false;
            }
            return true;
        }
    }

    bool VegetationRenderConfig::IsValid() const
    {
        return Constraint.IsValid() && MaxPoints == Constraint.MaxPoints &&
               OrderStep == Constraint.OrderStep && std::isfinite(WidthScale) &&
               WidthScale > 0 && std::isfinite(RootWidthScale) && RootWidthScale >= 0 &&
               std::isfinite(TipWidthScale) && TipWidthScale >= 0 &&
               std::isfinite(WidthExponent) && WidthExponent > 0 &&
               std::isfinite(BranchWidthScale) && BranchWidthScale >= 0 &&
               RadialSegments >= 3 && RadialSegments <= 12 && Subdivisions >= 1 &&
               Subdivisions <= 8;
    }

    Ref<Aether::System> ParseVegetationEffect(
        const std::string_view source,
        std::string* error,
        Ref<VegetationPlayback> playback
    )
    {
        std::string localError;
        auto& diagnostic = error ? *error : localError;
        diagnostic.clear();
        Value root;
        if (!Value::Parse(std::string(source), root, diagnostic))
            return nullptr;
        const auto emitters = root.Find("emitters");
        if (!emitters || emitters->Type != EValueType::Array)
        {
            diagnostic = "An effect requires an emitters array.";
            return nullptr;
        }
        if (!playback)
            playback = CreateRef<VegetationPlayback>();
        std::unordered_map<std::string, Ref<VegetationRenderConfig>> configs;
        for (auto& emitter : root.Object["emitters"].Array)
        {
            if (!emitter.Enabled())
                continue;
            const auto modules = emitter.Find("updateModules");
            bool vegetation = emitter.Find("leafRenderer") || emitter.Find("balloonRenderer");
            if (modules && modules->Type == EValueType::Array)
                for (const auto& module : modules->Array)
                    if (module.Enabled() &&
                        (module.String("type") == "StrandGrowth" ||
                         module.Find("endAttachment") || module.Find("surfaceAttachment")))
                        vegetation = true;
            if (!vegetation)
                continue;
            if (emitter.String("renderMode") != "Strand")
            {
                diagnostic = "Editor vegetation requires a Strand emitter.";
                return nullptr;
            }
            const auto config = CreateRef<VegetationRenderConfig>();
            if (!ReadVegetation(emitter, *config, diagnostic))
                return nullptr;
            const auto name = emitter.String("name");
            if (!configs.emplace(name, config).second)
            {
                diagnostic = "Vegetation emitter names must be unique.";
                return nullptr;
            }
            auto& update = emitter.Object["updateModules"].Array;
            const auto sampleModule = [](const Value& module)
            {
                const auto type = module.String("type");
                return type == "StrandGrowth" || type == "DistanceConstraint";
            };
            std::erase_if(update, sampleModule);
            emitter.Object["renderMode"] = Value::FromString("None");
            emitter.Object.erase("strand");
        }
        const auto system = Aether::Effect::ParseEffect(root.JSON(), &diagnostic);
        if (!system)
            return nullptr;
        for (const auto& emitter : system->GetEmitters())
        {
            const auto found = configs.find(emitter->GetName());
            if (found == configs.end())
                continue;
            const auto& config = found->second;
            const auto processor =
                CreateRef<VegetationProcessor>(config->Constraint, playback);
            const uint64_t count = uint64_t(config->MaxPoints) * config->OrderStep;
            if (count != emitter->GetMaxParticles() ||
                emitter->ShouldKillOnLifetimeExpired() || emitter->GetBurstCount() != count ||
                emitter->GetBurstIntervalSeconds() != 0 ||
                !emitter->GetTriggerEmitterName().empty())
            {
                diagnostic = "Vegetation requires persistent, fully burst particle chains.";
                return nullptr;
            }
            emitter->SetParticleProcessor(processor);
            if (!emitter->SetRenderConfig(config))
                return nullptr;
        }
        return system;
    }
}
