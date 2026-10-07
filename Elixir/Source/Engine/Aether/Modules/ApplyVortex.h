#pragma once

#include <utility>

#include <Engine/Aether/Modules/Module.h>
#include <Engine/Aether/Effect/ModuleParseContext.h>

namespace Elixir::Aether::Modules
{
    /** @brief Applies tangential and radial forces around a vortex center. */
    class ELIXIR_API ApplyVortex final : public UpdateModule
    {
    public:
        /** @brief Creates the module with the supplied values. */
        explicit ApplyVortex(
            glm::vec3 center,
            float tangentialStrength,
            float radialStrength,
            glm::vec3 normal = { 0, 1, 0 }
        ) : m_Center(center),
            m_Normal(normal),
            m_TangentialStrength(tangentialStrength),
            m_RadialStrength(radialStrength) {}

        /** @brief Appends this module's GPU operations to the supplied context. */
        void Compile(ModuleCompileContext& context) const override
        {
            context.Emit(
                EOperationType::ApplyVortex,
                EParticleAttribute::Velocity,
                {
                    context.Vector(m_Center, m_CenterParamName),
                    context.Vector(m_Normal, m_NormalParamName),
                    context.Scalar(m_TangentialStrength, m_TangentialParamName),
                    context.Scalar(m_RadialStrength, m_RadialParamName)
                }
            );
        }

        /** @brief Binds named parameters to the configured values and returns this module. */
        ApplyVortex& BindParameters(
            std::string centerParam,
            std::string tangentialParam,
            std::string radialParam,
            std::string normalParam = ""
        )
        {
            m_CenterParamName = std::move(centerParam);
            m_NormalParamName = std::move(normalParam);
            m_TangentialParamName = std::move(tangentialParam);
            m_RadialParamName = std::move(radialParam);
            return *this;
        }

        /** @brief Returns the configured center. */
        glm::vec3 GetCenter() const { return m_Center; }

        /** @brief Returns the configured normal. */
        glm::vec3 GetNormal() const { return m_Normal; }

        /** @brief Returns the configured tangential strength. */
        float GetTangentialStrength() const { return m_TangentialStrength; }

        /** @brief Returns the configured radial strength. */
        float GetRadialStrength() const { return m_RadialStrength; }

        /** @brief Returns the configured center parameter name. */
        const std::string& GetCenterParamName() const { return m_CenterParamName; }

        /** @brief Returns the configured normal parameter name. */
        const std::string& GetNormalParamName() const { return m_NormalParamName; }

        /** @brief Returns the configured tangential parameter name. */
        const std::string& GetTangentialParamName() const { return m_TangentialParamName; }

        /** @brief Returns the configured radial parameter name. */
        const std::string& GetRadialParamName() const { return m_RadialParamName; }

        /** @brief Creates this module from its serialized fields. */
        static Scope<Module> Create(Effect::ModuleParseContext& context, simdjson::ondemand::object& object)
        {
            const auto center = context.ParseFloat4(object, "center");
            const auto tangential = context.ParseScalar(object, "tangential");
            const auto radial = context.ParseScalar(object, "radial");

            if (context.HasField(object, "normal"))
            {
                const auto normal = context.ParseFloat4(object, "normal");
                auto module = CreateScope<ApplyVortex>(glm::vec3(center.Value), tangential.Value, radial.Value, normal.Value);
                module->BindParameters(
                        center.Param,
                        tangential.Param,
                        radial.Param,
                        normal.Param
                    );
                return module;
            }
            else
            {
                auto module = CreateScope<ApplyVortex>(glm::vec3(center.Value), tangential.Value, radial.Value);
                module->BindParameters(
                        center.Param,
                        tangential.Param,
                        radial.Param
                    );
                return module;
            }
        }

    private:
        glm::vec3 m_Center;
        glm::vec3 m_Normal;
        float m_TangentialStrength;
        float m_RadialStrength;

        std::string m_CenterParamName;
        std::string m_NormalParamName;
        std::string m_TangentialParamName;
        std::string m_RadialParamName;
    };
}
