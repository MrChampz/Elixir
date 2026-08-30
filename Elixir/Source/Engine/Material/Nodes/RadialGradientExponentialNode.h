#pragma once

#include <algorithm>

#include <Engine/Material/MaterialGraph.h>
#include <glm/glm.hpp>

namespace Elixir::MaterialNodes
{
    /** @brief Generates an exponential radial gradient. */
    class RadialGradientExponentialNode final : public IMaterialNode
    {
    public:
        /** @brief Creates a gradient with a center, positive radius, and exponent. */
        RadialGradientExponentialNode(const glm::vec2& center, const float radius, const float exponent)
            : m_Center(center), m_Radius(radius), m_Exponent(exponent),
              m_Inputs{{ "UV", EMaterialValueType::Float2, "input.TexCoord", EMaterialValueType::Float2 }} {}

        std::string_view GetTypeName() const override { return "material.radial_gradient_exponential"; }
        const std::vector<SMaterialNodeInput>& GetInputs() const override { return m_Inputs; }
        SMaterialExpression Emit(const SMaterialEmitContext& context) const override
        {
            const auto uv = context.Widen(context.Input(0), EMaterialValueType::Float2);
            const auto center = "float2(" + std::to_string(m_Center.x) + ", " + std::to_string(m_Center.y) + ")";
            const auto radius = std::to_string(std::max(m_Radius, 0.0001f));
            const auto exponent = std::to_string(std::max(m_Exponent, 0.0001f));
            return { "pow(saturate(1.0 - length((" + uv + " - " + center + ") / " + radius + ")), " + exponent + ")", EMaterialValueType::Float };
        }

    private:
        glm::vec2 m_Center;
        float m_Radius;
        float m_Exponent;
        std::vector<SMaterialNodeInput> m_Inputs;
    };
}
