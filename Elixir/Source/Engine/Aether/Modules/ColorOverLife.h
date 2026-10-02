#pragma once

#include <utility>

#include <Engine/Aether/Modules/DynamicInput.h>
#include <Engine/Aether/Modules/Module.h>
#include <Engine/Aether/Effect/ModuleParseContext.h>

namespace Elixir::Aether::Modules
{
    /** @brief Changes particle color over its lifetime. */
    class ELIXIR_API ColorOverLife final : public UpdateModule
    {
    public:
        /** @brief Creates the module with the supplied values. */
        explicit ColorOverLife(glm::vec4 startColor, glm::vec4 endColor)
            : m_StartColor(startColor), m_EndColor(endColor) {}

        /** @brief Appends this module's GPU operations to the supplied context. */
        void Compile(ModuleCompileContext& context) const override
        {
            if (!m_CurveName.empty())
            {
                context.Emit({
                    Core::EParticleOp::SampleColorCurve,
                    Core::EParticleAttribute::Temp0,
                    context.FindCurve(m_CurveName),
                    UINT32_MAX,
                    { (float)(uint32_t)m_CurveInput, 0.0f, 0.0f, 0.0f },
                });
                context.Emit({
                    Core::EParticleOp::CopyFromAttribute,
                    Core::EParticleAttribute::Color,
                    UINT32_MAX,
                    UINT32_MAX,
                    { (float)(uint32_t)Core::EParticleAttribute::Temp0, 0.0f, 0.0f, 0.0f },
                });
            }
            else
            {
                context.Emit({
                    Core::EParticleOp::LerpOverLife,
                    Core::EParticleAttribute::Color,
                    context.FindParameter(m_StartColorParamName),
                    context.FindParameter(m_EndColorParamName),
                    m_StartColor,
                    m_EndColor
                });
            }
        }

        /** @brief Binds named parameters to the configured values and returns this module. */
        ColorOverLife& BindParameters(std::string startColorParam, std::string endColorParam)
        {
            m_StartColorParamName = std::move(startColorParam);
            m_EndColorParamName = std::move(endColorParam);
            return *this;
        }

        /** @brief Binds a named curve and its sampling input and returns this module. */
        ColorOverLife& BindCurve(std::string name, EDynamicInput input = EDynamicInput::NormalizedAge)
        {
            m_CurveName = std::move(name);
            m_CurveInput = input;
            return *this;
        }

        /** @brief Returns the configured start color. */
        const glm::vec4& GetStartColor() const { return m_StartColor; }

        /** @brief Returns the configured end color. */
        const glm::vec4& GetEndColor() const { return m_EndColor; }

        /** @brief Returns the configured start color parameter name. */
        const std::string& GetStartColorParamName() const { return m_StartColorParamName; }

        /** @brief Returns the configured end color parameter name. */
        const std::string& GetEndColorParamName() const { return m_EndColorParamName; }

        /** @brief Returns the configured curve name. */
        const std::string& GetCurveName() const { return m_CurveName; }

        /** @brief Returns the dynamic input used to sample the curve. */
        EDynamicInput GetCurveInput() const { return m_CurveInput; }

        /** @brief Creates this module from its serialized fields. */
        static Scope<Module> Create(Effect::ModuleParseContext& context, simdjson::ondemand::object& object)
        {
            const auto start = context.ParseFloat4(object, "start");
            const auto end = context.ParseFloat4(object, "end");
            auto module = CreateScope<ColorOverLife>(start.Value, end.Value);
            module->BindParameters(start.Param, end.Param);

            if (context.HasField(object, "curve"))
            {
                const auto input = context.HasField(object, "input")
                    ? context.ParseDynamicInput(object, "input")
                    : EDynamicInput::NormalizedAge;
                module->BindCurve(context.RequireString(object, "curve"), input);
            }
            return module;
        }

    private:
        glm::vec4 m_StartColor;
        glm::vec4 m_EndColor;
        std::string m_StartColorParamName;
        std::string m_EndColorParamName;
        std::string m_CurveName;
        EDynamicInput m_CurveInput = EDynamicInput::None;
    };
}
