#pragma once

#include <cmath>

#include <utility>

#include <Engine/Aether/Modules/DynamicInput.h>
#include <Engine/Aether/Modules/Module.h>
#include <Engine/Aether/Effect/ModuleParseContext.h>

namespace Elixir::Aether::Modules
{
    /** @brief Changes particle scale over its lifetime. */
    class ELIXIR_API ScaleOverLife final : public UpdateModule
    {
    public:
        /** @brief Creates the module with the supplied values. */
        ScaleOverLife(float startScale, float endScale)
            : m_StartScale(startScale), m_EndScale(endScale) {}

        /** @brief Appends this module's GPU operations to the supplied context. */
        void Compile(ModuleCompileContext& context) const override
        {
            if (!m_CurveName.empty())
            {
                context.Emit({
                    Core::EParticleOp::SampleCurve,
                    Core::EParticleAttribute::Temp0,
                    context.FindCurve(m_CurveName),
                    UINT32_MAX,
                    { (float)(uint32_t)m_CurveInput, 0.0f, 0.0f, 0.0f },
                });

                const float scaleRange = m_EndScale - m_StartScale;
                if (std::abs(scaleRange - 1.0f) > 0.0001f)
                {
                    context.Emit({
                        Core::EParticleOp::Mul,
                        Core::EParticleAttribute::Temp0,
                        UINT32_MAX,
                        UINT32_MAX,
                        { scaleRange, 0.0f, 0.0f, 0.0f },
                    });
                }

                if (std::abs(m_StartScale) > 0.0001f)
                {
                    context.Emit({
                        Core::EParticleOp::Add,
                        Core::EParticleAttribute::Temp0,
                        UINT32_MAX,
                        UINT32_MAX,
                        { m_StartScale, 0.0f, 0.0f, 0.0f },
                    });
                }

                context.Emit({
                    Core::EParticleOp::Clamp,
                    Core::EParticleAttribute::Temp0,
                    UINT32_MAX,
                    UINT32_MAX,
                    glm::vec4(0.0f),
                    glm::vec4(4.0f),
                });

                context.Emit({
                    Core::EParticleOp::CopyFromAttribute,
                    Core::EParticleAttribute::Scale,
                    UINT32_MAX,
                    UINT32_MAX,
                    { (float)(uint32_t)Core::EParticleAttribute::Temp0, 0.0f, 0.0f, 0.0f },
                });
            }
            else
            {
                context.Emit({
                    Core::EParticleOp::LerpOverLife,
                    Core::EParticleAttribute::Scale,
                    context.FindParameter(m_StartScaleParamName),
                    context.FindParameter(m_EndScaleParamName),
                    { m_StartScale, 0.0f, 0.0f, 0.0f },
                    { m_EndScale, 0.0f, 0.0f, 0.0f }
                });
            }
        }

        /** @brief Binds named parameters to the configured values and returns this module. */
        ScaleOverLife& BindParameters(std::string startScaleParam, std::string endScaleParam)
        {
            m_StartScaleParamName = std::move(startScaleParam);
            m_EndScaleParamName = std::move(endScaleParam);
            return *this;
        }

        /** @brief Binds a named curve and its sampling input and returns this module. */
        ScaleOverLife& BindCurve(std::string curveName, EDynamicInput input = EDynamicInput::NormalizedAge)
        {
            m_CurveName = std::move(curveName);
            m_CurveInput = input;
            return *this;
        }

        /** @brief Returns the configured start scale. */
        float GetStartScale() const { return m_StartScale; }

        /** @brief Returns the configured end scale. */
        float GetEndScale() const { return m_EndScale; }

        /** @brief Returns the configured start scale parameter name. */
        const std::string& GetStartScaleParamName() const { return m_StartScaleParamName; }

        /** @brief Returns the configured end scale parameter name. */
        const std::string& GetEndScaleParamName() const { return m_EndScaleParamName; }

        /** @brief Returns the configured curve name. */
        const std::string& GetCurveName() const { return m_CurveName; }

        /** @brief Returns the dynamic input used to sample the curve. */
        EDynamicInput GetCurveInput() const { return m_CurveInput; }

        /** @brief Creates this module from its serialized fields. */
        static Scope<Module> Create(Effect::ModuleParseContext& context, simdjson::ondemand::object& object)
        {
            const auto start = context.ParseScalar(object, "start");
            const auto end = context.ParseScalar(object, "end");
            auto module = CreateScope<ScaleOverLife>(start.Value, end.Value);
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
        float m_StartScale;
        float m_EndScale;
        std::string m_StartScaleParamName;
        std::string m_EndScaleParamName;
        std::string m_CurveName;
        EDynamicInput m_CurveInput = EDynamicInput::None;
    };
}
