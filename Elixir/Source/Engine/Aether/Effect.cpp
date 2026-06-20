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
        // short-circuit once latched so a single root cause is reported and
        // no errored value is ever dereferenced.
        class EffectParser
        {
        public:
            explicit EffectParser(std::filesystem::path filepath)
                : m_Filepath(std::move(filepath)) {}

            Ref<System> Parse(od::object& root);

            bool Failed() const { return m_Failed; }

        private:
            template <typename... Args>
            void Fail(const std::string& error, Args&&... args)
            {
                if (m_Failed) return;

                EE_CORE_ERROR(error, std::forward<Args>(args)...);
                EE_CORE_ERROR("    in effect asset: {}", m_Filepath.c_str());
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
                if (m_Failed) return 0.0f;

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

            static bool HasField(od::object& object, std::string_view key)
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
                    Fail("Expected a Float{} array.", N);
                    return result;
                }

                int i = 0;
                for (auto element : array)
                {
                    double value;

                    if (element.get_double().get(value))
                    {
                        Fail("Field array element must be numeric.");
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
                    Fail("Field '{}' must be a Float{} array.", key, N);
                    return result;
                }

                return ParseFloatVec<N>(array);
            }

            /* Parameter-aware fields */

            ScalarField ParseScalar(
                od::object& object,
                std::string_view key,
                const float fallback = 0.0f
            )
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
                    std::string_view ref;
                    if (!field.get_string().get(ref))
                    {
                        if (auto param = ResolveParamRef(ref))
                        {
                            result.Param = std::move(*param);
                            return result;
                        }
                    }
                }

                Fail("Field '{}' must be a number or a parameter reference.", key);
                return result;
            }

            Float4Field ParseFloat4(
                od::object& object,
                std::string_view key,
                const glm::vec4 fallback = glm::vec4(0.0f)
            )
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
                    std::string_view ref;
                    if (!field.get_string().get(ref))
                    {
                        if (auto param = ResolveParamRef(ref))
                        {
                            result.Param = std::move(*param);
                            return result;
                        }
                    }
                }

                Fail("Field '{}' must be a number or a parameter reference.", key);
                return result;
            }

            /* Enums */

            EParticleRenderMode ParseRenderMode(od::object object, const std::string_view key)
            {
                constexpr auto result = EParticleRenderMode::Sprite;

                if (m_Failed || !HasField(object, key))
                    return result;

                const std::string value = RequireString(object, key);
                if (m_Failed) return result;

                if (const auto mode = magic_enum::enum_cast<EParticleRenderMode>(value))
                    return *mode;

                Fail("Unknown render mode '{}'.", value);
                return result;
            }

            EDynamicInput ParseDynamicInput(od::object& object, std::string_view key)
            {
                constexpr auto result = EDynamicInput::None;

                if (m_Failed || !HasField(object, key))
                    return result;

                const std::string value = RequireString(object, key);
                if (m_Failed) return result;

                if (const auto input = magic_enum::enum_cast<EDynamicInput>(value))
                    return *input;

                Fail("Unknown dynamic input '{}'.", value);
                return result;
            }

            /* Parameter/curve blocks */

            void LoadParametersFromObject(ParameterStore& store, od::object params)
            {
                for (auto field : params)
                {
                    if (m_Failed) return;

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
                    if (m_Failed) return;

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

            void LoadParameters(od::object& parent, ParameterStore& store)
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

                LoadParametersFromObject(store, params);
            }

            void LoadCurves(od::object& parent, CurveStore& store)
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

                LoadCurvesFromObject(store, curves);
            }

            /* Dispatch */

            using ModuleBuilder = void (*)(EffectParser&, Emitter&, od::object&);

            template <typename Module>
            void AddSpawnModule_Range(
                Emitter& emitter,
                od::object& object,
                const std::string_view lowKey,
                const std::string_view highKey
            )
            {
                const auto low = ParseScalar(object, lowKey);
                const auto high = ParseScalar(object, highKey);
                emitter.AddSpawnModule<Module>(low.Value, high.Value)
                    .BindParameters(low.Param, high.Param);
            }

            template <typename Module>
            void AddUpdateModule_Range(
                Emitter& emitter,
                od::object& object,
                const std::string_view lowKey,
                const std::string_view highKey
            )
            {
                const auto low = ParseScalar(object, lowKey);
                const auto high = ParseScalar(object, highKey);
                emitter.AddUpdateModule<Module>(low.Value, high.Value)
                    .BindParameters(low.Param, high.Param);
            }

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

                it->second(*this, emitter, json);
            }

            void BuildSpawnModule(Emitter& emitter, od::object& json)
            {
                static const std::unordered_map<std::string_view, ModuleBuilder> builders =
                {
                    { "SetLifetime", [](EffectParser& p, Emitter& e, od::object& o) { p.AddSpawnModule_Range<SetLifetime>(e, o, "min", "max"); }},
                    { "SetSize", [](EffectParser& p, Emitter& e, od::object& o) { p.AddSpawnModule_Range<SetSize>(e, o, "min", "max"); }},
                    { "SetScale", [](EffectParser& p, Emitter& e, od::object& o) { p.AddSpawnModule_Range<SetScale>(e, o, "min", "max"); }},
                    { "SetRotation", [](EffectParser& p, Emitter& e, od::object& o) { p.AddSpawnModule_Range<SetRotation>(e, o, "min", "max"); }},
                    {
                        "SetColor", [](EffectParser& p, Emitter& e, od::object& o)
                        {
                            const auto color = p.ParseFloat4(o, "color", glm::vec4(1.0f));
                            e.AddSpawnModule<SetColor>(color.Value).BindParameter(color.Param);
                        }
                    },
                    {
                        "SetPositionDisk", [](EffectParser& p, Emitter& e, od::object& o)
                        {
                            const glm::vec3 center = p.RequireFloatVec<3>(o, "center");
                            const float radius = p.RequireFloat(o, "radius");
                            if (p.HasField(o, "normal"))
                                e.AddSpawnModule<SetPositionDisk>(center, radius, p.RequireFloatVec<3>(o, "normal"));
                            else
                                e.AddSpawnModule<SetPositionDisk>(center, radius);
                        }
                    },
                    {
                        "SetVelocityCone", [](EffectParser& p, Emitter& e, od::object& o)
                        {
                            const glm::vec3 dir = p.RequireFloatVec<3>(o, "direction");
                            const auto angle = p.ParseScalar(o, "angle");
                            const auto minSpeed = p.ParseScalar(o, "minSpeed");
                            const auto maxSpeed = p.ParseScalar(o, "maxSpeed");
                            e.AddSpawnModule<SetVelocityCone>(dir, angle.Value, minSpeed.Value, maxSpeed.Value)
                                .BindParameters(angle.Param, minSpeed.Param, maxSpeed.Param);
                        }
                    },
                    {
                        "SetPositionOnCircle", [](EffectParser& p, Emitter& e, od::object& o)
                        {
                            const glm::vec3 center = p.RequireFloatVec<3>(o, "center");
                            const float radius = p.RequireFloat(o, "radius");
                            const float angularSpeed = p.RequireFloat(o, "angularSpeed");
                            if (p.HasField(o, "startAngle"))
                                e.AddSpawnModule<SetPositionOnCircle>(center, radius, angularSpeed, p.RequireFloat(o, "startAngle"));
                            else
                                e.AddSpawnModule<SetPositionOnCircle>(center, radius, angularSpeed);
                        }
                    },
                    {
                        "SetPositionCircularPath", [](EffectParser& p, Emitter& e, od::object& o)
                        {
                            const glm::vec3 baseOffset = p.RequireFloatVec<3>(o, "baseOffset");
                            const glm::vec3 primary = p.RequireFloatVec<3>(o, "primaryAmplitude");
                            const glm::vec3 secondary = p.RequireFloatVec<3>(o, "secondaryAmplitude");
                            const float timeScale = p.RequireFloat(o, "timeScale");
                            e.AddSpawnModule<SetPositionCircularPath>(baseOffset, primary, secondary, timeScale);
                        }
                    },
                    {
                        "SetRibbonId", [](EffectParser& p, Emitter& e, od::object& o)
                        {
                            e.AddSpawnModule<SetRibbonId>(p.RequireUInt(o, "ribbonId"));
                        }
                    },
                    {
                        "SetRibbonIdFromSpawnOrder", [](EffectParser& p, Emitter& e, od::object& o)
                        {
                            const uint32_t count = p.RequireUInt(o, "ribbonCount");
                            const uint32_t first = p.RequireUInt(o, "firstRibbonId");
                            e.AddSpawnModule<SetRibbonIdFromSpawnOrder>(count, first);
                        }
                    },
                };

                BuildModule(emitter, json, builders, "spawn");
            }

            void BuildUpdateModule(Emitter& emitter, od::object& json)
            {
                static const std::unordered_map<std::string_view, ModuleBuilder> builders =
                {
                    { "SizeOverLife", [](EffectParser& s, Emitter& e, od::object& j) { s.AddUpdateModule_Range<SizeOverLife>(e, j, "start", "end"); } },
                    {
                        "ColorOverLife", [](EffectParser& s, Emitter& e, od::object& j)
                        {
                            const auto start = s.ParseFloat4(j, "start");
                            const auto end = s.ParseFloat4(j, "end");
                            e.AddUpdateModule<ColorOverLife>(start.Value, end.Value).BindParameters(start.Param, end.Param);
                        }
                    },
                    {
                        "ApplyGravity", [](EffectParser& s, Emitter& e, od::object& j)
                        {
                            const auto gravity = s.ParseFloat4(j, "gravity");
                            e.AddUpdateModule<ApplyGravity>(glm::vec3{ gravity.Value }).BindParameter(gravity.Param);
                        }
                    },
                    {
                        "ApplyLinearDrag", [](EffectParser& s, Emitter& e, od::object& j)
                        {
                            const auto drag = s.ParseScalar(j, "drag");
                            e.AddUpdateModule<ApplyLinearDrag>(drag.Value).BindParameter(drag.Param);
                        }
                    },
                    {
                        "ApplyAngularVelocity", [](EffectParser& s, Emitter& e, od::object& j)
                        {
                            const auto value = s.ParseScalar(j, "value");
                            auto& m = e.AddUpdateModule<ApplyAngularVelocity>(value.Value);
                            m.BindParameter(value.Param);
                            if (s.HasField(j, "input"))
                                m.BindInput(s.ParseDynamicInput(j, "input"));
                        }
                    },
                    {
                        "ScaleOverLife", [](EffectParser& s, Emitter& e, od::object& j)
                        {
                            const auto start = s.ParseScalar(j, "start");
                            const auto end = s.ParseScalar(j, "end");
                            auto& m = e.AddUpdateModule<ScaleOverLife>(start.Value, end.Value);
                            m.BindParameters(start.Param, end.Param);
                            if (s.HasField(j, "curve"))
                                m.BindCurve(s.RequireString(j, "curve"), s.ParseDynamicInput(j, "input"));
                        }
                    },
                    {
                        "KillOutsideBounds", [](EffectParser& s, Emitter& e, od::object& j)
                        {
                            const glm::vec3 min = s.RequireFloatVec<3>(j, "min");
                            const glm::vec3 max = s.RequireFloatVec<3>(j, "max");
                            e.AddUpdateModule<KillOutsideBounds>(min, max);
                        }
                    },
                };

                BuildModule(emitter, json, builders, "update");
            }

            void LoadModules(
                od::object& parent,
                std::string_view key,
                Emitter& emitter,
                const bool spawn
            )
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
                const auto renderMode = ParseRenderMode(json, "renderMode");
                const uint32_t maxParticles = RequireUInt(json, "maxParticles");
                const auto spawnRate = ParseScalar(json, "spawnRate");

                if (m_Failed) return;

                auto& emitter = system->AddEmitter(name, maxParticles, spawnRate.Value);
                emitter.SetRenderMode(renderMode);

                if (!spawnRate.Param.empty())
                    emitter.SetSpawnRateParamName(spawnRate.Param);

                LoadParameters(json, emitter.GetParameters());
                LoadCurves(json, emitter.GetCurves());
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

            LoadParameters(root, system->GetParameters());
            LoadCurves(root, system->GetCurves());
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
        if (const auto error = json.error())
        {
            const auto message = simdjson::error_message(error);
            EE_CORE_ERROR("Failed to open effect asset '{}': {}", filepath.c_str(), message)
            return nullptr;
        }

        auto document = parser.iterate(json.value());
        if (const auto error = document.error())
        {
            const auto message = simdjson::error_message(error);
            EE_CORE_ERROR("Failed to parse effect asset '{}': {}", filepath.c_str(), message)
            return nullptr;
        }

        od::object root;
        if (document.get_object().get(root))
        {
            EE_CORE_ERROR("Effect asset root must be an object: {}", filepath.c_str())
            return nullptr;
        }

        EffectParser effectParser{ filepath };
        return effectParser.Parse(root);
    }
}
