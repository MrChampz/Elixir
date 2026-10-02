#include "epch.h"
#include "Effect.h"

#include <limits>
#include <simdjson.h>
#include <magic_enum/magic_enum.hpp>

#include <Engine/Aether/Effect/MaterialDescription.h>
#include <Engine/Aether/Effect/MaterialFactory.h>
#include <Engine/Aether/Effect/ModuleRegistry.h>

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

                        store.SetFloat4(name, ParseFloatVec<4>(array));
                    }
                    else
                    {
                        Fail("Parameter '{}' must be a number or a Float4 array.", name);
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

                return desc;
            }

            void ParseEmitter(const Ref<System>& system, od::object& json)
            {
                if (Failed()) return;

                const std::string name = RequireString(json, "name");
                const auto renderMode = ParseRenderMode(json, "renderMode");
                const uint32_t maxParticles = RequireUInt(json, "maxParticles");
                const auto spawnRate = ParseScalar(json, "spawnRate");

                if (Failed()) return;

                auto& emitter = system->AddEmitter(name, maxParticles, spawnRate.Value);
                emitter.SetRenderMode(renderMode);

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
        od::parser parser;

        auto json = simdjson::padded_string::load(filepath.string());
        if (const auto error = json.error())
        {
            const auto message = simdjson::error_message(error);
            EE_CORE_ERROR("Failed to open effect asset '{}': {}", filepath.string(), message)
            return nullptr;
        }

        auto document = parser.iterate(json.value());
        if (const auto error = document.error())
        {
            const auto message = simdjson::error_message(error);
            EE_CORE_ERROR("Failed to parse effect asset '{}': {}", filepath.string(), message)
            return nullptr;
        }

        od::object root;
        if (document.get_object().get(root))
        {
            EE_CORE_ERROR("Effect asset root must be an object: {}", filepath.string())
            return nullptr;
        }

        EffectParser effectParser;
        auto system = effectParser.Parse(root);
        if (effectParser.Failed())
        {
            EE_CORE_ERROR("{}", effectParser.GetError());
            EE_CORE_ERROR("    in effect asset: {}", filepath.string());
        }
        return system;
    }
}
