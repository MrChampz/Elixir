#include "epch.h"
#include "Effect.h"

#include <magic_enum/magic_enum.hpp>
#include <simdjson.h>

namespace Elixir::Aether
{
    namespace
    {
        namespace od = simdjson::ondemand;

        // A field that is either a literal value or a reference to a named
        // parameter (written as "$paramName" in the asset).
        struct ScalarField
        {
            float Value = 0.0f;
            std::string Param;
        };

        struct Float4Field
        {
            glm::vec4 Value{};
            std::string Param;
        };

        // Resolves a "$name" reference to "name"; returns nullopt otherwise.
        std::optional<std::string> ResolveParamRef(const std::string_view value)
        {
            if (!value.empty() && value.front() == '$')
                return std::string{ value.substr(1) };

            return std::nullopt;
        }

        // Parses an effect asset into a System. Every parsing step funnels
        // failures through Fail(), which logs and latches m_Failed; helpers
        // short-circuit once latched so a single root cause is reported and no
        // errored simdjson value is ever dereferenced. LoadEffectFile returns
        // nullptr whenever m_Failed is set.
        class EffectParser
        {
          public:
            explicit EffectParser(std::filesystem::path filepath)
                : m_Filepath(std::move(filepath)) {}

            Ref<System> Parse(od::object& root);

            bool Failed() const { return m_Failed; }

          private:
            template <typename... Args>
            void Fail(const spdlog::format_string_t<Args...> fmt, Args&&... args)
            {
                if (m_Failed) return;

                EE_CORE_ERROR(fmt, std::forward<Args>(args)...)
                EE_CORE_ERROR("  in effect asset: {}", m_Filepath.string())
                m_Failed = true;
            }

            /* Scalar accessors */

            float RequireFloat(od::object& object, std::string_view key)
            {
                if (m_Failed) return 0.0f;

                auto field = object[key];
                double value;
                if (field.error() || field.get_double().get(value))
                {
                    Fail("Field '{}' must be a number.", key);
                    return 0.0f;
                }
                return static_cast<float>(value);
            }

            uint32_t RequireUInt(od::object& object, std::string_view key)
            {
                if (m_Failed) return 0;

                auto field = object[key];
                uint64_t value;
                if (field.error() || field.get_uint64().get(value))
                {
                    Fail("Field '{}' must be an unsigned integer.", key);
                    return 0;
                }
                return static_cast<uint32_t>(value);
            }

            std::string RequireString(od::object& object, std::string_view key)
            {
                if (m_Failed) return {};

                auto field = object[key];
                std::string_view value;
                if (field.error() || field.get_string().get(value))
                {
                    Fail("Field '{}' must be a string.", key);
                    return {};
                }
                return std::string{ value };
            }

            bool HasField(od::object& object, std::string_view key)
            {
                return !object[key].error();
            }

            /* Vector accessors */

            template <int N>
            glm::vec<N, float> ParseFloatVec(od::array array)
            {
                glm::vec<N, float> result{};
                if (m_Failed) return result;

                size_t count;
                if (array.count_elements().get(count) || count != N)
                {
                    Fail("Expected a float{} array.", N);
                    return result;
                }

                int i = 0;
                for (auto element : array)
                {
                    double value;
                    if (element.get_double().get(value))
                    {
                        Fail("Float array element must be numeric.");
                        return result;
                    }
                    result[i++] = static_cast<float>(value);
                }
                return result;
            }

            template <int N>
            glm::vec<N, float> RequireFloatVec(od::object& object, std::string_view key)
            {
                glm::vec<N, float> result{};
                if (m_Failed) return result;

                auto field = object[key];
                od::array array;
                if (field.error() || field.get_array().get(array))
                {
                    Fail("Field '{}' must be a float{} array.", key, N);
                    return result;
                }
                return ParseFloatVec<N>(array);
            }

            /* Parameter-aware fields */

            ScalarField ParseScalar(od::object& object, std::string_view key, float fallback = 0.0f)
            {
                ScalarField result{ fallback, {} };
                if (m_Failed) return result;

                auto field = object[key];
                if (field.error())
                    return result;

                od::json_type type;
                if (field.type().get(type))
                {
                    Fail("Field '{}' has an invalid type.", key);
                    return result;
                }

                if (type == od::json_type::number)
                {
                    double value;
                    if (field.get_double().get(value))
                    {
                        Fail("Field '{}' is not a valid number.", key);
                        return result;
                    }
                    result.Value = static_cast<float>(value);
                    return result;
                }

                if (type == od::json_type::string)
                {
                    std::string_view str;
                    if (!field.get_string().get(str))
                    {
                        if (auto param = ResolveParamRef(str))
                        {
                            result.Param = std::move(*param);
                            return result;
                        }
                    }
                }

                Fail("Field '{}' must be a number or a $parameter reference.", key);
                return result;
            }

            Float4Field ParseFloat4(od::object& object, std::string_view key, glm::vec4 fallback = glm::vec4(0.0f))
            {
                Float4Field result{ fallback, {} };
                if (m_Failed) return result;

                auto field = object[key];
                if (field.error())
                    return result;

                od::json_type type;
                if (field.type().get(type))
                {
                    Fail("Field '{}' has an invalid type.", key);
                    return result;
                }

                if (type == od::json_type::array)
                {
                    od::array array;
                    if (field.get_array().get(array))
                    {
                        Fail("Field '{}' is not a valid array.", key);
                        return result;
                    }
                    result.Value = ParseFloatVec<4>(array);
                    return result;
                }

                if (type == od::json_type::string)
                {
                    std::string_view str;
                    if (!field.get_string().get(str))
                    {
                        if (auto param = ResolveParamRef(str))
                        {
                            result.Param = std::move(*param);
                            return result;
                        }
                    }
                }

                Fail("Field '{}' must be a float4 array or a $parameter reference.", key);
                return result;
            }

            /* Enums */

            EParticleRenderMode ParseRenderMode(std::string_view value)
            {
                if (auto mode = magic_enum::enum_cast<EParticleRenderMode>(value))
                    return *mode;

                Fail("Unknown render mode '{}'.", value);
                return EParticleRenderMode::Sprite;
            }

            EDynamicInput ParseDynamicInput(od::object& object, std::string_view key, EDynamicInput fallback)
            {
                if (m_Failed || !HasField(object, key))
                    return fallback;

                const std::string value = RequireString(object, key);
                if (m_Failed)
                    return fallback;

                if (auto input = magic_enum::enum_cast<EDynamicInput>(value))
                    return *input;

                Fail("Unknown dynamic input '{}'.", value);
                return fallback;
            }

            /* Parameter / curve blocks */

            void LoadParameters(ParameterStore& store, od::object params)
            {
                for (auto field : params)
                {
                    if (m_Failed) return;

                    std::string_view keyView;
                    if (field.unescaped_key().get(keyView))
                    {
                        Fail("Invalid parameter name.");
                        return;
                    }
                    const std::string name{ keyView };

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
                        Fail("Parameter '{}' must be a number or a float4 array.", name);
                    }
                }
            }

            void LoadCurves(CurveStore& store, od::object curves)
            {
                for (auto curve : curves)
                {
                    if (m_Failed) return;

                    std::string_view keyView;
                    if (curve.unescaped_key().get(keyView))
                    {
                        Fail("Invalid curve name.");
                        return;
                    }
                    const std::string name{ keyView };

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

            void LoadOptionalParameters(od::object& parent, ParameterStore& store)
            {
                if (m_Failed) return;

                auto field = parent["parameters"];
                if (field.error())
                    return;

                od::object params;
                if (field.get_object().get(params))
                {
                    Fail("'parameters' must be an object.");
                    return;
                }
                LoadParameters(store, params);
            }

            void LoadOptionalCurves(od::object& parent, CurveStore& store)
            {
                if (m_Failed) return;

                auto field = parent["curves"];
                if (field.error())
                    return;

                od::object curves;
                if (field.get_object().get(curves))
                {
                    Fail("'curves' must be an object.");
                    return;
                }
                LoadCurves(store, curves);
            }

            /* Spawn module builders */

            void Spawn_SetPositionDisk(Emitter& emitter, od::object& json)
            {
                const glm::vec3 center = RequireFloatVec<3>(json, "center");
                const float radius = RequireFloat(json, "radius");

                if (HasField(json, "normal"))
                    emitter.AddSpawnModule<SetPositionDisk>(center, radius, RequireFloatVec<3>(json, "normal"));
                else
                    emitter.AddSpawnModule<SetPositionDisk>(center, radius);
            }

            void Spawn_SetVelocityCone(Emitter& emitter, od::object& json)
            {
                const glm::vec3 direction = HasField(json, "direction")
                    ? RequireFloatVec<3>(json, "direction")
                    : glm::vec3{ 0.0f, 1.0f, 0.0f };

                const auto angle = ParseScalar(json, "angle");
                const auto minSpeed = ParseScalar(json, "minSpeed");
                const auto maxSpeed = ParseScalar(json, "maxSpeed");

                emitter.AddSpawnModule<SetVelocityCone>(direction, angle.Value, minSpeed.Value, maxSpeed.Value)
                    .BindParameters(angle.Param, minSpeed.Param, maxSpeed.Param);
            }

            void Spawn_SetLifetime(Emitter& emitter, od::object& json)
            {
                const auto min = ParseScalar(json, "min");
                const auto max = ParseScalar(json, "max");
                emitter.AddSpawnModule<SetLifetime>(min.Value, max.Value)
                    .BindParameters(min.Param, max.Param);
            }

            void Spawn_SetSize(Emitter& emitter, od::object& json)
            {
                const auto min = ParseScalar(json, "min");
                const auto max = ParseScalar(json, "max");
                emitter.AddSpawnModule<SetSize>(min.Value, max.Value)
                    .BindParameters(min.Param, max.Param);
            }

            void Spawn_SetScale(Emitter& emitter, od::object& json)
            {
                const auto min = ParseScalar(json, "min");
                const auto max = ParseScalar(json, "max");
                emitter.AddSpawnModule<SetScale>(min.Value, max.Value)
                    .BindParameters(min.Param, max.Param);
            }

            void Spawn_SetRotation(Emitter& emitter, od::object& json)
            {
                const auto min = ParseScalar(json, "min");
                const auto max = ParseScalar(json, "max");
                emitter.AddSpawnModule<SetRotation>(min.Value, max.Value)
                    .BindParameters(min.Param, max.Param);
            }

            void Spawn_SetColor(Emitter& emitter, od::object& json)
            {
                const auto color = ParseFloat4(json, "color", glm::vec4(1.0f));
                emitter.AddSpawnModule<SetColor>(color.Value).BindParameter(color.Param);
            }

            void Spawn_SetPositionOnCircle(Emitter& emitter, od::object& json)
            {
                const glm::vec3 center = RequireFloatVec<3>(json, "center");
                const float radius = RequireFloat(json, "radius");
                const float angularSpeed = RequireFloat(json, "angularSpeed");

                if (HasField(json, "startAngle"))
                    emitter.AddSpawnModule<SetPositionOnCircle>(center, radius, angularSpeed, RequireFloat(json, "startAngle"));
                else
                    emitter.AddSpawnModule<SetPositionOnCircle>(center, radius, angularSpeed);
            }

            void Spawn_SetPositionCircularPath(Emitter& emitter, od::object& json)
            {
                const glm::vec3 baseOffset = RequireFloatVec<3>(json, "baseOffset");
                const glm::vec3 primaryAmplitude = RequireFloatVec<3>(json, "primaryAmplitude");
                const glm::vec3 secondaryAmplitude = RequireFloatVec<3>(json, "secondaryAmplitude");
                const float timeScale = RequireFloat(json, "timeScale");
                emitter.AddSpawnModule<SetPositionCircularPath>(baseOffset, primaryAmplitude, secondaryAmplitude, timeScale);
            }

            void Spawn_SetRibbonId(Emitter& emitter, od::object& json)
            {
                emitter.AddSpawnModule<SetRibbonId>(RequireUInt(json, "ribbonId"));
            }

            void Spawn_SetRibbonIdFromSpawnOrder(Emitter& emitter, od::object& json)
            {
                const uint32_t ribbonCount = RequireUInt(json, "ribbonCount");
                const uint32_t firstRibbonId = RequireUInt(json, "firstRibbonId");
                emitter.AddSpawnModule<SetRibbonIdFromSpawnOrder>(ribbonCount, firstRibbonId);
            }

            /* Update module builders */

            void Update_ApplyGravity(Emitter& emitter, od::object& json)
            {
                const auto gravity = ParseFloat4(json, "gravity");
                emitter.AddUpdateModule<ApplyGravity>(glm::vec3{ gravity.Value })
                    .BindParameter(gravity.Param);
            }

            void Update_ApplyLinearDrag(Emitter& emitter, od::object& json)
            {
                const auto drag = ParseScalar(json, "drag");
                emitter.AddUpdateModule<ApplyLinearDrag>(drag.Value).BindParameter(drag.Param);
            }

            void Update_ApplyAngularVelocity(Emitter& emitter, od::object& json)
            {
                const auto value = ParseScalar(json, "value");
                auto& module = emitter.AddUpdateModule<ApplyAngularVelocity>(value.Value);
                module.BindParameter(value.Param);

                if (HasField(json, "input"))
                    module.BindInput(ParseDynamicInput(json, "input", EDynamicInput::None));
            }

            void Update_ColorOverLife(Emitter& emitter, od::object& json)
            {
                const auto start = ParseFloat4(json, "start");
                const auto end = ParseFloat4(json, "end");
                emitter.AddUpdateModule<ColorOverLife>(start.Value, end.Value)
                    .BindParameters(start.Param, end.Param);
            }

            void Update_SizeOverLife(Emitter& emitter, od::object& json)
            {
                const auto start = ParseScalar(json, "start");
                const auto end = ParseScalar(json, "end");
                emitter.AddUpdateModule<SizeOverLife>(start.Value, end.Value)
                    .BindParameters(start.Param, end.Param);
            }

            void Update_ScaleOverLife(Emitter& emitter, od::object& json)
            {
                const auto start = ParseScalar(json, "start");
                const auto end = ParseScalar(json, "end");
                auto& module = emitter.AddUpdateModule<ScaleOverLife>(start.Value, end.Value);
                module.BindParameters(start.Param, end.Param);

                if (HasField(json, "curve"))
                {
                    const std::string curveName = RequireString(json, "curve");
                    module.BindCurve(curveName, ParseDynamicInput(json, "input", EDynamicInput::NormalizedAge));
                }
            }

            void Update_KillOutsideBounds(Emitter& emitter, od::object& json)
            {
                const glm::vec3 min = RequireFloatVec<3>(json, "min");
                const glm::vec3 max = RequireFloatVec<3>(json, "max");
                emitter.AddUpdateModule<KillOutsideBounds>(min, max);
            }

            /* Dispatch */

            using ModuleBuilder = void (EffectParser::*)(Emitter&, od::object&);

            void BuildModule(
                Emitter& emitter,
                od::object& json,
                const std::unordered_map<std::string_view, ModuleBuilder>& builders,
                std::string_view kind
            )
            {
                if (m_Failed) return;

                const std::string type = RequireString(json, "type");
                if (m_Failed) return;

                const auto it = builders.find(type);
                if (it == builders.end())
                {
                    Fail("Unsupported {} module type '{}'.", kind, type);
                    return;
                }

                (this->*it->second)(emitter, json);
            }

            void BuildSpawnModule(Emitter& emitter, od::object& json)
            {
                static const std::unordered_map<std::string_view, ModuleBuilder> builders{
                    { "SetPositionDisk", &EffectParser::Spawn_SetPositionDisk },
                    { "SetVelocityCone", &EffectParser::Spawn_SetVelocityCone },
                    { "SetLifetime", &EffectParser::Spawn_SetLifetime },
                    { "SetSize", &EffectParser::Spawn_SetSize },
                    { "SetScale", &EffectParser::Spawn_SetScale },
                    { "SetRotation", &EffectParser::Spawn_SetRotation },
                    { "SetColor", &EffectParser::Spawn_SetColor },
                    { "SetPositionOnCircle", &EffectParser::Spawn_SetPositionOnCircle },
                    { "SetPositionCircularPath", &EffectParser::Spawn_SetPositionCircularPath },
                    { "SetRibbonId", &EffectParser::Spawn_SetRibbonId },
                    { "SetRibbonIdFromSpawnOrder", &EffectParser::Spawn_SetRibbonIdFromSpawnOrder },
                };
                BuildModule(emitter, json, builders, "spawn");
            }

            void BuildUpdateModule(Emitter& emitter, od::object& json)
            {
                static const std::unordered_map<std::string_view, ModuleBuilder> builders{
                    { "ApplyGravity", &EffectParser::Update_ApplyGravity },
                    { "ApplyLinearDrag", &EffectParser::Update_ApplyLinearDrag },
                    { "ApplyAngularVelocity", &EffectParser::Update_ApplyAngularVelocity },
                    { "ColorOverLife", &EffectParser::Update_ColorOverLife },
                    { "SizeOverLife", &EffectParser::Update_SizeOverLife },
                    { "ScaleOverLife", &EffectParser::Update_ScaleOverLife },
                    { "KillOutsideBounds", &EffectParser::Update_KillOutsideBounds },
                };
                BuildModule(emitter, json, builders, "update");
            }

            void LoadModules(od::object& parent, std::string_view key, Emitter& emitter, bool spawn)
            {
                if (m_Failed) return;

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
                    if (m_Failed) return;

                    od::object module;
                    if (element.get_object().get(module))
                    {
                        Fail("'{}' entries must be objects.", key);
                        return;
                    }

                    if (spawn)
                        BuildSpawnModule(emitter, module);
                    else
                        BuildUpdateModule(emitter, module);
                }
            }

            void ParseEmitter(const Ref<System>& system, od::object& json)
            {
                if (m_Failed) return;

                const std::string name = RequireString(json, "name");
                const auto renderMode = ParseRenderMode(RequireString(json, "renderMode"));
                const uint32_t maxParticles = RequireUInt(json, "maxParticles");
                const auto spawnRate = ParseScalar(json, "spawnRate");
                if (m_Failed) return;

                auto& emitter = system->AddEmitter(name, maxParticles, spawnRate.Value);
                emitter.SetRenderMode(renderMode);

                if (!spawnRate.Param.empty())
                    emitter.SetSpawnRateParamName(spawnRate.Param);

                LoadOptionalParameters(json, emitter.GetParameters());
                LoadOptionalCurves(json, emitter.GetCurves());
                LoadModules(json, "spawnModules", emitter, true);
                LoadModules(json, "updateModules", emitter, false);
            }

            std::filesystem::path m_Filepath;
            bool m_Failed = false;
        };

        Ref<System> EffectParser::Parse(od::object& root)
        {
            const std::string name = RequireString(root, "name");
            if (m_Failed) return nullptr;

            auto system = CreateRef<System>(name);

            LoadOptionalParameters(root, system->GetParameters());
            LoadOptionalCurves(root, system->GetCurves());
            if (m_Failed) return nullptr;

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
                if (m_Failed) return nullptr;
            }

            return system;
        }
    }

    Ref<System> LoadEffectFile(const std::filesystem::path& filepath)
    {
        od::parser parser;

        auto json = simdjson::padded_string::load(filepath.string());
        if (json.error())
        {
            EE_CORE_ERROR("Failed to open effect asset '{}': {}", filepath.string(), simdjson::error_message(json.error()))
            return nullptr;
        }

        auto document = parser.iterate(json.value());
        if (document.error())
        {
            EE_CORE_ERROR("Failed to parse effect asset '{}': {}", filepath.string(), simdjson::error_message(document.error()))
            return nullptr;
        }

        od::object root;
        if (document.get_object().get(root))
        {
            EE_CORE_ERROR("Effect asset root must be an object: '{}'", filepath.string())
            return nullptr;
        }

        EffectParser effectParser{ filepath };
        return effectParser.Parse(root);
    }
}
