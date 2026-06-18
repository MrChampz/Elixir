#include "epch.h"
#include "Effect.h"

#include <MacTypes.h>
#include <simdjson.h>

namespace Elixir::Aether
{
    struct ScalarField
    {
        float Value = 0.0f;
        std::string ParamName;
    };

    struct Float4Field
    {
        glm::vec4 Value{};
        std::string ParamName;
    };

    void HandleError(
        const std::string& message,
        const std::filesystem::path& filepath,
        const simdjson::error_code* error = nullptr
    )
    {
        if (error)
            EE_CORE_ERROR(simdjson::error_message(*error))
        EE_CORE_FATAL("{0} [Path = {1}]", message, filepath.c_str())
    }

    glm::vec3 ParseFloat3(simdjson::ondemand::array array)
    {
        if (array.count_elements() != 3)
            EE_CORE_FATAL("Expected Float3 array.")

        glm::vec3 result;
        int i = 0;

        for (auto element : array)
            result[i++] = (float)element.get_double().value();

        return result;
    }

    glm::vec4 ParseFloat4(simdjson::ondemand::array array)
    {
        if (array.count_elements() != 4)
            EE_CORE_FATAL("Expected Float4 array.")

        glm::vec4 result;
        int i = 0;

        for (auto element : array)
            result[i++] = (float)element.get_double().value();

        return result;
    }

    ScalarField ParseScalarField(
        simdjson::ondemand::object object,
        const std::string& key,
        float fallback = 0.0f
    )
    {
        ScalarField field{};
        field.Value = fallback;

        auto value = object[key];
        if (value.error())
            return field;

        if (value.type() == simdjson::ondemand::json_type::number)
        {
            field.Value = (float)value.get_double().value();
            return field;
        }

        if (value.type() == simdjson::ondemand::json_type::string)
        {
            const auto& str = value.get_string().value();
            if (!str.empty() && str.front() == '$')
            {
                field.ParamName = str.substr(1);
                return field;
            }
        }

        EE_CORE_FATAL("Expected number or parameter reference! [Field = {0}]", key)
        return field;
    }

    Float4Field ParseFloat4Field(
        simdjson::ondemand::object object,
        const std::string& key,
        glm::vec4 fallback = glm::vec4(0.0f)
    )
    {
        Float4Field field{};
        field.Value = fallback;

        auto value = object[key];
        if (value.error())
            return field;

        if (value.type() == simdjson::ondemand::json_type::array)
        {
            field.Value = ParseFloat4(value.get_array().value());
            return field;
        }

        if (value.type() == simdjson::ondemand::json_type::string)
        {
            const auto& str = value.get_string().value();
            if (!str.empty() && str.front() == '$')
            {
                field.ParamName = str.substr(1);
                return field;
            }
        }

        EE_CORE_FATAL("Expected Float4 array or parameter reference! [Field = {0}]", key)
        return field;
    }

    EParticleRenderMode ParseRenderMode(const std::string_view& value)
    {
        if (value == "Sprite")
            return EParticleRenderMode::Sprite;

        if (value == "Ribbon")
            return EParticleRenderMode::Ribbon;

        if (value == "Mesh")
            return EParticleRenderMode::Mesh;

        EE_CORE_FATAL("Unknown render mode! [RenderMode = {0}]", value)
        return EParticleRenderMode::Sprite;
    }

    void LoadParameters(ParameterStore& store, simdjson::ondemand::object params)
    {
        for (auto field : params)
        {
            const auto name = std::string{ field.escaped_key().value() };
            auto value = field.value();

            if (value.type() == simdjson::ondemand::json_type::number)
            {
                store.SetFloat(name, (float)value.get_number().value().as_double());
            }
            else if (value.type() == simdjson::ondemand::json_type::array)
            {
                auto array = value.get_array().value();
                if (array.count_elements() == 4)
                {
                    store.SetFloat4(name, ParseFloat4(array));
                }
                else
                {
                    EE_CORE_FATAL("Only Float4 arrays are supported in parameters block!")
                }
            }
            else
            {
                EE_CORE_FATAL("Unsupported parameter value type! [Param = {0}]", name)
            }
        }
    }

    void LoadCurves(CurveStore& store, simdjson::ondemand::object curves)
    {
        for (auto curve : curves)
        {
            const auto name = std::string{ curve.escaped_key().value() };
            auto value = curve.value();

            if (value.type() != simdjson::ondemand::json_type::array)
                EE_CORE_FATAL("Curve must be an array! [Curve = {0}]", name)

            std::vector<float> samples;
            samples.reserve(value.count_elements());

            for (auto sample : value.get_array().value())
            {
                if (sample.value().type() != simdjson::ondemand::json_type::number)
                    EE_CORE_FATAL("Curve sample must be numeric! [Curve = {0}]", name)

                samples.push_back((float)sample.get_double().value());
            }

            store.SetCurve(name, std::move(samples));
        }
    }

    void BuildSpawnModule(Emitter& emitter, simdjson::ondemand::object json)
    {
        const auto type = std::string{ json["type"].get_string().value() };

        if (type == "SetPositionDisk")
        {
            glm::vec3 center = ParseFloat3(json["center"]);
            float radius = json["radius"].get_double().value();
            if (auto field = json["normal"]; field.error())
            {
                emitter.AddSpawnModule<SetPositionDisk>(center, radius);
            }
            else
            {
                const auto normal = ParseFloat3(field);
                emitter.AddSpawnModule<SetPositionDisk>(center, radius, normal);
            }
        }
        else if (type == "SetVelocityCone")
        {
            glm::vec3 direction = ParseFloat3(json["direction"]);
            const auto angle = ParseScalarField(json, "angle");
            const auto minSpeedField = ParseScalarField(json, "minSpeed");
            const auto maxSpeedField = ParseScalarField(json, "maxSpeed");
            emitter.AddSpawnModule<SetVelocityCone>(direction, angle.Value, minSpeedField.Value, maxSpeedField.Value)
                .BindParameters(angle.ParamName, minSpeedField.ParamName, maxSpeedField.ParamName);
        }
        else if (type == "SetLifetime")
        {
            const auto minField = ParseScalarField(json, "min");
            const auto maxField = ParseScalarField(json, "max");
            emitter.AddSpawnModule<SetLifetime>(minField.Value, maxField.Value)
                .BindParameters(minField.ParamName, maxField.ParamName);
        }
        else if (type == "SetSize")
        {
            const auto minField = ParseScalarField(json, "min");
            const auto maxField = ParseScalarField(json, "max");
            emitter.AddSpawnModule<SetSize>(minField.Value, maxField.Value)
                .BindParameters(minField.ParamName, maxField.ParamName);
        }
        else if (type == "SetColor")
        {
            const auto field = ParseFloat4Field(json, "color", glm::vec4(1.0f));
            const auto maxField = ParseScalarField(json, "max");
            emitter.AddSpawnModule<SetColor>(field.Value).BindParameter(field.ParamName);
        }
        else if (type == "SetRotation")
        {
            const auto minField = ParseScalarField(json, "min");
            const auto maxField = ParseScalarField(json, "max");
            emitter.AddSpawnModule<SetRotation>(minField.Value, maxField.Value)
                .BindParameters(minField.ParamName, maxField.ParamName);
        }
        else if (type == "SetScale")
        {
            const auto minField = ParseScalarField(json, "min");
            const auto maxField = ParseScalarField(json, "max");
            emitter.AddSpawnModule<SetScale>(minField.Value, maxField.Value)
                .BindParameters(minField.ParamName, maxField.ParamName);
        }
        else if (type == "SetPositionOnCircle")
        {
            glm::vec3 center = ParseFloat3(json["center"]);
            float radius = json["radius"].get_double().value();
            float angularSpeed = json["angularSpeed"].get_double().value();

            if (auto field = json["startAngle"]; field.error())
            {
                emitter.AddSpawnModule<SetPositionOnCircle>(center, radius, angularSpeed);
            }
            else
            {
                float startAngle = field.get_double().value();
                emitter.AddSpawnModule<SetPositionOnCircle>(center, radius, angularSpeed, startAngle);
            }
        }
        else if (type == "SetPositionCircularPath")
        {
            glm::vec3 baseOffset = ParseFloat3(json["baseOffset"]);
            glm::vec3 primaryAmplitude = ParseFloat3(json["primaryAmplitude"]);
            glm::vec3 secondaryAmplitude = ParseFloat3(json["secondaryAmplitude"]);
            float timeScale = json["timeScale"].get_double().value();
            emitter.AddSpawnModule<SetPositionCircularPath>(baseOffset, primaryAmplitude, secondaryAmplitude, timeScale);
        }
        else if (type == "SetRibbonId")
        {
            const auto ribbonIdField = ParseScalarField(json, "ribbonId");
            emitter.AddSpawnModule<SetRibbonId>(ribbonIdField.Value);
        }
        else if (type == "SetRibbonIdFromSpawnOrder")
        {
            const uint32_t ribbonCount = json["ribbonCount"].get_uint64().value();
            const uint32_t firstRibbonId = json["firstRibbonId"].get_uint64().value();
            emitter.AddSpawnModule<SetRibbonIdFromSpawnOrder>(ribbonCount, firstRibbonId);
        }
        else
        {
            EE_CORE_FATAL("Unsupported spawn module type! [Type = {0}]", type)
        }
    }

    void BuildUpdateModule(Emitter& emitter, simdjson::ondemand::object json)
    {
        const auto type = std::string{ json["type"].get_string().value() };

        if (type == "ApplyGravity")
        {
            const auto field = ParseFloat4Field(json, "gravity");
            emitter.AddUpdateModule<ApplyGravity>(glm::vec3{ field.Value.x, field.Value.y, field.Value.z })
                .BindParameter(field.ParamName);
        }
        else if (type == "ApplyLinearDrag")
        {
            const auto field = ParseScalarField(json, "drag");
            emitter.AddUpdateModule<ApplyLinearDrag>(field.Value)
                .BindParameter(field.ParamName);
        }
        else if (type == "ApplyAngularVelocity")
        {
            const auto field = ParseScalarField(json, "value");
            emitter.AddUpdateModule<ApplyAngularVelocity>(field.Value)
                .BindParameter(field.ParamName);
        }
        else if (type == "ColorOverLife")
        {
            const auto startField = ParseFloat4Field(json, "start");
            const auto endField = ParseFloat4Field(json, "end");
            emitter.AddUpdateModule<ColorOverLife>(startField.Value, endField.Value)
                .BindParameters(startField.ParamName, endField.ParamName);
        }
        else if (type == "SizeOverLife")
        {
            const auto startField = ParseScalarField(json, "start");
            const auto endField = ParseScalarField(json, "end");
            emitter.AddUpdateModule<SizeOverLife>(startField.Value, endField.Value)
                .BindParameters(startField.ParamName, endField.ParamName);
        }
        else if (type == "ScaleOverLife")
        {
            const auto startField = ParseScalarField(json, "start");
            const auto endField = ParseScalarField(json, "end");
            emitter.AddUpdateModule<ScaleOverLife>(startField.Value, endField.Value)
                .BindParameters(startField.ParamName, endField.ParamName);
        }
        else if (type == "KillOutsideBounds")
        {
            const auto min = ParseFloat3(json["min"]);
            const auto max = ParseFloat3(json["max"]);
            emitter.AddUpdateModule<KillOutsideBounds>(min, max);
        }
        else
        {
            EE_CORE_FATAL("Unsupported update module type! [Type = {0}]", type)
        }
    }

    void ParseEmitter(const Ref<System>& system, simdjson::ondemand::object json)
    {
        const std::string name = std::string{ json["name"].get_string().value() };
        const uint32_t maxParticles = json["maxParticles"].get_uint64().value();
        const auto spawnRate = ParseScalarField(json, "spawnRate");

        auto& emitter = system->AddEmitter(name, maxParticles, spawnRate.Value);
        emitter.SetRenderMode(ParseRenderMode(json["renderMode"].get_string().value()));

        if (!spawnRate.ParamName.empty())
            emitter.SetSpawnRateParamName(spawnRate.ParamName);

        auto parameters = json["parameters"];
        if (!parameters.error())
            LoadParameters(emitter.GetParameters(), parameters.get_object());

        auto curves = json["curves"];
        if (!curves.error())
            LoadCurves(emitter.GetCurves(), curves.get_object());

        auto spawnModules = json["spawnModules"];
        if (!spawnModules.error())
        {
            for (auto module : spawnModules.get_array())
                BuildSpawnModule(emitter, module);
        }

        auto updateModules = json["updateModules"];
        if (!updateModules.error())
        {
            for (auto module : updateModules.get_array())
                BuildUpdateModule(emitter, module);
        }
    }

    Ref<System> ParseAetherSystem(
        simdjson::ondemand::object root,
        const std::filesystem::path& filepath
    )
    {
        Ref<System> system = CreateRef<System>("name");

        auto parameters = root["parameters"];
        if (!parameters.error())
            LoadParameters(system->GetParameters(), parameters.get_object());

        auto curves = root["curves"];
        if (!curves.error())
            LoadCurves(system->GetCurves(), curves.get_object());

        auto emitters = root["emitters"];
        if (emitters.error() || emitters.value().type() != simdjson::ondemand::json_type::array)
            HandleError("Effect asset must contain an emitters array!", filepath);

        for (auto emitter : emitters.get_array())
            ParseEmitter(system, emitter);

        return system;
    }

    Ref<System> LoadEffectFile(const std::filesystem::path& filepath)
    {
        simdjson::ondemand::parser parser;

        auto json = simdjson::padded_string::load(filepath.string());
        if (const auto error = json.error())
            HandleError("Failed to open effect asset!", filepath, &error);

        auto document = parser.iterate(json.value());
        if (const auto error = document.error())
            HandleError("Failed to parse effect asset!", filepath, &error);

        if (document.value().type() != simdjson::ondemand::json_type::object)
            HandleError("Effect asset root must be an object!", filepath);

        return ParseAetherSystem(document.get_object(), filepath);
    }
}