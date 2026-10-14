#include "epch.h"
#include "Effect.h"

#include <limits>
#include <simdjson.h>
#include <magic_enum/magic_enum.hpp>

#include <Engine/Aether/Effect/MaterialDescription.h>
#include <Engine/Aether/Effect/MaterialFactory.h>
#include <Engine/Aether/Effect/ModuleRegistry.h>
#include <Engine/Aether/Modules/ApplyCurlNoise.h>
#include <Engine/Aether/Modules/ApplyGravity.h>

namespace Elixir::Aether::Effect
{
    namespace
    {
        namespace od = simdjson::ondemand;

        // Parses an effect asset into a System. Every parsing step funnels
        // failures through Fail(), which records the first error; helpers
        // short-circuit once latched so a single root cause is reported and
        // no errored value is ever dereferenced.
        class EffectParser : public ModuleParseContext
        {
        public:
            Ref<System> Parse(od::object& root);

        private:
            // Reads the optional authoring toggle without hiding invalid values.
            bool IsEnabled(od::object& object)
            {
                if (!HasField(object, "enabled")) return true;
                bool enabled = true;
                if (object["enabled"].get_bool().get(enabled))
                    Fail("Field 'enabled' must be a boolean.");
                return enabled;
            }

            EParticleRenderMode ParseRenderMode(od::object& object, const std::string_view key)
            {
                constexpr auto result = EParticleRenderMode::Sprite;

                if (Failed() || !HasField(object, key))
                    return result;

                const std::string value = RequireString(object, key);
                if (Failed()) return result;

                if (const auto mode = magic_enum::enum_cast<EParticleRenderMode>(value))
                    return *mode;

                Fail("Unknown render mode '{}'.", value);
                return result;
            }

            // Reads particle death policy independently of authored lifetime values.
            void ParseParticleState(Emitter& emitter, od::object& json)
            {
                if (!HasField(json, "particleState")) return;
                od::object state;
                if (json["particleState"].get_object().get(state))
                {
                    Fail("'particleState' must be an object.");
                    return;
                }
                if (!HasField(state, "killOnLifetimeExpired")) return;
                bool enabled = true;
                if (state["killOnLifetimeExpired"].get_bool().get(enabled))
                {
                    Fail("'killOnLifetimeExpired' must be a boolean.");
                    return;
                }
                emitter.SetKillOnLifetimeExpired(enabled);
            }

            // Reads optional strand physics and tube settings.
            void ParseStrand(Emitter& emitter, od::object& json)
            {
                if (!HasField(json, "strand")) return;
                od::object object;
                if (json["strand"].get_object().get(object))
                {
                    Fail("'strand' must be an object.");
                    return;
                }
                SStrandSettings settings;
                if (HasField(object, "points")) settings.PointCount = RequireUInt(object, "points");
                if (HasField(object, "segmentLength")) settings.SegmentLength = RequireFloat(object, "segmentLength");
                if (HasField(object, "stiffness")) settings.Stiffness = RequireFloat(object, "stiffness");
                if (HasField(object, "damping")) settings.Damping = RequireFloat(object, "damping");
                // Translate earlier strand assets to the same explicit force modules.
                if ((HasField(object, "gravity") || HasField(object, "sway")) && emitter.GetRenderMode() != EParticleRenderMode::Strand)
                {
                    Fail("Strand forces require a Strand emitter.");
                    return;
                }
                if (HasField(object, "gravity"))
                {
                    auto force = CreateScope<ApplyGravity>(RequireFloatVec<3>(object, "gravity"));
                    force->SetTarget(EModuleTarget::StrandPoints);
                    emitter.AddModule(std::move(force));
                }
                if (HasField(object, "sway"))
                {
                    const float strength = RequireFloat(object, "sway");
                    if (strength < 0) { Fail("Strand sway must be nonnegative."); return; }
                    auto force = CreateScope<ApplyCurlNoise>(strength);
                    force->SetTarget(EModuleTarget::StrandPoints);
                    emitter.AddModule(std::move(force));
                }
                if (HasField(object, "widthScale")) settings.WidthScale = RequireFloat(object, "widthScale");
                if (HasField(object, "rootWidthScale")) settings.RootWidthScale = RequireFloat(object, "rootWidthScale");
                if (HasField(object, "tipWidthScale")) settings.TipWidthScale = RequireFloat(object, "tipWidthScale");
                if (HasField(object, "widthExponent")) settings.WidthExponent = RequireFloat(object, "widthExponent");
                if (HasField(object, "branchWidthScale")) settings.BranchWidthScale = RequireFloat(object, "branchWidthScale");
                if (HasField(object, "solverIterations")) settings.SolverIterations = RequireUInt(object, "solverIterations");
                if (HasField(object, "radialSegments")) settings.RadialSegments = RequireUInt(object, "radialSegments");
                if (HasField(object, "subdivisions")) settings.Subdivisions = RequireUInt(object, "subdivisions");
                if (HasField(object, "growthSpeed")) settings.GrowthSpeed = RequireFloat(object, "growthSpeed");
                if (HasField(object, "growthMaxLength")) settings.GrowthMaxLength = RequireFloat(object, "growthMaxLength");
                if (HasField(object, "branchCount")) settings.BranchCount = RequireUInt(object, "branchCount");
                if (HasField(object, "branchInterval")) settings.BranchInterval = RequireUInt(object, "branchInterval");
                if (HasField(object, "branchLengthScale")) settings.BranchLengthScale = RequireFloat(object, "branchLengthScale");
                if (HasField(object, "endAttachment"))
                {
                    od::object attachment;
                    if (object["endAttachment"].get_object().get(attachment))
                    { Fail("'endAttachment' must be an object."); return; }
                    auto& config = settings.EndAttachment;
                    config.Enabled = true;
                    if (HasField(attachment, "enabled") && attachment["enabled"].get_bool().get(config.Enabled))
                    { Fail("'endAttachment.enabled' must be a boolean."); return; }
                    if (HasField(attachment, "position")) config.Position = RequireFloatVec<3>(attachment, "position");
                    else if (config.Enabled) { Fail("Enabled strand end attachment requires a position."); return; }
                    if (HasField(attachment, "space"))
                    {
                        const auto space = RequireString(attachment, "space");
                        if (space == "Simulation") config.Space = EStrandEndSpace::Simulation;
                        else if (space == "World") config.Space = EStrandEndSpace::World;
                        else { Fail("Unknown strand endpoint space '{}'.", space); return; }
                    }
                }
                if (HasField(object, "surfaceAttachment"))
                {
                    od::object attachment;
                    if (object["surfaceAttachment"].get_object().get(attachment))
                    { Fail("'surfaceAttachment' must be an object."); return; }
                    auto& config = settings.SurfaceAttachment;
                    if (HasField(attachment, "searchRadius")) config.SearchRadius = RequireFloat(attachment, "searchRadius");
                    if (HasField(attachment, "offset")) config.Offset = RequireFloat(attachment, "offset");
                    if (HasField(attachment, "breakStretchRatio")) config.BreakStretchRatio = RequireFloat(attachment, "breakStretchRatio");
                    if (HasField(attachment, "climbStrength")) config.ClimbStrength = RequireFloat(attachment, "climbStrength");
                    if (HasField(attachment, "climbDirection")) config.ClimbDirection = RequireFloatVec<3>(attachment, "climbDirection");
                    od::array surfaces;
                    if (attachment["surfaces"].get_array().get(surfaces))
                    { Fail("'surfaceAttachment.surfaces' must be an array."); return; }
                    for (auto value : surfaces)
                    {
                        if (config.Count == SStrandSurfaceSettings::Capacity)
                        { Fail("A strand emitter supports at most eight surfaces."); return; }
                        od::object surface;
                        if (value.get_object().get(surface)) { Fail("A strand surface must be an object."); return; }
                        auto& shape = config.Surfaces[config.Count++];
                        const auto type = RequireString(surface, "type");
                        shape.Center = RequireFloatVec<3>(surface, "center");
                        if (type == "Plane")
                        { shape.Type = EStrandSurfaceType::Plane; shape.Normal = RequireFloatVec<3>(surface, "normal"); }
                        else if (type == "Sphere")
                        { shape.Type = EStrandSurfaceType::Sphere; shape.Radius = RequireFloat(surface, "radius"); }
                        else if (type == "Box")
                        {
                            shape.Type = EStrandSurfaceType::Box;
                            shape.HalfExtents = RequireFloatVec<3>(surface, "halfExtents");
                            if (HasField(surface, "rounding")) shape.Rounding = RequireFloat(surface, "rounding");
                        }
                        else { Fail("Unknown strand surface type '{}'.", type); return; }
                        if (Failed()) return;
                    }
                }
                if (Failed()) return;
                if (!emitter.SetStrandSettings(settings)) Fail("Invalid strand settings.");
            }

            /* Parameter/curve blocks */

            void LoadParametersFromObject(ParameterStore& store, od::object params)
            {
                for (auto field : params)
                {
                    if (Failed()) return;

                    std::string_view key;
                    if (field.unescaped_key().get(key))
                    {
                        Fail("Invalid parameter name.");
                        return;
                    }

                    const std::string name{ key };
                    auto value = field.value();

                    od::json_type type;
                    if (value.type().get(type))
                    {
                        Fail("Parameter '{}' has an invalid type.", name);
                        return;
                    }

                    if (type == od::json_type::number)
                    {
                        double scalar;

                        if (value.get_double().get(scalar))
                        {
                            Fail("Parameter '{}' is not a valid number.", name);
                            return;
                        }

                        store.SetFloat(name, static_cast<float>(scalar));
                    }
                    else if (type == od::json_type::array)
                    {
                        od::array array;
                        if (value.get_array().get(array))
                        {
                            Fail("Parameter '{}' is not a valid array.", name);
                            return;
                        }

                        size_t count;

                        if (array.count_elements().get(count))
                        {
                            Fail("Parameter '{}' has an invalid array length.", name);
                            return;
                        }

                        if (count == 2)
                            store.SetFloat2(name, ParseFloatVec<2>(array));
                        else if (count == 3)
                            store.SetFloat3(name, ParseFloatVec<3>(array));
                        else if (count == 4)
                            store.SetFloat4(name, ParseFloatVec<4>(array));
                        else
                            Fail("Parameter '{}' requires two, three, or four vector components.", name);
                    }
                    else
                    {
                        Fail("Parameter '{}' must be a number or a vector array.", name);
                    }
                }
            }

            void LoadCurvesFromObject(CurveStore& store, od::object curves)
            {
                for (auto curve : curves)
                {
                    if (Failed()) return;

                    std::string_view key;
                    if (curve.unescaped_key().get(key))
                    {
                        Fail("Invalid curve name.");
                        return;
                    }

                    const std::string name{ key };

                    od::array array;
                    if (curve.value().get_array().get(array))
                    {
                        Fail("Curve '{}' must be an array.", name);
                        return;
                    }

                    std::vector<float> samples;
                    for (auto sample : array)
                    {
                        double value;

                        if (sample.get_double().get(value))
                        {
                            Fail("Curve '{}' sample must be numeric.", name);
                            return;
                        }

                        samples.push_back(static_cast<float>(value));
                    }

                    store.SetCurve(name, std::move(samples));
                }
            }

            void LoadColorCurvesFromObject(ColorCurveStore& store, od::object curves)
            {
                for (auto curve : curves)
                {
                    if (Failed()) return;

                    std::string_view key;
                    if (curve.unescaped_key().get(key))
                    {
                        Fail("Invalid curve name.");
                        return;
                    }

                    const std::string name{ key };

                    od::array array;
                    if (curve.value().get_array().get(array))
                    {
                        Fail("Curve '{}' must be an array.", name);
                        return;
                    }

                    std::vector<glm::vec4> samples;
                    for (auto sample : array)
                    {
                        od::array array;

                        if (sample.get_array().get(array))
                        {
                            Fail("Curve '{}' sample must be a Float4.", name);
                            return;
                        }

                        const auto value = ParseFloatVec<4>(array);
                        if (Failed()) return;

                        samples.push_back(value);
                    }

                    store.SetCurve(name, std::move(samples));
                }
            }

            void LoadParameters(od::object& parent, ParameterStore& store)
            {
                if (Failed()) return;

                auto field = parent["parameters"];
                if (field.error())
                    return;

                od::object params;
                if (field.get_object().get(params))
                {
                    Fail("'parameters' must be an object.");
                    return;
                }

                LoadParametersFromObject(store, params);
            }

            void LoadCurves(od::object& parent, CurveStore& store)
            {
                if (Failed()) return;

                auto field = parent["curves"];
                if (field.error())
                    return;

                od::object curves;
                if (field.get_object().get(curves))
                {
                    Fail("'curves' must be an object.");
                    return;
                }

                LoadCurvesFromObject(store, curves);
            }

            void LoadColorCurves(od::object& parent, ColorCurveStore& store)
            {
                if (Failed()) return;

                auto field = parent["colorCurves"];
                if (field.error())
                    return;

                od::object curves;
                if (field.get_object().get(curves))
                {
                    Fail("'colorCurves' must be an object.");
                    return;
                }

                LoadColorCurvesFromObject(store, curves);
            }

            // Creates a registered module and validates its emitter phase.
            void BuildModule(Emitter& emitter, od::object& json, const EModulePhase phase)
            {
                if (Failed()) return;

                const std::string type = RequireString(json, "type");
                if (Failed()) return;

                auto module = ModuleRegistry::Create(type, *this, json);
                if (Failed()) return;

                const auto kind = phase == EModulePhase::Spawn ? "spawn" : "update";
                if (!module)
                {
                    Fail("Unsupported {} module type '{}'.", kind, type);
                    return;
                }

                if (module->GetPhase() != phase)
                {
                    Fail("Module type '{}' cannot run during {}.", type, kind);
                    return;
                }

                if (module->GetTarget() == EModuleTarget::StrandPoints && emitter.GetRenderMode() != EParticleRenderMode::Strand)
                {
                    Fail("StrandPoints modules require a Strand emitter.");
                    return;
                }
                emitter.AddModule(std::move(module));
            }

            void LoadModules(
                od::object& parent,
                std::string_view key,
                Emitter& emitter,
                const bool spawn
            )
            {
                if (Failed()) return;

                auto field = parent[key];
                if (field.error())
                    return;

                od::array array;
                if (field.get_array().get(array))
                {
                    Fail("'{}' must be an array.", key);
                    return;
                }

                for (auto element : array)
                {
                    if (Failed()) return;

                    od::object module;
                    if (element.get_object().get(module))
                    {
                        Fail("'{}' entries must be objects.", key);
                        return;
                    }

                    if (IsEnabled(module))
                        BuildModule(emitter, module, spawn ? EModulePhase::Spawn : EModulePhase::Update);
                }
            }

            glm::vec4 ResolveMaterialColor(
                const Float4Field& field,
                const Emitter& emitter,
                const System& system
            ) const
            {
                if (field.Param.empty()) return field.Value;

                const auto systemValue = system.GetParameters().GetFloat4(
                    field.Param,
                    field.Value
                );

                return emitter.GetParameters().GetFloat4(field.Param, systemValue);
            }

            std::optional<SMaterialDescription> ParseMaterial(
                od::object& json,
                const Emitter& emitter,
                const System& system
            )
            {
                auto field = json["material"];
                if (field.error()) return std::nullopt;

                SMaterialDescription desc{};

                od::object material;
                if (field.get_object().get(material))
                {
                    Fail("'material' must be an object.");
                    return std::nullopt;
                }

                const auto color = ResolveMaterialColor(
                    ParseFloat4(material, "color", glm::vec4(1.0f)),
                    emitter,
                    system
                );

                const auto emissive = ResolveMaterialColor(
                    ParseFloat4(material, "emissive", glm::vec4(0.0f)),
                    emitter,
                    system
                );

                desc.BaseColor = glm::vec3(color);
                desc.Opacity = color.w;
                desc.Emissive = glm::vec3(emissive);
                desc.BaseColorTexturePath = ParseString(material, "texture");
                if (HasField(material, "blendMode"))
                {
                    const auto value = RequireString(material, "blendMode");
                    const auto blend = magic_enum::enum_cast<EMaterialBlendMode>(value);
                    if (!blend) { Fail("Unknown material blend mode '{}'.", value); return std::nullopt; }
                    desc.BlendMode = *blend;
                }

                return desc;
            }

            void ParseEmitter(const Ref<System>& system, od::object& json)
            {
                if (Failed()) return;
                if (!IsEnabled(json) || Failed()) return;

                const std::string name = RequireString(json, "name");
                const auto renderMode = ParseRenderMode(json, "renderMode");
                const uint32_t maxParticles = RequireUInt(json, "maxParticles");
                const auto spawnRate = ParseScalar(json, "spawnRate");

                if (Failed()) return;

                auto& emitter = system->AddEmitter(name, maxParticles, spawnRate.Value);
                emitter.SetRenderMode(renderMode);
                if (HasField(json, "simulationSpace"))
                {
                    const auto value = RequireString(json, "simulationSpace");
                    const auto space = magic_enum::enum_cast<EParticleSimulationSpace>(value);
                    if (!space) { Fail("Unknown simulation space '{}'.", value); return; }
                    emitter.SetSimulationSpace(*space);
                }
                ParseParticleState(emitter, json);
                if (Failed()) return;
                ParseStrand(emitter, json);
                if (Failed()) return;

                LoadParameters(json, emitter.GetParameters());
                if (Failed()) return;

                if (HasField(json, "burst"))
                {
                    od::object burst;
                    if (json["burst"].get_object().get(burst))
                    {
                        Fail("'burst' must be an object.");
                        return;
                    }

                    const auto burstCount = RequireUInt(burst, "count");
                    const auto burstInterval = RequireFloat(burst, "interval");
                    if (Failed()) return;

                    emitter.SetBurst(burstCount, burstInterval);
                }

                if (HasField(json, "trigger"))
                {
                    od::object trigger;
                    if (json["trigger"].get_object().get(trigger))
                    {
                        Fail("'trigger' must be an object.");
                        return;
                    }

                    const auto triggerSource = RequireString(trigger, "source");
                    const auto triggerDelay = RequireFloat(trigger, "delay");
                    if (Failed()) return;

                    emitter.SetTriggerEmitter(triggerSource, triggerDelay);
                }

                if (Failed()) return;

                if (const auto material = ParseMaterial(json, emitter, *system))
                    emitter.SetMaterialDescription(std::move(*material));

                if (!spawnRate.Param.empty())
                    emitter.SetSpawnRateParamName(spawnRate.Param);

                LoadParameters(json, emitter.GetParameters());
                LoadCurves(json, emitter.GetCurves());
                LoadColorCurves(json, emitter.GetColorCurves());
                LoadModules(json, "spawnModules", emitter, true);
                LoadModules(json, "updateModules", emitter, false);
            }

        };

        Ref<System> EffectParser::Parse(od::object& root)
        {
            const std::string name = RequireString(root, "name");
            if (Failed()) return nullptr;

            auto system = CreateRef<System>(name);

            LoadParameters(root, system->GetParameters());
            LoadCurves(root, system->GetCurves());
            LoadColorCurves(root, system->GetColorCurves());
            if (Failed()) return nullptr;

            auto emittersField = root["emitters"];
            od::array emitters;

            if (emittersField.error() || emittersField.get_array().get(emitters))
            {
                Fail("Effect asset must contain an 'emitters' array.");
                return nullptr;
            }

            for (auto element : emitters)
            {
                od::object emitter;

                if (element.get_object().get(emitter))
                {
                    Fail("'emitters' entries must be objects.");
                    return nullptr;
                }

                ParseEmitter(system, emitter);
                if (Failed()) return nullptr;
            }

            return system;
        }
    }

    Ref<System> LoadEffectFile(const std::filesystem::path& filepath)
    {
        auto json = simdjson::padded_string::load(filepath.string());
        if (const auto error = json.error())
        {
            const auto message = simdjson::error_message(error);
            EE_CORE_ERROR("Failed to open effect asset '{}': {}", filepath.string(), message)
            return nullptr;
        }

        return LoadEffectText(std::string_view(json.value().data(), json.value().size()));
    }

    Ref<System> LoadEffectText(const std::string_view source, std::string* error)
    {
        if (error) error->clear();
        od::parser parser;
        const simdjson::padded_string json(source);
        auto document = parser.iterate(json);
        if (const auto parseError = document.error())
        {
            const auto message = simdjson::error_message(parseError);
            EE_CORE_ERROR("Failed to parse effect data: {}", message)
            if (error) *error = message;
            return nullptr;
        }

        od::object root;
        if (document.get_object().get(root))
        {
            EE_CORE_ERROR("Effect asset root must be an object.")
            if (error) *error = "Effect asset root must be an object.";
            return nullptr;
        }

        EffectParser effectParser;
        auto system = effectParser.Parse(root);
        if (effectParser.Failed())
        {
            EE_CORE_ERROR("{}", effectParser.GetError());
            if (error) *error = effectParser.GetError();
        }
        return system;
    }
}
